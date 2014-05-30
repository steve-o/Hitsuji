/* UPA interactive fake snapshot provider.  Fake snapshots are convenient for
 * consumption in TR products such as Eikon, DataView, RsslSinkApp.
 */

#ifndef HITSUJI_HH_
#define HITSUJI_HH_

#include <cstdint>
#include <list>
#include <memory>

/* Boost Atomics */
#include <boost/atomic.hpp>

/* Boost threading */
#include <boost/thread.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>

#include "client.hh"
#include "config.hh"

namespace hitsuji
{
	class provider_t;
	class upa_t;

	class hitsuji_t
/* Permit global weak pointer to application instance for shutdown notification. */
		: public std::enable_shared_from_this<hitsuji_t>
#ifndef CONFIG_AS_APPLICATION
/* SearchEngine plugin interface. */
		, public vpf::AbstractUserPlugin
/* Tcl API interface. */
		, public vpf::Command
#endif
		, public client_t::Delegate
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
		virtual bool OnRequest (std::weak_ptr<client_t> client, uint16_t rwf_version, int32_t token, uint16_t service_id, const std::string& item_name, bool use_attribinfo_in_updates) override;

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

/* Mainloop procesing thread. */
		std::unique_ptr<boost::thread> event_thread_;

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
/* FLexRecord cursor */
		FlexRecDefinitionManager* manager_;
		std::shared_ptr<FlexRecWorkAreaElement> work_area_;
		std::shared_ptr<FlexRecViewElement> view_element_;
	};

} /* namespace hitsuji */

#endif /* HITSUJI_HH_ */

/* eof */