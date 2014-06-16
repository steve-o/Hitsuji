/* Reimplmentation of FlexRecPermData internal to SEDll.
 */

#ifndef PERMDATA_HH_
#define PERMDATA_HH_

#include <cstdint>
#include <unordered_map>
#include <string>

/* Boost threading. */
#include <boost/thread.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>
#include <TBPrimitives.h>

#include "chromium/string_piece.hh"

namespace vhayu
{
	class permdata_t
	{
	public:
		permdata_t() {}

		bool GetDacsLock (const chromium::StringPiece& item_name, std::string *lock);
		bool GetDacsLock (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element, std::string *lock);

/* FlexRecPrimitives callback */
		static int OnFlexRecord(FRTreeCallbackInfo* info);

		static void asciiLockToBinary (const chromium::StringPiece& ascii_lock, std::string* dacs_lock);

	protected:
// TBD: performance testing.
//		std::unordered_map<std::string, std::string> map_;
//		boost::shared_mutex map_lock_;
	};

} /* namespace vhayu */

#endif /* PERMDATA_HH_ */

/* eof */