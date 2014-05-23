/* UPA interactive fake snapshot provider.
 *
 * An interactive provider sits listening on a port for RSSL connections,
 * once a client is connected requests may be submitted for snapshots or
 * subscriptions to item streams.  This application will broadcast updates
 * continuously independent of client interest and the provider side will
 * perform fan-out as required.
 *
 * The provider is not required to perform last value caching, forcing the
 * client to wait for a subsequent broadcast to actually see data.
 */

#ifndef HITSUJI_HH_
#define HITSUJI_HH_

#include <cstdint>
#include <memory>

/* Boost noncopyable base class */
#include <boost/utility.hpp>

#include "config.hh"
#include "provider.hh"

namespace hitsuji
{
	class upa_t;
	class provider_t;

	class hitsuji_t :
		boost::noncopyable
	{
	public:
		hitsuji_t();
		~hitsuji_t();

/* Run the provider with the given command-line parameters.
 * Returns the error code to be returned by main().
 */
		int Run();
		void Clear();

	private:

/* Run core event loop. */
		void MainLoop();

/* Application configuration. */
		config_t config_;

/* UPA context. */
		std::shared_ptr<upa_t> upa_;

/* UPA provider */
		std::shared_ptr<provider_t> provider_;	
	};

} /* namespace hitsuji */

#endif /* HITSUJI_HH_ */

/* eof */