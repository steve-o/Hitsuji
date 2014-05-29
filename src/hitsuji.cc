/* UPA interactive fake snapshot provider.  Fake snapshots are convenient for
 * consumption in TR products such as Eikon, DataView, RsslSinkApp.
 */

#include "hitsuji.hh"

#define __STDC_FORMAT_MACROS
#include <cstdint>
#include <inttypes.h>

#include <windows.h>

#include "chromium/logging.hh"
#include "provider.hh"
#include "upa.hh"
#include "version.hh"

#include "vta_bar.hh"
#include "vta_test.hh"

/* Maximum encoded size of an RSSL provider to client message. */
#define MAX_MSG_SIZE 4096

static const std::string kErrorMalformedRequest = "Malformed request.";
static const std::string kErrorNotFound = "Not found in SearchEngine.";
static const std::string kErrorInternal = "Internal error.";

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
		LOG(ERROR) << "Initialisation exception: { "
			"\"What\": \"" << e.what() << "\" }";
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
	std::weak_ptr<client_t> client,
	uint16_t rwf_version, 
	int32_t token,
	uint16_t service_id,
	const std::string& item_name,
	bool use_attribinfo_in_updates
	)
{
	vta::bar_t vta (rwf_version, token, service_id, item_name);
/* Validate request, e.g. be satisifed with this SearchEngine instance */
	if (!(bool)vta) {
		auto sp = client.lock();
		return sp->ReplyWithClose (token, service_id, RSSL_DMT_MARKET_PRICE, item_name.c_str(), item_name.size(), use_attribinfo_in_updates,
					RSSL_STREAM_CLOSED, RSSL_SC_NOT_FOUND, kErrorMalformedRequest);
	}
/* Fake asynchronous operation */
	if (!vta.Calculate (vta.underlying_symbol_.c_str())) {
		if (auto sp = client.lock()) {
			return sp->ReplyWithClose (token, service_id, RSSL_DMT_MARKET_PRICE, item_name.c_str(), item_name.size(), use_attribinfo_in_updates,
						RSSL_STREAM_CLOSED_RECOVER, RSSL_SC_ERROR, kErrorInternal);
		} else {
			return false;
		}
	}
	char buf[MAX_MSG_SIZE];
	size_t length = sizeof (buf);
	if (auto sp = client.lock()) {
		if (!vta.WriteRaw (buf, &length)) {
			return sp->ReplyWithClose (token, service_id, RSSL_DMT_MARKET_PRICE, item_name.c_str(), item_name.size(), use_attribinfo_in_updates,
						RSSL_STREAM_CLOSED_RECOVER, RSSL_SC_ERROR, kErrorInternal);
		}
		return sp->Reply (buf, length, token);
	} else {
		return false;
	}
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
/* Raise condition loop is complete. */
	{
		boost::lock_guard<boost::mutex> lock (mainloop_lock_);
		mainloop_shutdown_ = true;
	}
	mainloop_cond_.notify_one();
}

/* eof */