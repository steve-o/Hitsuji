/* OHLCV implementation.
 */

#include "vta_bar.hh"

/* Velocity Analytics Plugin Framework */
#include <FlexRecReader.h>

#include "chromium/logging.hh"
#include "chromium/string_piece.hh"
#include "googleurl/url_parse.h"
#include "upaostream.hh"
#include "unix_epoch.hh"
#include "rounding.hh"

/* RDM FIDs. */
static const int kRdmTimeOfUpdateId		= 5;
static const int kRdmTodaysHighId		= 12;
static const int kRdmTodaysLowId		= 13;
static const int kRdmActiveDateId		= 17;
static const int kRdmOpeningPriceId 		= 19;
static const int kRdmHistoricCloseId		= 21;
static const int kRdmAccumulatedVolumeId	= 32;
static const int kRdmNumberTradesId		= 77;

/* Flex Record Trade identifier. */
static const uint32_t kTradeId			= 40001;
/* Flex Record name for trades */
static const char* kTradeRecord			= "Trade";
/* Offsets for Flex Record fields */
static const int kFRLastPrice			= kFRFixedFields + 0;
static const int kFRTickVolume			= kFRFixedFields + 19;
/* Field names */
static const char* kLastPriceField		= "LastPrice";
static const char* kTickVolumeField		= "TickVolume";

/* RIC request fields. */
static const char* kOpenParameter		= "open";
static const char* kCloseParameter		= "close";

/* Nul values for fast copying */
static const boost::accumulators::accumulator_set<double,
	boost::accumulators::features<boost::accumulators::tag::first,
					boost::accumulators::tag::last,
					boost::accumulators::tag::max,
					boost::accumulators::tag::min,
					boost::accumulators::tag::count>> kNullLastPrice;
static const boost::accumulators::accumulator_set<uint64_t,
	boost::accumulators::features<boost::accumulators::tag::sum>> kNullTickVolume;


vta::bar_t::bar_t (
	const std::string& prefix,
	uint16_t rwf_version,
	int32_t token, 
	uint16_t service_id,
	const std::string& item_name
	)
	: super (prefix, rwf_version, token, service_id, item_name)
{
	url_parse::Parsed parsed;
	std::string url, value;
	std::istringstream iss;
/* decompose request */
	url.assign ("null://localhost/");
	url.append (item_name);
	url_parse::ParseStandardURL (url.c_str(), static_cast<int>(url.size()), &parsed);
	DCHECK(parsed.path.is_valid());
	using namespace boost::gregorian;
    	using namespace boost::posix_time;
	if (parsed.query.is_valid()) {
		url_parse::Component query = parsed.query;
		url_parse::Component key_range, value_range;
		ptime t;
/* For each key-value pair, i.e. ?a=x&b=y&c=z -> (a,x) (b,y) (c,z) */
		while (url_parse::ExtractQueryKeyValue (url.c_str(), &query, &key_range, &value_range))
		{
/* Lazy std::string conversion for key. */
			const chromium::StringPiece key (url.c_str() + key_range.begin, key_range.len);
/* Value must convert to add NULL terminator for conversion APIs. */
			value.assign (url.c_str() + value_range.begin, value_range.len);
			if (key == kOpenParameter) {
/* Disabling exceptions in boost::posix_time::time_duration requires stringstream which requires a string to initialise. */
				iss.str (value);
				if (iss >> t) open_time_ = t;
			} else if (key == kCloseParameter) {
				iss.str (value);
				if (iss >> t) close_time_ = t;
			}
		}
	}
}

vta::bar_t::~bar_t()
{
}

/* Calculate bar data with FlexRecord Cursor API.
 *
 * FlexRecReader::Open is an expensive call, ~250ms and allocates virtual memory pages.
 * FlexRecReader::Close is an expensive call, ~150ms.
 * FlexRecReader::Next copies and filters from FlexRecord Primitives into buffers allocated by Open.
 *
 * Returns false on error, true on success.
 */
bool
vta::bar_t::Calculate (
	const char* symbol_name
	)
{
/* Symbol names */
	std::set<std::string> symbol_set;
	symbol_set.insert (symbol_name);
/* FlexRecord fields */
	double   last_price;
	uint64_t tick_volume;
	std::set<FlexRecBinding> binding_set;
	FlexRecBinding binding (kTradeId);
	binding.Bind (kLastPriceField, &last_price);
	binding.Bind (kTickVolumeField, &tick_volume);
	binding_set.insert (binding);
/* Time period */
	const __time32_t from = internal::to_unix_epoch (open_time());
	const __time32_t till = internal::to_unix_epoch (close_time());
/* Open cursor */
	FlexRecReader fr;
	try {
		char error_text[1024];
		const int cursor_status = fr.Open (symbol_set, binding_set, from, till, 0 /* forward */, 0 /* no limit */, error_text);
		if (1 != cursor_status) {
			LOG(ERROR) << prefix_ << "FlexRecReader::Open failed { \"code\": " << cursor_status
				<< ", \"text\": \"" << error_text << "\" }";
			return false;
		}
	} catch (const std::exception& e) {
/* typically out-of-memory exceptions due to insufficient virtual memory */
		LOG(ERROR) << prefix_ << "FlexRecReader::Open raised exception " << e.what();
		return false;
	}
/* iterate through all ticks */
	while (fr.Next()) {
		last_price_ (last_price);
		tick_volume_ (tick_volume);
	}
/* Cleanup */
	fr.Close();
/* State now represents valid data. */
	set();
	return true;
}

/* Calculate bar data with FlexRecord Primitives API.
 *
 * Returns false on error, true on success.
 */
bool
vta::bar_t::Calculate(
	const TBSymbolHandle& handle,
	FlexRecWorkAreaElement* work_area,
	FlexRecViewElement* view_element
	)
{
/* Time period */
	const __time32_t from = internal::to_unix_epoch (open_time());
	const __time32_t till = internal::to_unix_epoch (close_time());

	DVLOG(4) << prefix_ << "from: " << from << " till: " << till;
	try {
		U64 numRecs = FlexRecPrimitives::GetFlexRecords (
							handle, 
							const_cast<char*> (kTradeRecord),
							from, till, 0 /* forward */,
							0 /* no limit */,
							view_element->view,
							work_area->data,
							OnFlexRecord,
							this /* closure */
								);
	} catch (const std::exception& e) {
		LOG(ERROR) << prefix_ << "FlexRecPrimitives::GetFlexRecords raised exception " << e.what();
		return false;
	}
/* State now represents valid data. */
	set();
	return true;
}

bool
vta::bar_t::WriteRaw (
	char* data,
	size_t* length
	)
{
/* 7.4.8.1 Create a response message (4.2.2) */
	RsslRefreshMsg response = RSSL_INIT_REFRESH_MSG;
#ifndef NDEBUG
	RsslEncodeIterator it = RSSL_INIT_ENCODE_ITERATOR;
#else
	RsslEncodeIterator it;
	rsslClearEncodeIterator (&it);
#endif
	RsslBuffer buf = { static_cast<uint32_t> (*length), data };
	RsslRet rc;

	DCHECK(!item_name().empty());

/* 7.4.8.3 Set the message model type of the response. */
	response.msgBase.domainType = RSSL_DMT_MARKET_PRICE;
/* 7.4.8.4 Set response type, response type number, and indication mask. */
	response.msgBase.msgClass = RSSL_MC_REFRESH;
/* for snapshot images do not cache */
	response.flags = RSSL_RFMF_SOLICITED        |
			 RSSL_RFMF_REFRESH_COMPLETE |
			 RSSL_RFMF_DO_NOT_CACHE;
/* RDM field list. */
	response.msgBase.containerType = RSSL_DT_FIELD_LIST;

/* 7.4.8.2 Create or re-use a request attribute object (4.2.4) */
	response.msgBase.msgKey.serviceId   = service_id();
	response.msgBase.msgKey.nameType    = RDM_INSTRUMENT_NAME_TYPE_RIC;
	response.msgBase.msgKey.name.data   = const_cast<char*> (item_name().c_str());
	response.msgBase.msgKey.name.length = static_cast<uint32_t> (item_name().size());
	response.msgBase.msgKey.flags = RSSL_MKF_HAS_SERVICE_ID | RSSL_MKF_HAS_NAME_TYPE | RSSL_MKF_HAS_NAME;
	response.flags |= RSSL_RFMF_HAS_MSG_KEY;
/* Set the request token. */
	response.msgBase.streamId = token();

/** Optional: but require to replace stale values in cache when stale values are supported. **/
/* Item interaction state: Open, Closed, ClosedRecover, Redirected, NonStreaming, or Unspecified. */
	response.state.streamState = RSSL_STREAM_NON_STREAMING;
/* Data quality state: Ok, Suspect, or Unspecified. */
	response.state.dataState = RSSL_DATA_OK;
/* Error code, e.g. NotFound, InvalidArgument, ... */
	response.state.code = RSSL_SC_NONE;

	rc = rsslSetEncodeIteratorBuffer (&it, &buf);
	if (RSSL_RET_SUCCESS != rc) {
		LOG(ERROR) << prefix_ << "rsslSetEncodeIteratorBuffer: { "
			  "\"returnCode\": " << static_cast<signed> (rc) << ""
			", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
			", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
			" }";
		return false;
	}
	rc = rsslSetEncodeIteratorRWFVersion (&it, rwf_major_version(), rwf_major_version());
	if (RSSL_RET_SUCCESS != rc) {
		LOG(ERROR) << prefix_ << "rsslSetEncodeIteratorRWFVersion: { "
			  "\"returnCode\": " << static_cast<signed> (rc) << ""
			", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
			", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
			", \"majorVersion\": " << static_cast<unsigned> (rwf_major_version()) << ""
			", \"minorVersion\": " << static_cast<unsigned> (rwf_minor_version()) << ""
			" }";
		return false;
	}
	rc = rsslEncodeMsgInit (&it, reinterpret_cast<RsslMsg*> (&response), /* maximum size */ 0);
	if (RSSL_RET_ENCODE_CONTAINER != rc) {
		LOG(ERROR) << prefix_ << "rsslEncodeMsgInit: { "
			  "\"returnCode\": " << static_cast<signed> (rc) << ""
			", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
			", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
			" }";
		return false;
	}
	{
/* 4.3.1 RespMsg.Payload */
/* Clear required for SingleWriteIterator state machine. */
		RsslFieldList field_list;
		RsslFieldEntry field;
		RsslReal rssl_real;

		rsslClearFieldList (&field_list);
		rsslClearFieldEntry (&field);		

		field_list.flags = RSSL_FLF_HAS_STANDARD_DATA;
		rc = rsslEncodeFieldListInit (&it, &field_list, 0 /* summary data */, 0 /* payload */);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldListInit: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"flags\": \"RSSL_FLF_HAS_STANDARD_DATA\""
				" }";
			return false;
		}

/* For each field set the Id via a FieldEntry bound to the iterator followed by setting the data.
 * The iterator API provides setters for common types excluding 32-bit floats, with fallback to 
 * a generic DataBuffer API for other types or support of pre-calculated values.
 */
/* HIGH_1 */
		field.fieldId  = kRdmTodaysHighId;
		field.dataType = RSSL_DT_REAL;
		if (0 == number_trades()) {
			rsslBlankReal (&rssl_real);
		} else {
			const double high_price = this->high_price();
			rsslClearReal (&rssl_real);
			rssl_real.value = rounding::mantissa (high_price);
			rssl_real.hint  = rounding::hint();
		}
		rc = rsslEncodeFieldEntry (&it, &field, &rssl_real);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"HIGH_1\": { "
					  "\"isBlank\": " << (rssl_real.isBlank ? "true" : "false") << ""
					", \"value\": " << rssl_real.value << ""
					", \"hint\": \"" << internal::real_hint_string (static_cast<RsslRealHints> (rssl_real.hint)) << "\""
				" }"
				" }";
			return false;
		}
/* LOW_1 */
		field.fieldId  = kRdmTodaysLowId;
		field.dataType = RSSL_DT_REAL;
		if (0 == number_trades()) {
			rsslBlankReal (&rssl_real);
		} else {
        		const double low_price = this->low_price();
			rsslClearReal (&rssl_real);
			rssl_real.value = rounding::mantissa (low_price);
			rssl_real.hint  = rounding::hint();
		}
		rc = rsslEncodeFieldEntry (&it, &field, &rssl_real);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"LOW_1\": { "
					  "\"isBlank\": " << (rssl_real.isBlank ? "true" : "false") << ""
					", \"value\": " << rssl_real.value << ""
					", \"hint\": \"" << internal::real_hint_string (static_cast<RsslRealHints> (rssl_real.hint)) << "\""
				" }"
				" }";
			return false;
		}
/* OPEN_PRC */
		field.fieldId  = kRdmOpeningPriceId;
		field.dataType = RSSL_DT_REAL;
		if (0 == number_trades()) {
			rsslBlankReal (&rssl_real);
		} else {
			const double open_price = this->open_price();
			rsslClearReal (&rssl_real);
			rssl_real.value = rounding::mantissa (open_price);
			rssl_real.hint  = rounding::hint();
		}
		rc = rsslEncodeFieldEntry (&it, &field, &rssl_real);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"OPEN_PRC\": { "
					  "\"isBlank\": " << (rssl_real.isBlank ? "true" : "false") << ""
					", \"value\": " << rssl_real.value << ""
					", \"hint\": \"" << internal::real_hint_string (static_cast<RsslRealHints> (rssl_real.hint)) << "\""
				" }"
				" }";
			return false;
		}
/* HST_CLOSE */
		field.fieldId  = kRdmHistoricCloseId;
		field.dataType = RSSL_DT_REAL;
		if (0 == number_trades()) {
			rsslBlankReal (&rssl_real);
		} else {
			const double close_price = this->close_price();
			rsslClearReal (&rssl_real);
			rssl_real.value = rounding::mantissa (close_price);
			rssl_real.hint  = rounding::hint();
		}
		rc = rsslEncodeFieldEntry (&it, &field, &rssl_real);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"HST_CLOSE\": { "
					  "\"isBlank\": " << (rssl_real.isBlank ? "true" : "false") << ""
					", \"value\": " << rssl_real.value << ""
					", \"hint\": \"" << internal::real_hint_string (static_cast<RsslRealHints> (rssl_real.hint)) << "\""
				" }"
				" }";
			return false;
		}
/* ACVOL_1 */
		field.fieldId  = kRdmAccumulatedVolumeId;
		field.dataType = RSSL_DT_REAL;
		const uint64_t accumulated_volume = this->accumulated_volume();
/* WARNING: overflow at source not managed. */
		if (accumulated_volume <= 0xFFFFFFFFFFFFFF) {	    /* max(RWF_LEN) == 7 bytes */
			rsslClearReal (&rssl_real);
			rssl_real.value = accumulated_volume;
			rssl_real.hint  = RSSL_RH_EXPONENT0;
		} else {    /* > 72,057,594,037,927,935 (17+ digits) */
			const RsslDouble rssl_double = static_cast<RsslDouble> (accumulated_volume); /* 15 significant figures */
			rsslDoubleToReal (&rssl_real, const_cast<RsslDouble*> (&rssl_double), RSSL_RH_EXPONENT7);
		}   /* 24+ digits (78bits+) will still cause overflow and RSSL_DT_DOUBLE must be used. */
		rc = rsslEncodeFieldEntry (&it, &field, &rssl_real);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"ACVOL_1\": { "
					  "\"isBlank\": " << (rssl_real.isBlank ? "true" : "false") << ""
					", \"value\": " << rssl_real.value << ""
					", \"hint\": \"" << internal::real_hint_string (static_cast<RsslRealHints> (rssl_real.hint)) << "\""
				" }"
				" }";
			return false;
		}
/* NUM_MOVES */
		field.fieldId  = kRdmNumberTradesId;
		field.dataType = RSSL_DT_REAL;
		const uint64_t number_trades = this->number_trades();
/* WARNING: overflow not managed. */
		rsslClearReal (&rssl_real);
		rssl_real.value = number_trades;
		rssl_real.hint  = RSSL_RH_EXPONENT0;
		rc = rsslEncodeFieldEntry (&it, &field, &rssl_real);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"NUM_MOVES\": { "
					  "\"isBlank\": " << (rssl_real.isBlank ? "true" : "false") << ""
					", \"value\": " << rssl_real.value << ""
					", \"hint\": \"" << internal::real_hint_string (static_cast<RsslRealHints> (rssl_real.hint)) << "\""
				" }"
				" }";
			return false;
		}

		rc = rsslEncodeFieldListComplete (&it, RSSL_TRUE /* commit */);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldListComplete: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				" }";
			return false;
		}
	}
/* finalize multi-step encoder */
	rc = rsslEncodeMsgComplete (&it, RSSL_TRUE /* commit */);
	if (RSSL_RET_SUCCESS != rc) {
		LOG(ERROR) << prefix_ << "rsslEncodeMsgComplete: { "
			  "\"returnCode\": " << static_cast<signed> (rc) << ""
			", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
			", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
			" }";
		return false;
	}
	buf.length = rsslGetEncodedBufferLength (&it);
	LOG_IF(WARNING, 0 == buf.length) << prefix_ << "rsslGetEncodedBufferLength returned 0.";

	if (DCHECK_IS_ON()) {
/* Message validation: must use ASSERT libraries for error description :/ */
		if (!rsslValidateMsg (reinterpret_cast<RsslMsg*> (&response))) {
			LOG(ERROR) << prefix_ << "rsslValidateMsg failed.";
			return false;
		} else {
			LOG(INFO) << prefix_ << "rsslValidateMsg succeeded.";
		}
	}
	return true;
}

void
vta::bar_t::Reset()
{
	last_price_ = kNullLastPrice;
	tick_volume_ = kNullTickVolume;
	clear();
}

/* Apply a FlexRecord to a partial bar result.
 *
 * Returns <1> to continue processing, <2> to halt processing due to an error.
 */
int
vta::bar_t::OnFlexRecord(
	FRTreeCallbackInfo* info
	)
{
	CHECK(nullptr != info->callersData);
	auto& bar = *reinterpret_cast<bar_t*> (info->callersData);

/* extract from view */
	const double   last_price  = *reinterpret_cast<double*>   (info->theView[kFRLastPrice].data);
	const uint64_t tick_volume = *reinterpret_cast<uint64_t*> (info->theView[kFRTickVolume].data);

/* add to accumulators */
	bar.last_price_  (last_price);
	bar.tick_volume_ (tick_volume);

/* continue processing */
	return 1;
}


/* eof */