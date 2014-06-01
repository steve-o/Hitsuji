/* UPA interactive fake snapshot provider.  Fake snapshots are convenient for
 * consumption in TR products such as Eikon, DataView, RsslSinkApp.
 */

#include "hitsuji.hh"

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

#include <windows.h>

/* ZeroMQ messaging middleware. */
#include <zmq.h>

#include "chromium/logging.hh"
#include "provider.hh"
#include "upa.hh"
#include "version.hh"
#include "worker.hh"

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

/* Global weak pointer to shutdown as application */
static std::weak_ptr<hitsuji::hitsuji_t> g_application;

/* Global instance counter for generation of unique identifiers */
boost::atomic_uint hitsuji::hitsuji_t::instance_count_ (0);
/* List of all instances for external enumeration, e.g. SNMP walk. */
std::list<hitsuji::hitsuji_t*> hitsuji::hitsuji_t::global_list_;
boost::shared_mutex hitsuji::hitsuji_t::global_list_lock_;

hitsuji::hitsuji_t::hitsuji_t()
	: mainloop_shutdown_ (false)
	, shutting_down_ (false)
/* Unique instance number, never decremented. */
	, instance_ (instance_count_.fetch_add (1, boost::memory_order_relaxed))
	, sbe_hdr_ (new hitsuji::MessageHeader())
	, sbe_msg_ (new hitsuji::Request())
{
}

hitsuji::hitsuji_t::~hitsuji_t()
{
}

#ifndef CONFIG_AS_APPLICATION
/* Plugin entry point from the Velocity Analytics Engine.
 */
void
hitsuji::hitsuji_t::init (
	const vpf::UserPluginConfig& vpf_config
	)
{
/* Thunk to VA user-plugin base class. */
	vpf::AbstractUserPlugin::init (vpf_config);
/* Save copies of provided identifiers. */
	plugin_id_.assign (vpf_config.getPluginId());
	plugin_type_.assign (vpf_config.getPluginType());
	LOG(INFO) << "Starting SearchEngine plugin: { "
		  "\"pluginType\": \"" << plugin_type_ << "\""
		", \"pluginId\": \"" << plugin_id_ << "\""
		" }";
	if (!Start()) {
		throw vpf::UserPluginException ("Hitsuji plugin start failed.");
	}
}

/* Free all resources whilst logging device is attached. */
void
hitsuji::hitsuji_t::destroy ()
{
	LOG(INFO) << "Shutting down SearchEngine plugin: { "
		  "\"pluginType\": \"" << plugin_type_ << "\""
		", \"pluginId\": \"" << plugin_id_ << "\""
		" }";
	Stop();
/* Thunk to VA user-plugin base class. */
	vpf::AbstractUserPlugin::destroy();
}

/* Tcl API call */
int
hitsuji::hitsuji_t::execute (
	const vpf::CommandInfo& cmdInfo,
	vpf::TCLCommandData& cmdData
	)
{
	return TCL_ERROR;
}
#else /* CONFIG_AS_APPLICATION */
/* On a shutdown event set a global flag and force the event queue
 * to catch the event by submitting a log event.
 */
static
BOOL
CtrlHandler (
	DWORD	fdwCtrlType
	)
{
	const char* message;
	switch (fdwCtrlType) {
	case CTRL_C_EVENT:
		message = "Caught ctrl-c event";
		break;
	case CTRL_CLOSE_EVENT:
		message = "Caught close event";
		break;
	case CTRL_BREAK_EVENT:
		message = "Caught ctrl-break event";
		break;
	case CTRL_LOGOFF_EVENT:
		message = "Caught logoff event";
		break;
	case CTRL_SHUTDOWN_EVENT:
	default:
		message = "Caught shutdown event";
		break;
	}
	if (!g_application.expired()) {
		LOG(INFO) << message << "; shutting down application.";
		auto sp = g_application.lock();
		sp->Quit();
	} else {
		LOG(WARNING) << message << "; application already expired.";
	}
	return TRUE;
}

int
hitsuji::hitsuji_t::Run()
{
	int rc = EXIT_SUCCESS;
	VLOG(1) << "Run as application starting.";
/* Add shutdown handler. */
	g_application = shared_from_this();
	::SetConsoleCtrlHandler ((PHANDLER_ROUTINE)::CtrlHandler, TRUE);
	if (Start()) {
/* Wait for mainloop to quit */
		boost::unique_lock<boost::mutex> lock (mainloop_lock_);
		while (!mainloop_shutdown_)
			mainloop_cond_.wait (lock);
		Reset();
	} else {
		rc = EXIT_FAILURE;
	}
/* Remove shutdown handler. */
	::SetConsoleCtrlHandler ((PHANDLER_ROUTINE)::CtrlHandler, FALSE);
	VLOG(1) << "Run as application finished.";
	return rc;
}

void
hitsuji::hitsuji_t::Quit()
{
	shutting_down_ = true;
	if ((bool)provider_) {
		provider_->Quit();
	}
}
#endif /* CONFIG_AS_APPLICATION */

bool
hitsuji::hitsuji_t::Initialize()
{
	LOG(INFO) << "Hitsuji: { "
		  "\"version\": \"" << version_major << '.' << version_minor << '.' << version_build << "\""
		", \"build\": { "
			  "\"date\": \"" << build_date << "\""
			", \"time\": \"" << build_time << "\""
			", \"system\": \"" << build_system << "\""
			", \"machine\": \"" << build_machine << "\""
			" }"
		", \"config\": " << config_ <<
		" }";
	try {
/* UPA context */
		upa_.reset (new upa_t (config_));
		if (!(bool)upa_ || !upa_->Initialize())
			goto cleanup;
/* UPA provider */
		provider_.reset (new provider_t (config_, upa_, static_cast<client_t::Delegate*> (this)));
		if (!(bool)provider_ || !provider_->Initialize())
			goto cleanup;
	} catch (const std::exception& e) {
		LOG(ERROR) << "Upa::Initialisation exception: { "
			"\"What\": \"" << e.what() << "\""
			" }";
		goto cleanup;
	}
	try {
		static const std::function<int(void*)> zmq_term_deleter = zmq_term;
		static const std::function<int(void*)> zmq_close_deleter = zmq_close;
/* ZeroMQ context */
		zmq_context_.reset (zmq_init (0), zmq_term_deleter);
		if (!(bool)zmq_context_)
			goto cleanup;
/* pull for worker reply messages */
		worker_reply_sock_.reset (zmq_socket (zmq_context_.get(), ZMQ_PULL), zmq_close_deleter);
		if (!(bool)worker_reply_sock_)
			goto cleanup;
		int rc = zmq_bind (worker_reply_sock_.get(), "inproc://worker/reply");
		if (rc)
			goto cleanup;
	} catch (const std::exception& e) {
		LOG(ERROR) << "ZMQ::Exception: { "
			"\"What\": \"" << e.what() << "\" }";
		goto cleanup;
	}
	try {
/* Worker threads */
		worker_.reset (new worker_t (provider_));
		if (!(bool)worker_ || !worker_->Initialize())
			goto cleanup;
	} catch (const std::exception& e) {
		LOG(ERROR) << "Worker::Initialisation exception: { "
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
hitsuji::hitsuji_t::OnRequest (
	uintptr_t handle,
	uint16_t rwf_version, 
	int32_t token,
	uint16_t service_id,
	const std::string& item_name,
	bool use_attribinfo_in_updates
	)
{
/* distribute to worker */
	static const int version = 0;
	sbe_hdr_->wrap (sbe_buf_, 0, version, sizeof (sbe_buf_))
	    .blockLength (Request::sbeBlockLength())
	    .templateId (Request::sbeTemplateId())
	    .schemaId (Request::sbeSchemaId())
	    .version (Request::sbeSchemaVersion());
	sbe_msg_->wrapForEncode (sbe_buf_, sbe_hdr_->size(), sizeof (sbe_buf_))
	    .handle (handle)
	    .rwfVersion (rwf_version)
	    .token (token)
	    .serviceId (service_id)
	    .useAttribInfoInUpdates (use_attribinfo_in_updates ? BooleanType::YES : BooleanType::NO);
	sbe_msg_->putItemName (item_name.c_str(), static_cast<int> (item_name.size()));

	LOG(INFO) << "Distributing task \"" << item_name << "\" to worker.";
	return worker_->OnTask (sbe_buf_, sbe_hdr_->size() + sbe_msg_->size());
}

bool
hitsuji::hitsuji_t::Start()
{
	LOG(INFO) << "Starting instance: { "
		  "\"instance\": " << instance_ <<
		" }";
	if (!shutting_down_ && Initialize()) {
/* Spawn new thread for message pump. */
		event_thread_.reset (new boost::thread ([this]() {
			MainLoop();
/* Raise condition loop is complete. */
			boost::lock_guard<boost::mutex> lock (mainloop_lock_);
			mainloop_shutdown_ = true;
			mainloop_cond_.notify_one();
		}));
	}
	return true;
}

void
hitsuji::hitsuji_t::Stop()
{
	LOG(INFO) << "Shutting down instance: { "
		  "\"instance\": " << instance_ <<
		" }";
	shutting_down_ = true;
	if ((bool)provider_) {
		provider_->Quit();
/* Wait for mainloop to quit */
		boost::unique_lock<boost::mutex> lock (mainloop_lock_);
		while (!mainloop_shutdown_)
			mainloop_cond_.wait (lock);
		Reset();
	}
}

void
hitsuji::hitsuji_t::Reset()
{
/* Worker threads */
	worker_.reset();
	chromium::debug::LeakTracker<worker_t>::CheckForLeaks();
/* Release ZMQ sockets before context */
	CHECK (worker_reply_sock_.use_count() <= 1);
	worker_reply_sock_.reset();
	CHECK (zmq_context_.use_count() <= 1);
	zmq_context_.reset();
/* Close client sockets with reference counts on provider. */
	if ((bool)provider_)
		provider_->Close();
/* Release everything with an UPA dependency. */
	CHECK_LE (provider_.use_count(), 1);
	provider_.reset();
/* Final tests before releasing UPA context */
	chromium::debug::LeakTracker<client_t>::CheckForLeaks();
	chromium::debug::LeakTracker<provider_t>::CheckForLeaks();
/* No more UPA sockets so close up context */
	CHECK_LE (upa_.use_count(), 1);
	upa_.reset();
	chromium::debug::LeakTracker<upa_t>::CheckForLeaks();
}

void
hitsuji::hitsuji_t::MainLoop()
{
	{
/* Add to global list of all instances. */
		boost::unique_lock<boost::shared_mutex> (global_list_lock_);
		global_list_.push_back (this);
	}
	try {
		provider_->Run(); 
	} catch (const std::exception& e) {
		LOG(ERROR) << "Runtime exception: { "
			"\"What\": \"" << e.what() << "\" }";
	}
	{
/* Remove from list before clearing. */
		boost::unique_lock<boost::shared_mutex> (global_list_lock_);
		global_list_.remove (this);
	}
}

/* eof */