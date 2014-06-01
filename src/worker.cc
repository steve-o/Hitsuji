/* Worker thread to execute Vhayu Trade Analytics on demand.
 */

#include "worker.hh"

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

#include "chromium/logging.hh"
#include "provider.hh"

/* Outstanding defects:
 * warning C4244: '=' : conversion from 'const sbe_uint64_t' to 'int', possible loss of data
 * warning C4244: 'return' : conversion from 'sbe_uint64_t' to 'int', possible loss of data
 * warning C4146: unary minus operator applied to unsigned type, result still unsigned
 * warning C4244: 'initializing' : conversion from 'sbe_int64_t' to 'int', possible loss of data
 */
#pragma warning(push)
#pragma warning(disable: 4244 146)
#include "hitsuji/MessageHeader.hpp"
#include "hitsuji/Request.hpp"
#pragma warning(pop)

#include "vta_bar.hh"
#include "vta_test.hh"

static const std::string kErrorMalformedRequest = "Malformed request.";
static const std::string kErrorNotFound = "Not found in SearchEngine.";
static const std::string kErrorInternal = "Internal error.";

hitsuji::worker_t::worker_t (
	std::shared_ptr<provider_t> provider
	)
	: provider_ (provider)
	, manager_ (nullptr)
{
}

hitsuji::worker_t::~worker_t()
{
}

bool
hitsuji::worker_t::Initialize()
{
	LOG(INFO) << "Worker thread: { "
		  "\"id\": \"" << boost::this_thread::get_id() << "\""
		" }";

/* Set logger ID */
	std::ostringstream ss;
	ss << boost::this_thread::get_id() << ':';
	prefix_.assign (ss.str());

	try {
		if (!AcquireFlexRecordCursor())
			goto cleanup;
		sbe_hdr_.reset (new hitsuji::MessageHeader());
		sbe_msg_.reset (new hitsuji::Request());
		vta_bar_.reset (new vta::bar_t (prefix_));
		vta_test_.reset (new vta::test_t (prefix_));
		if (!(bool)sbe_hdr_ || !(bool)sbe_msg_ || !(bool)vta_bar_ || !(bool)vta_test_)
			goto cleanup;
	} catch (const std::exception& e) {
		LOG(ERROR) << "Initialisation exception: { "
			"\"What\": \"" << e.what() << "\""
			" }";
		goto cleanup;
	}
	LOG(INFO) << "Initialisation complete.";
	return true;
cleanup:
	Reset();
	LOG(INFO) << "Initialisation failed.";
	return false;
}

bool
hitsuji::worker_t::AcquireFlexRecordCursor()
{
	try {
/* FlexRecPrimitives cursor */
		manager_ = FlexRecDefinitionManager::GetInstance (nullptr);
		work_area_.reset (manager_->AcquireWorkArea(), [this](FlexRecWorkAreaElement* work_area){ manager_->ReleaseWorkArea (work_area); });
		view_element_.reset (manager_->AcquireView(), [this](FlexRecViewElement* view_element){ manager_->ReleaseView (view_element); });
		if (!manager_->GetView ("Trade", view_element_->view)) {
			LOG(ERROR) << "FlexRecDefinitionManager::GetView failed.";
			return false;
		}
	} catch (const std::exception& e) {
		LOG(ERROR) << "FlexRecord::Initialisation exception: { "
			"\"What\": \"" << e.what() << "\""
			" }";
		return false;
	}
	return true;
}

bool
hitsuji::worker_t::OnTask (
	const void* buffer,
	size_t length
	)
{
	static const int version = 0;
	sbe_hdr_->wrap (reinterpret_cast<char*> (const_cast<void*> (buffer)), 0, version, static_cast<int> (length));
	sbe_msg_->wrapForDecode (reinterpret_cast<char*> (const_cast<void*> (buffer)), sbe_hdr_->size(), sbe_hdr_->blockLength(), sbe_hdr_->version(), static_cast<int> (length));
	const uintptr_t handle = sbe_msg_->handle();
	const uint16_t rwf_version = sbe_msg_->rwfVersion();
	const int32_t token = sbe_msg_->token();
	const uint16_t service_id = sbe_msg_->serviceId();
	const bool use_attribinfo_in_updates = sbe_msg_->useAttribInfoInUpdates() == BooleanType::YES;
	const chromium::StringPiece item_name (sbe_msg_->itemName(), sbe_msg_->itemNameLength());

	using namespace boost::chrono;
	auto t0 = high_resolution_clock::now();

/* clear analytic state */
	vta_bar_->Reset();
	rssl_length_ = sizeof (rssl_buf_);
/* decompose request */
	url_parse::Parsed parsed;
	url_parse::Component file_name;
	url_.assign ("null://localhost/");
	url_.append (item_name.as_string());
	url_parse::ParseStandardURL (url_.c_str(), static_cast<int>(url_.size()), &parsed);
	if (parsed.path.is_valid())
		url_parse::ExtractFileName (url_.c_str(), parsed.path, &file_name);
	if (!file_name.is_valid()) {
//		cumulative_stats_[CLIENT_PC_ITEM_REQUEST_REJECTED]++;
//		cumulative_stats_[CLIENT_PC_ITEM_REQUEST_MALFORMED]++;
		LOG(INFO) << prefix_ << "Closing invalid request for \"" << item_name << "\"";
		if (!provider_->WriteRawClose (
				rwf_version,
				token,
				service_id,
				RSSL_DMT_MARKET_PRICE,
				item_name,
				use_attribinfo_in_updates,
				RSSL_STREAM_CLOSED, RSSL_SC_NOT_FOUND, kErrorMalformedRequest,
				rssl_buf_,
				&rssl_length_
				))
		{
			return false;
		}
		goto send_reply;
	}
/* require a NULL terminated string */
	underlying_symbol_.assign (url_.c_str() + file_name.begin, file_name.len);
#ifndef CONFIG_AS_APPLICATION
/* Check SearchEngine.exe inventory */
	if (0 == TBPrimitives::IsSymbolExists (underlying_symbol_.c_str())) {
//		cumulative_stats_[CLIENT_PC_ITEM_NOT_FOUND]++;
//		cumulative_stats_[CLIENT_PC_ITEM_REQUEST_REJECTED]++;
		LOG(INFO) << prefix_ << "Closing request for unknown item \"" << underlying_symbol_ << "\".";
		if (!provider_->WriteRawClose (
				rwf_version,
				token,
				service_id,
				RSSL_DMT_MARKET_PRICE,
				item_name,
				use_attribinfo_in_updates,
				RSSL_STREAM_CLOSED, RSSL_SC_NOT_FOUND, kErrorNotFound,
				rssl_buf_,
				&rssl_length_
				))
		{
			return false;
		}
		goto send_reply;
	}
#endif
/* Validate request, e.g. be satisifed with this SearchEngine instance */
	if (parsed.query.is_valid() && !vta_bar_->ParseRequest (url_, parsed.query)) {
		if (!provider_->WriteRawClose (
				rwf_version,
				token,
				service_id,
				RSSL_DMT_MARKET_PRICE,
				item_name,
				use_attribinfo_in_updates,
				RSSL_STREAM_CLOSED, RSSL_SC_NOT_FOUND, kErrorMalformedRequest,
				rssl_buf_,
				&rssl_length_
				))
		{
			return false;
		}
		goto send_reply;
	}	
/* Fake asynchronous operation */
	if (!vta_bar_->Calculate (underlying_symbol_.c_str())) {
//	if (!vta_bar_->Calculate (TBPrimitives::GetSymbolHandle (underlying_symbol_.c_str(), 1), work_area_.get(), view_element_.get())) {
		if (!provider_->WriteRawClose (
				rwf_version,
				token,
				service_id,
				RSSL_DMT_MARKET_PRICE,
				item_name,
				use_attribinfo_in_updates,
				RSSL_STREAM_CLOSED_RECOVER, RSSL_SC_ERROR, kErrorInternal,
				rssl_buf_,
				&rssl_length_
				))
		{
			return false;
		}
		goto send_reply;
	}
/* Response message with analytic payload */
	if (!vta_bar_->WriteRaw (rwf_version, token, service_id, item_name, rssl_buf_, &rssl_length_)) {
/* Extremely unlikely situation that writing the response fails but writing a close will not */
		if (!provider_->WriteRawClose (
				rwf_version,
				token,
				service_id,
				RSSL_DMT_MARKET_PRICE,
				item_name,
				use_attribinfo_in_updates,
				RSSL_STREAM_CLOSED_RECOVER, RSSL_SC_ERROR, kErrorInternal,
				rssl_buf_,
				&rssl_length_
				))
		{
			return false;
		}
		goto send_reply;
	}

send_reply:
	auto t1 = high_resolution_clock::now();
	VLOG(3) << boost::chrono::duration_cast<boost::chrono::milliseconds> (t1 - t0).count() << "ms @ " << item_name;
	return provider_->SendReply (reinterpret_cast<RsslChannel*> (handle), token, rssl_buf_, rssl_length_);
}

bool
hitsuji::worker_t::Start()
{
	return false;
}

void
hitsuji::worker_t::Stop()
{
}

void
hitsuji::worker_t::Reset()
{
}

void
hitsuji::worker_t::MainLoop()
{
}

/* eof */