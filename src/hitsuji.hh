/* UPA interactive fake snapshot provider.  Fake snapshots are convenient for
 * consumption in TR products such as Eikon, DataView, RsslSinkApp.
 */

#ifndef HITSUJI_HH_
#define HITSUJI_HH_

#include <cstdint>
#include <forward_list>
#include <list>
#include <memory>

/* Boost Atomics */
#include <boost/atomic.hpp>

/* Boost threading */
#include <boost/thread.hpp>

/* ZeroMQ messaging middleware. */
#include <zmq.h>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "googleurl/url_parse.h"
#include "chromium/string_piece.hh"
#include "client.hh"
#include "provider.hh"
#include "config.hh"

/* Maximum encoded size of an RSSL provider to client message. */
#define MAX_MSG_SIZE 4096

namespace vta
{
	class bar_t;
	class test_t;
}

namespace hitsuji
{
	class upa_t;
	class worker_t;
	class MessageHeader;
	class Request;
	class Reply;

	class hitsuji_t
/* Permit global weak pointer to application instance for shutdown notification. */
		: public std::enable_shared_from_this<hitsuji_t>
#ifndef CONFIG_AS_APPLICATION
/* SearchEngine plugin interface. */
		, public vpf::AbstractUserPlugin
/* Tcl API interface. */
		, public vpf::Command
#endif
		, public client_t::Delegate	/* Rssl requests */
		, public provider_t::Delegate	/* Worker replies */
	{
	public:
		explicit hitsuji_t();
		virtual ~hitsuji_t();

#ifndef CONFIG_AS_APPLICATION
/* Plugin entry point. */
		virtual void init (const vpf::UserPluginConfig& config_) override;
/* Plugin termination point. */
		virtual void destroy() override;
/* Tcl entry point. */
		virtual int execute (const vpf::CommandInfo& cmdInfo, vpf::TCLCommandData& cmdData) override;
#else
/* Run as an application.  Blocks until Quit is called.  Returns the error code
 * to be returned by main().
 */
		int Run();
/* Quit an earlier call to Run(). */
		void Quit();
#endif
		virtual bool OnRequest (uintptr_t handle, uint16_t rwf_version, int32_t token, uint16_t service_id, const std::string& item_name, bool use_attribinfo_in_updates) override;
		virtual bool OnRequest (uintptr_t handle, uint16_t rwf_version, int32_t token, uint16_t service_id, const std::string& item_name, bool use_attribinfo_in_updates, const std::vector<int_fast16_t>& view_by_fid) override;
		virtual bool OnRead() override;

		bool Initialize();
		void Reset();

/* Global list of all instances.  SearchEngine.exe owns pointer. */
		static std::list<hitsuji_t*> global_list_;
		static boost::shared_mutex global_list_lock_;
	private:
/* Run core event loop. */
		void MainLoop();

/* Start the encapsulated provider instance until Stop is called.  Stop may be
 * called to pre-emptively prevent execution.
 */
		bool Start();
		void Stop();

		bool AbortWorkers();
		bool AbortOneWorker();

		bool OnReply (const void* buffer, size_t length);

/* Mainloop procesing thread. */
		std::unique_ptr<boost::thread> event_thread_;
/* Worker threads*/		
		std::forward_list<std::pair<std::shared_ptr<worker_t>, std::shared_ptr<boost::thread>>> workers_;

/* Asynchronous shutdown notification mechanism. */
		boost::condition_variable mainloop_cond_;
		boost::mutex mainloop_lock_;
		bool mainloop_shutdown_;
/* Flag to indicate Stop has be called and thus prohibit start of new provider. */
		boost::atomic_bool shutting_down_;
/* Plugin Xml identifiers. */
		std::string plugin_id_, plugin_type_;
/* Unique instance number per process. */
		unsigned instance_;
		static boost::atomic_uint instance_count_;
/* Application configuration. */
		config_t config_;
/* UPA context. */
		std::shared_ptr<upa_t> upa_;
/* UPA provider */
		std::shared_ptr<provider_t> provider_;
/* ZMQ context. */
		std::shared_ptr<void> zmq_context_;
		std::shared_ptr<void> worker_request_sock_;
		std::shared_ptr<void> worker_reply_sock_;
/* ZMQ message */
		zmq_msg_t zmq_msg_;
/* Sbe message buffer */
		std::shared_ptr<MessageHeader> sbe_hdr_;
		std::shared_ptr<Request> sbe_request_;
		std::shared_ptr<Reply> sbe_reply_;
	};

} /* namespace hitsuji */

#endif /* HITSUJI_HH_ */

/* eof */