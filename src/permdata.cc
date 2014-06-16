/* Reimplmentation of FlexRecPermData internal to SEDll.
 */

#include "permdata.hh"

/* Velocity Analytics Plugin Framework */
#include <FlexRecReader.h>

#include "chromium/logging.hh"

/* Flex Record PermData identifier. */
static const uint32_t kPermDataId		= 40018;
/* Flex Record name for permission data */
static const char* kPermDataRecord		= "PermData";
/* Offsets for Flex Record fields */
static const int kFRPermission			= kFRFixedFields + 0;
/* Field names */
static const char* kPermissionField		= "Permission";


/* Fetch permission data with FlexRecord Cursor API.
 *
 * FlexRecReader::Open is an expensive call, ~250ms and allocates virtual memory pages.
 * FlexRecReader::Close is an expensive call, ~150ms.
 * FlexRecReader::Next copies and filters from FlexRecord Primitives into buffers allocated by Open.
 *
 * Returns false on error, true on success.
 */
bool
vhayu::permdata_t::GetDacsLock (
	const chromium::StringPiece& symbol_name,
	std::string *lock
	)
{
#ifndef CONFIG_AS_APPLICATION
/* Symbol names */
	std::set<std::string> symbol_set;
	symbol_set.insert (symbol_name.as_string());
/* FlexRecord fields */
	char permdata[32];
	size_t length = sizeof (permdata);
	std::set<FlexRecBinding> binding_set;
	FlexRecBinding binding (kPermDataId);
	binding.Bind (kPermissionField, permdata, sizeof (permdata));
	binding_set.insert (binding);
/* Time period */
	static const __time32_t from = 86400 * 2;  /* magic numbers */
	static const __time32_t till = INT32_MAX - 4;
/* Open cursor */
	FlexRecReader fr;
	try {
		char error_text[1024];
		const int cursor_status = fr.Open (symbol_set, binding_set, from, till, kDirectionNewToOld /* backward */, 1 /* limit */, error_text);
		if (1 != cursor_status) {
			LOG(ERROR) << "FlexRecReader::Open failed { \"code\": " << cursor_status
				<< ", \"text\": \"" << error_text << "\" }";
			return false;
		}
	} catch (const std::exception& e) {
/* typically out-of-memory exceptions due to insufficient virtual memory */
		LOG(ERROR) << "FlexRecReader::Open raised exception " << e.what();
		return false;
	}
/* pull single record */
	if (!fr.Next()) {
		LOG(ERROR) << "No recorded permission data for item \"" << symbol_name << "\".";
		goto cleanup;
	}
	asciiLockToBinary (permdata, lock);
    	fr.Close();
	return true;
cleanup:
	fr.Close();
	return false;
#else
	return false;
#endif /* CONFIG_AS_APPLICATION */
}

/* Fetch permission data with FlexRecord Primitives API.
 *
 * Returns false on error, true on success.
 */
bool
vhayu::permdata_t::GetDacsLock (
	const TBSymbolHandle& handle,
	FlexRecWorkAreaElement* work_area,
	FlexRecViewElement* view_element,
	std::string *lock
	)
{
#ifndef CONFIG_AS_APPLICATION
/* Time period */
	static const __time32_t from = 86400 * 2;  /* magic numbers */
/*   Unable to deblob.  Data get error: -1000024
 *   DataInfo: symbol:<MSFT.O>  time: 1383782467  num: 331  len: 36
 */
	static const __time32_t till = INT32_MAX - 4;

	DVLOG(4) << "from: " << from << " till: " << till;
	try {
		U64 numRecs = FlexRecPrimitives::GetFlexRecords (
							handle, 
							const_cast<char*> (kPermDataRecord),
							till, from, kDirectionNewToOld /* backward */,
							1 /* limit */,
							view_element->view,
							work_area->data,
							OnFlexRecord,
							lock /* closure */
								);
	} catch (const std::exception& e) {
		LOG(ERROR) << "FlexRecPrimitives::GetFlexRecords raised exception " << e.what();
		return false;
	}
#endif /* CONFIG_AS_APPLICATION */
	return true;
}

/* Returns <1> to continue processing, <2> to halt processing due to an error.
 */
int
vhayu::permdata_t::OnFlexRecord(
	FRTreeCallbackInfo* info
	)
{
	CHECK(nullptr != info->callersData);
	auto& lock = *reinterpret_cast<std::string*> (info->callersData);

/* extract from view */
	const char* permission = reinterpret_cast<char*> (info->theView[kFRPermission].data);

/* save as lock */
	lock.assign (permission);

/* continue processing */
	return 1;
}

// HexChar parses |c| as a hex character. If valid, it sets |*value| to the
// value of the hex character and returns true. Otherwise it returns false.
static
bool 
HexChar (char c, uint8_t* value)
{
	if (c >= '0' && c <= '9') {
		*value = c - '0';
		return true;
	}
	if (c >= 'a' && c <= 'f') {
	        *value = c - 'a' + 10;
		return true;
	}
	if (c >= 'A' && c <= 'F') {
		*value = c - 'A' + 10;
		return true;
	}
	return false;
}

void
vhayu::permdata_t::asciiLockToBinary (
	const chromium::StringPiece& ascii_lock,
	std::string* dacs_lock
	)
{
	dacs_lock->resize (ascii_lock.size() / 2);

	for (size_t i = 0; i < dacs_lock->size(); i++) {
		uint8_t v = 0;
		CHECK(HexChar(ascii_lock[i*2], &v));
		(*dacs_lock)[i] = v << 4;
		CHECK(HexChar(ascii_lock[i*2 + 1], &v));
		(*dacs_lock)[i] |= v;
	}
}


/* eof */