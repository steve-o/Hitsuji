/* A simple test implementation.
 */

#include "vta_test.hh"

#include "chromium/logging.hh"
#include "upaostream.hh"
#include "rounding.hh"

/* RDM FIDs. */
static const int kRdmProductPermissionId	= 1;
static const int kRdmPreferredDisplayTemplateId	= 1080;

static const int kRdmBackroundReferenceId	= 967;
static const int kRdmGeneralText1Id		= 1000;
static const int kRdmGeneralText2Id		= 1001;
static const int kRdmPrimaryActivity1Id		= 393;
static const int kRdmSecondActivity1Id		= 275;
static const int kRdmContributor1Id		= 831;
static const int kRdmContributorLocation1Id	= 836;
static const int kRdmContributorPage1Id		= 841;
static const int kRdmDealingCode1Id		= 826;
static const int kRdmActivityTime1Id		= 1010;
static const int kRdmActivityDate1Id		= 875;

vta::test_t::test_t (
	const std::string& prefix,
	uint16_t rwf_version,
	int32_t token, 
	uint16_t service_id,
	const std::string& name,
	const boost::posix_time::time_period& tp
	)
	: super (prefix, rwf_version, token, service_id, name, tp)
{
}

vta::test_t::~test_t()
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
vta::test_t::Calculate (
	const char* symbol_name
	)
{
/* State now represents valid data. */
	is_null_ = false;

	return true;
}

/* Calculate bar data with FlexRecord Primitives API.
 *
 * Returns false on error, true on success.
 */
bool
vta::test_t::Calculate(
	const TBSymbolHandle& handle,
	FlexRecWorkAreaElement* work_area,
	FlexRecViewElement* view_element
	)
{
	is_null_ = false;
	return true;
}

bool
vta::test_t::WriteRaw (
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
	RsslError rssl_err;
	RsslRet rc;

	DCHECK(!name().empty());

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
	response.msgBase.msgKey.name.data   = const_cast<char*> (name().c_str());
	response.msgBase.msgKey.name.length = static_cast<uint32_t> (name().size());
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
		RsslBuffer data_buffer;
		RsslReal rssl_real;

		rsslClearFieldList (&field_list);
		rsslClearFieldEntry (&field);
		rsslClearReal (&rssl_real);

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
/* PROD_PERM */
		field.fieldId  = kRdmProductPermissionId;
		field.dataType = RSSL_DT_UINT;
		const uint64_t prod_perm = 213;		/* for JPY= */
		rc = rsslEncodeFieldEntry (&it, &field, &prod_perm);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"productPermission\": " << prod_perm << ""
				" }";
			return false;
		}

/* PREF_DISP */
		field.fieldId  = kRdmPreferredDisplayTemplateId;
		field.dataType = RSSL_DT_UINT;
		const uint64_t pref_disp = 6205;	/* for JPY= */
		rc = rsslEncodeFieldEntry (&it, &field, &pref_disp);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"preferredDisplayTemplate\": " << pref_disp << ""
				" }";
			return false;
		}

/* BKGD_REF */
		field.fieldId  = kRdmBackroundReferenceId;
		field.dataType = RSSL_DT_ASCII_STRING;
		const std::string bkgd_ref ("Japanese Yen");
		data_buffer.data   = const_cast<char*> (bkgd_ref.c_str());
		data_buffer.length = static_cast<uint32_t> (bkgd_ref.size());
		rc = rsslEncodeFieldEntry (&it, &field, &data_buffer);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"backgroundReference\": \"" << bkgd_ref << "\""
				" }";
			return false;
		}

/* GV1_TEXT */
		field.fieldId  = kRdmGeneralText1Id;
		field.dataType = RSSL_DT_RMTES_STRING;
		const std::string gv1_text ("SPOT");
		data_buffer.data   = const_cast<char*> (gv1_text.c_str());
		data_buffer.length = static_cast<uint32_t> (gv1_text.size());
		rc = rsslEncodeFieldEntry (&it, &field, &data_buffer);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"generalText1\": \"" << gv1_text << "\""
				" }";
			return false;
		}

/* GV2_TEXT */
		field.fieldId  = kRdmGeneralText2Id;
		field.dataType = RSSL_DT_RMTES_STRING;
		const std::string gv2_text ("USDJPY");
		data_buffer.data   = const_cast<char*> (gv2_text.c_str());
		data_buffer.length = static_cast<uint32_t> (gv2_text.size());
		rc = rsslEncodeFieldEntry (&it, &field, &data_buffer);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"generalText2\": \"" << gv2_text << "\""
				" }";
			return false;
		}

/* PRIMACT_1 */
		field.fieldId  = kRdmPrimaryActivity1Id;
		field.dataType = RSSL_DT_REAL;
		const double bid = 82.20;
		rssl_real.value = rounding::mantissa (bid);
		rssl_real.hint  = RSSL_RH_EXPONENT_2;
		rc = rsslEncodeFieldEntry (&it, &field, &rssl_real);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"primaryActivity1\": { "
					  "\"isBlank\": " << (rssl_real.isBlank ? "true" : "false") << ""
					", \"value\": " << rssl_real.value << ""
					", \"hint\": \"" << internal::real_hint_string (static_cast<RsslRealHints> (rssl_real.hint)) << "\""
				" }"
				" }";
			return false;
		}

/* SEC_ACT_1 */
		field.fieldId  = kRdmSecondActivity1Id;
		field.dataType = RSSL_DT_REAL;
		const double ask = 82.22;
		rssl_real.value = rounding::mantissa (ask);
		rssl_real.hint  = RSSL_RH_EXPONENT_2;
		rc = rsslEncodeFieldEntry (&it, &field, &rssl_real);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"secondActivity1\": { "
					  "\"isBlank\": " << (rssl_real.isBlank ? "true" : "false") << ""
					", \"value\": " << rssl_real.value << ""
					", \"hint\": \"" << internal::real_hint_string (static_cast<RsslRealHints> (rssl_real.hint)) << "\""
				" }"
				" }";
			return false;
		}

/* CTBTR_1 */
		field.fieldId  = kRdmContributor1Id;
		field.dataType = RSSL_DT_RMTES_STRING;
		const std::string ctbtr_1 ("RBS");
		data_buffer.data   = const_cast<char*> (ctbtr_1.c_str());
		data_buffer.length = static_cast<uint32_t> (ctbtr_1.size());
		rc = rsslEncodeFieldEntry (&it, &field, &data_buffer);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"contributor1\": \"" << ctbtr_1 << "\""
				" }";
			return false;
		}

/* CTB_LOC1 */
		field.fieldId  = kRdmContributorLocation1Id;
		field.dataType = RSSL_DT_RMTES_STRING;
		const std::string ctb_loc1 ("XST");
		data_buffer.data   = const_cast<char*> (ctb_loc1.c_str());
		data_buffer.length = static_cast<uint32_t> (ctb_loc1.size());
		rc = rsslEncodeFieldEntry (&it, &field, &data_buffer);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"contributorLocation1\": \"" << ctb_loc1 << "\""
				" }";
			return false;
		}

/* CTB_PAGE1 */
		field.fieldId  = kRdmContributorPage1Id;
		field.dataType = RSSL_DT_RMTES_STRING;
		const std::string ctb_page1 ("1RBS");
		data_buffer.data   = const_cast<char*> (ctb_page1.c_str()); 
		data_buffer.length = static_cast<uint32_t> (ctb_page1.size());
		rc = rsslEncodeFieldEntry (&it, &field, &data_buffer);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"contributorPage1\": \"" << ctb_page1 << "\""
				" }";
			return false;
		}

/* DLG_CODE1 */
		field.fieldId  = kRdmDealingCode1Id;
		field.dataType = RSSL_DT_RMTES_STRING;
		const std::string dlg_code1 ("RBSN");
		data_buffer.data   = const_cast<char*> (dlg_code1.c_str());
		data_buffer.length = static_cast<uint32_t> (dlg_code1.size());
		rc = rsslEncodeFieldEntry (&it, &field, &data_buffer);
		if (RSSL_RET_SUCCESS != rc) {
			LOG(ERROR) << prefix_ << "rsslEncodeFieldEntry: { "
				  "\"returnCode\": " << static_cast<signed> (rc) << ""
				", \"enumeration\": \"" << rsslRetCodeToString (rc) << "\""
				", \"text\": \"" << rsslRetCodeInfo (rc) << "\""
				", \"fieldId\": " << field.fieldId << ""
				", \"dataType\": \"" << rsslDataTypeToString (field.dataType) << "\""
				", \"dealingCode1\": \"" << dlg_code1 << "\""
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
vta::test_t::Reset()
{
}

/* eof */