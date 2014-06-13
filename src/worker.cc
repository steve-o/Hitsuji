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
#include "hitsuji/Reply.hpp"
#pragma warning(pop)

#include "permdata.hh"
#include "vta_bar.hh"
#include "vta_test.hh"

static const std::string kErrorMalformedRequest = "Malformed request.";
static const std::string kErrorNotFound = "Not found in SearchEngine.";
static const std::string kErrorInternal = "Internal error.";

hitsuji::worker_t::worker_t (
	std::shared_ptr<void>& zmq_context
	)
	: zmq_context_ (zmq_context)
	, manager_ (nullptr)
{
}

hitsuji::worker_t::~worker_t()
{
}

bool
hitsuji::worker_t::Initialize (size_t id)
{
/* Set thread affinity to this thread */
	DWORD_PTR default_mask;
	DWORD_PTR system_mask;
	if (GetProcessAffinityMask (GetCurrentProcess(), &default_mask, &system_mask)) {
		DWORD_PTR current_mask = 1;
		for (size_t i = 0; i < id; ++i) {
			const DWORD_PTR old_mask = current_mask;
			current_mask <<= 1;
			while ((current_mask & default_mask) == 0) {
				current_mask <<= 1;
				if (!current_mask) {
					current_mask = 1;
				}
				if (current_mask == old_mask) {
					break;
				}
			}
		}
		if (0 == SetThreadAffinityMask (GetCurrentThread(), current_mask)) {
			LPVOID lpMsgBuf;
			const DWORD dw = GetLastError();
			FormatMessage (
				FORMAT_MESSAGE_ALLOCATE_BUFFER | 
				    FORMAT_MESSAGE_FROM_SYSTEM |
				    FORMAT_MESSAGE_IGNORE_INSERTS,
				NULL,
				dw,
				MAKELANGID (LANG_NEUTRAL, SUBLANG_DEFAULT),
				(LPTSTR)&lpMsgBuf,
				0, NULL);
			LOG(ERROR) << "Failed to set thread affinity mask: " << static_cast<const char*> (lpMsgBuf);
			LocalFree (lpMsgBuf);
		} else {
			SetThreadPriority (GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
		}
	}

	LOG(INFO) << "Worker thread: { "
		    "\"id\":" << id << ""
		  ", \"boost::thread::id\": \"" << boost::this_thread::get_id() << "\""
		  ", \"processor\": " << GetCurrentProcessorNumber() << ""
		" }";

/* Set logger ID */
	std::ostringstream ss;
	ss << boost::this_thread::get_id() << ':';
	prefix_.assign (ss.str());

	try {
		static const std::function<int(void*)> zmq_close_deleter = zmq_close;
/* Setup ZMQ sockets */
		request_sock_.reset (zmq_socket (zmq_context_.get(), ZMQ_PULL), zmq_close_deleter);
		if (!(bool)request_sock_)
			goto cleanup;
		int rc = zmq_connect (request_sock_.get(), "inproc://worker/request");
		if (-1 == rc)
			goto cleanup;
		reply_sock_.reset (zmq_socket (zmq_context_.get(), ZMQ_PUSH), zmq_close_deleter);
		if (!(bool)reply_sock_)
			goto cleanup;
		rc = zmq_connect (reply_sock_.get(), "inproc://worker/reply");
		if (-1 == rc)
			goto cleanup;
	} catch (const std::exception& e) {
		LOG(ERROR) << prefix_ << "ZeroMQ::Exception: { "
			"\"What\": \"" << e.what() << "\""
			" }";
		goto cleanup;
	}
	try {
		if (!AcquireFlexRecordCursor())
			goto cleanup;
	} catch (const std::exception& e) {
		LOG(ERROR) << prefix_ << "FlexRecord::Initialisation exception: { "
			"\"What\": \"" << e.what() << "\""
			" }";
		goto cleanup;
	}
	try {
		sbe_hdr_.reset (new hitsuji::MessageHeader());
		sbe_request_.reset (new hitsuji::Request());
		sbe_reply_.reset (new hitsuji::Reply());
		vta_bar_.reset (new vta::bar_t (prefix_));
		vta_test_.reset (new vta::test_t (prefix_));
		if (!(bool)sbe_hdr_ || !(bool)sbe_request_ || !(bool)sbe_reply_ || !(bool)vta_bar_ || !(bool)vta_test_)
			goto cleanup;
	} catch (const std::exception& e) {
		LOG(ERROR) << prefix_ << "SBE::Initialisation exception: { "
			"\"What\": \"" << e.what() << "\""
			" }";
		goto cleanup;
	}
	LOG(INFO) << prefix_ << "Initialisation complete.";
	return true;
cleanup:
	Reset();
	LOG(INFO) << prefix_ << "Initialisation failed.";
	return false;
}

bool
hitsuji::worker_t::AcquireFlexRecordCursor()
{
/* FlexRecPrimitives cursor */
	manager_ = FlexRecDefinitionManager::GetInstance (nullptr);
	work_area_.reset (manager_->AcquireWorkArea(), [this](FlexRecWorkAreaElement* work_area){ manager_->ReleaseWorkArea (work_area); });
	view_element_.reset (manager_->AcquireView(), [this](FlexRecViewElement* view_element){ manager_->ReleaseView (view_element); });
	if (!manager_->GetView ("Trade", view_element_->view)) {
		LOG(ERROR) << prefix_ << "FlexRecDefinitionManager::GetView failed.";
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
	sbe_request_->wrapForDecode (reinterpret_cast<char*> (const_cast<void*> (buffer)), sbe_hdr_->size(), sbe_hdr_->blockLength(), sbe_hdr_->version(), static_cast<int> (length));

/* abort flag */
	if (sbe_request_->flags().abort()) {
		LOG(INFO) << prefix_ << "Abort flag received.";
		return false;
	}

	const uintptr_t handle = sbe_request_->handle();
	const uint16_t rwf_version = sbe_request_->rwfVersion();
	const int32_t token = sbe_request_->token();
	const uint16_t service_id = sbe_request_->serviceId();
	const bool use_attribinfo_in_updates = sbe_request_->flags().useAttribInfoInUpdates();
	const chromium::StringPiece item_name (sbe_request_->itemName(), sbe_request_->itemNameLength());

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
		if (!provider_t::WriteRawClose (
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
		if (!provider_t::WriteRawClose (
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
		if (!provider_t::WriteRawClose (
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
/* Fetch DACS lock from PermData FlexRecord history: string is cleared. */
	{
	auto permdata = std::make_shared<vhayu::permdata_t> ();
	if (!permdata->GetDacsLock (underlying_symbol_, &dacs_lock_)) {
/* underlying API only ever returns 1. */
		NOTIMPLEMENTED();
	}
	}
/* Execute analytic */
	if (!vta_bar_->Calculate (underlying_symbol_)) {
//	if (!vta_bar_->Calculate (TBPrimitives::GetSymbolHandle (underlying_symbol_.c_str(), 1), work_area_.get(), view_element_.get())) {
		if (!provider_t::WriteRawClose (
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
	if (!vta_bar_->WriteRaw (rwf_version, token, service_id, item_name, dacs_lock_, rssl_buf_, &rssl_length_)) {
/* Extremely unlikely situation that writing the response fails but writing a close will not */
		if (!provider_t::WriteRawClose (
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
	VLOG(3) << prefix_ << boost::chrono::duration_cast<boost::chrono::milliseconds> (t1 - t0).count() << "ms @ " << item_name;
	return SendReply (handle, token);
}

bool
hitsuji::worker_t::SendReply(
	uintptr_t handle,
	int32_t token
	)
{
	static const int version = 0;
	int rc;
	rc = zmq_msg_init_size (&zmq_msg_, MessageHeader::size() + Reply::sbeBlockLength() + Reply::rsslBufferHeaderSize() + rssl_length_);
	if (-1 == rc) {
		LOG(ERROR) << prefix_ << "zmq_msg_init_size failed: " << zmq_strerror (zmq_errno());
		return false;		}
	sbe_hdr_->wrap (reinterpret_cast<char*> (zmq_msg_data (&zmq_msg_)), 0, version, static_cast<int> (zmq_msg_size (&zmq_msg_)))
		.blockLength (Reply::sbeBlockLength())
		.templateId (Reply::sbeTemplateId())
		.schemaId (Reply::sbeSchemaId())
		.version (Reply::sbeSchemaVersion());
	sbe_reply_->wrapForEncode (reinterpret_cast<char*> (zmq_msg_data (&zmq_msg_)), sbe_hdr_->size(), static_cast<int> (zmq_msg_size (&zmq_msg_)))
		.handle (handle)
		.token (token);
	sbe_reply_->putRsslBuffer (rssl_buf_, static_cast<int> (rssl_length_));
	rc = zmq_msg_send (&zmq_msg_, reply_sock_.get(), 0);
	if (-1 == rc) {
		LOG(ERROR) << prefix_ << "zmq_send failed: " << zmq_strerror (zmq_errno());
		rc = zmq_msg_close (&zmq_msg_);
		LOG_IF(ERROR, -1 == rc) << prefix_ << "zmq_msg_close failed: " << zmq_strerror (zmq_errno());
		return false;
	} else {
		return true;
	}
}

void
hitsuji::worker_t::Reset()
{
}

void
hitsuji::worker_t::MainLoop()
{
	int rc;
	LOG(INFO) << prefix_ << "Accepting requests.";
	while (true) {
		rc = zmq_msg_init (&zmq_msg_);
		if (-1 == rc) {
			LOG(ERROR) << "zmq_msg_init failed: " << zmq_strerror (zmq_errno());
			break;
		}
		rc = zmq_msg_recv (&zmq_msg_, request_sock_.get(), 0);
		if (-1 == rc) {
			LOG(ERROR) << "zmq_recv failed: " << zmq_strerror (zmq_errno());
			zmq_msg_close (&zmq_msg_);
			break;
		}
		if (!OnTask (zmq_msg_data (&zmq_msg_), zmq_msg_size (&zmq_msg_))) {
			break;
		}
		rc = zmq_msg_close (&zmq_msg_);
		if (-1 == rc) {
			LOG(ERROR) << "zmq_msg_close failed: " << zmq_strerror (zmq_errno());
			break;
		}
	}
	LOG(INFO) << prefix_ << "Muted.";
}

/* eof */