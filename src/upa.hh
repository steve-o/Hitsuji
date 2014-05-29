/* UPA context.
 */

#ifndef UPA_HH_
#define UPA_HH_

#include <memory>

#include "chromium/debug/leak_tracker.hh"
#include "config.hh"

namespace hitsuji
{
	class upa_t
	{
	public:
		explicit upa_t (const config_t& config);
		~upa_t();

		bool Initialize();
		bool VerifyVersion();

	private:
		const config_t& config_;

		chromium::debug::LeakTracker<upa_t> leak_tracker_;
	};

} /* namespace hitsuji */

#endif /* UPA_HH_ */

/* eof */