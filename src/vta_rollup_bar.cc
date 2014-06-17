/* OHLCV implementation.
 */

#include "vta_rollup_bar.hh"

/* Velocity Analytics Plugin Framework */
#include <FlexRecReader.h>

#include "chromium/logging.hh"
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
static const uint32_t kTradeId			= 41103;
/* Flex Record name for trades */
static const char* kTradeRecord			= "TradeBarRollupFrom1Minute";
static const char* kBarWidthProperty		= "width";
/* Offsets for Flex Record fields */
static const int kFROpenPrice			= kFRFixedFields + 0;
static const int kFRClosePrice			= kFRFixedFields + 1;
static const int kFRHighPrice			= kFRFixedFields + 2;
static const int kFRLowPrice			= kFRFixedFields + 3;
static const int kFRTickVolume			= kFRFixedFields + 4;
static const int kFRTickCount			= kFRFixedFields + 5;
/* Field names */
static const char* kOpenPriceField		= "Open";
static const char* kClosePriceField		= "Close";
static const char* kHighPriceField		= "High";
static const char* kLowPriceField		= "Low";
static const char* kTickVolumeField		= "Volume";
static const char* kTickCountField		= "TickCount";

/* RIC request fields. */
static const char* kOpenParameter		= "open";
static const char* kCloseParameter		= "close";

/* Nul values for fast copying */
static const boost::accumulators::accumulator_set<double,
	boost::accumulators::features<boost::accumulators::tag::first>> kNullFirstAccumulator;
static const boost::accumulators::accumulator_set<double,
	boost::accumulators::features<boost::accumulators::tag::last>> kNullLastAccumulator;
static const boost::accumulators::accumulator_set<double,
	boost::accumulators::features<boost::accumulators::tag::max>> kNullMaxAccumulator;
static const boost::accumulators::accumulator_set<double,
	boost::accumulators::features<boost::accumulators::tag::min>> kNullMinAccumulator;
static const boost::accumulators::accumulator_set<uint64_t,
	boost::accumulators::features<boost::accumulators::tag::sum>> kNullSumAccumulator;

vta::rollup_bar_t::rollup_bar_t (
	const chromium::StringPiece& worker_name
	)
	: super (worker_name)
{
}

vta::rollup_bar_t::~rollup_bar_t()
{
}

bool
vta::rollup_bar_t::ParseRequest (
	const chromium::StringPiece& url,
	const url_parse::Component& parsed_query
	)
{
	using namespace boost::gregorian;
    	using namespace boost::posix_time;
	url_parse::Component query = parsed_query;
	url_parse::Component key_range, value_range;
/* For each key-value pair, i.e. ?a=x&b=y&c=z -> (a,x) (b,y) (c,z) */
	while (url_parse::ExtractQueryKeyValue (url.data(), &query, &key_range, &value_range))
	{
/* Lazy std::string conversion for key. */
		const chromium::StringPiece key (url.data() + key_range.begin, key_range.len);
/* Value must convert to add NULL terminator for conversion APIs. */
		value_.assign (url.data() + value_range.begin, value_range.len);
		if (key == kOpenParameter) {
			open_time_ = from_time_t (std::atol (value_.c_str()));
		} else if (key == kCloseParameter) {
			close_time_ = from_time_t (std::atol (value_.c_str()));
		}
	}
/* Validation success. */
	return true;
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
vta::rollup_bar_t::Calculate (
	const chromium::StringPiece& symbol_name
	)
{
#ifndef CONFIG_AS_APPLICATION
/* Symbol names */
	std::set<std::string> symbol_set;
	symbol_set.insert (symbol_name.as_string());
/* FlexRecord fields */
	double   open_price, close_price, high_price, low_price;
	uint64_t tick_volume;
	uint32_t tick_count;
	std::set<FlexRecBinding> binding_set;
	FlexRecBinding binding (kTradeId);
	binding.Bind (kOpenPriceField, &open_price);
	binding.Bind (kClosePriceField, &close_price);
	binding.Bind (kHighPriceField, &high_price);
	binding.Bind (kLowPriceField, &low_price);
	binding.Bind (kTickVolumeField, &tick_volume);
	binding.Bind (kTickCountField, &tick_count);
	binding_set.insert (binding);
/* Open cursor */
	FlexRecReader fr;
	try {
		char error_text[1024];
/* Time period */
		const __time32_t from = internal::to_unix_epoch (open_time());
    		const __time32_t till = internal::to_unix_epoch (close_time());
/* FlexRecord query properties */
		const unsigned width = till - from + 1;
		std::ostringstream query_props;
		query_props << kBarWidthProperty << '=' << width;
		const int cursor_status = fr.Open (
					    symbol_set,
					    binding_set,
					    width + from, width + till, 0, /* forward */
					    0 /* no limit */,
					    error_text,
					    nullptr, /* bulk_retrieval_callback */
					    nullptr, /* void */
					    query_props.str().c_str()
					    );
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
		open_price_  (open_price);
		close_price_ (close_price);
		high_price_  (high_price);
		low_price_   (low_price);
		tick_volume_ (tick_volume);
		num_moves_   (tick_count);
	}
/* Cleanup */
	fr.Close();
#endif /* CONFIG_AS_APPLICATION */
	return true;
}

/* Calculate bar data with FlexRecord Primitives API.
 *
 * Returns false on error, true on success.
 */
bool
vta::rollup_bar_t::Calculate(
	const TBSymbolHandle& handle,
	FlexRecWorkAreaElement* work_area,
	FlexRecViewElement* view_element
	)
{
#ifndef CONFIG_AS_APPLICATION
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
#endif /* CONFIG_AS_APPLICATION */
	return true;
}

/* Apply a FlexRecord to a partial bar result.
 *
 * Returns <1> to continue processing, <2> to halt processing due to an error.
 */
int
vta::rollup_bar_t::OnFlexRecord(
	FRTreeCallbackInfo* info
	)
{
	CHECK(nullptr != info->callersData);
	auto& bar = *reinterpret_cast<rollup_bar_t*> (info->callersData);

/* extract from view */
	const double   open_price  = *reinterpret_cast<double*>   (info->theView[kFROpenPrice].data);
	const double   close_price = *reinterpret_cast<double*>   (info->theView[kFRClosePrice].data);
	const double   high_price  = *reinterpret_cast<double*>   (info->theView[kFRHighPrice].data);
	const double   low_price   = *reinterpret_cast<double*>   (info->theView[kFRLowPrice].data);
	const uint64_t tick_volume = *reinterpret_cast<uint64_t*> (info->theView[kFRTickVolume].data);
	const uint32_t tick_count  = *reinterpret_cast<uint32_t*> (info->theView[kFRTickCount].data);

/* add to accumulators */
	bar.open_price_  (open_price);
	bar.close_price_ (close_price);
	bar.high_price_  (high_price);
	bar.low_price_   (low_price);
	bar.tick_volume_ (tick_volume);
	bar.num_moves_   (tick_count);

/* continue processing */
	return 1;
}

bool
vta::rollup_bar_t::WriteRaw (
	uint16_t rwf_version,
	int32_t token,
	uint16_t service_id,
	const chromium::StringPiece& item_name,
	const chromium::StringPiece& dacs_lock,
	void* data,
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
	RsslBuffer buf = { static_cast<uint32_t> (*length), static_cast<char*> (data) };
	RsslRet rc;

	DCHECK(!item_name.empty());

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
	response.msgBase.msgKey.serviceId   = service_id;
	response.msgBase.msgKey.nameType    = RDM_INSTRUMENT_NAME_TYPE_RIC;
	response.msgBase.msgKey.name.data   = const_cast<char*> (item_name.data());
	response.msgBase.msgKey.name.length = static_cast<uint32_t> (item_name.size());
	response.msgBase.msgKey.flags = RSSL_MKF_HAS_SERVICE_ID | RSSL_MKF_HAS_NAME_TYPE | RSSL_MKF_HAS_NAME;
	response.flags |= RSSL_RFMF_HAS_MSG_KEY;
/* Set the request token. */
	response.msgBase.streamId = token;

/* DACS permission data, if provided */
	if (!dacs_lock.empty()) {
		response.permData.data = const_cast<char*> (dacs_lock.data());
		response.permData.length = static_cast<uint32_t> (dacs_lock.size());
		response.flags |= RSSL_RFMF_HAS_PERM_DATA;
	}

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
	rc = rsslSetEncodeIteratorRWFVersion (&it, rwf_major_version (rwf_version), rwf_major_version (rwf_version));
	if (RSSL_RET_SUCCESS != rc) {
		LOG(ERROR) << prefix_ << "rsslSetEncodeIteratorRWFVersion: { "
			  "\"returnCode\": " << static_cast<signed> (rc) << ""
			", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
			", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
			", \"majorVersion\": " << static_cast<unsigned> (rwf_major_version (rwf_version)) << ""
			", \"minorVersion\": " << static_cast<unsigned> (rwf_minor_version (rwf_version)) << ""
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
	*length = static_cast<size_t> (buf.length);
	return true;
}

void
vta::rollup_bar_t::Reset()
{
	open_time_ = close_time_ = boost::posix_time::not_a_date_time;
	open_price_ = kNullFirstAccumulator;
	close_price_ = kNullLastAccumulator;
	high_price_ = kNullMaxAccumulator;
	low_price_ = kNullMinAccumulator;
	tick_volume_ = kNullSumAccumulator;
	num_moves_ = kNullSumAccumulator;
}

/* eof */