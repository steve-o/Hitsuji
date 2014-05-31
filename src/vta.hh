/* Intraday analytic interface for Vhayu Trade Analytics.
 */

#ifndef VTA_HH_
#define VTA_HH_

#include <cstdint>
#include <sstream>
#include <string>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>
#include <TBPrimitives.h>

#include "googleurl/url_parse.h"

namespace vta
{
	class intraday_t
	{
	public:
		intraday_t (const std::string& worker_name)
		{
/* Set logger ID */
			std::ostringstream ss;
			ss << worker_name << ':';
			prefix_.assign (ss.str());
		}
		virtual ~intraday_t()
		{
		}

		virtual bool ParseRequest (const std::string& url, const url_parse::Component& parsed_query) = 0;
		virtual bool Calculate (const char* symbol_name) = 0;
		virtual bool Calculate (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element) = 0;
		virtual bool WriteRaw (uint16_t rwf_version, int32_t token, uint16_t service_id, const std::string& item_name, char* data, size_t* length) = 0;
		virtual void Reset() = 0;

	protected:
		uint8_t rwf_major_version (uint16_t rwf_version) const { return rwf_version / 256; }
		uint8_t rwf_minor_version (uint16_t rwf_version) const { return rwf_version % 256; }

/* logging unique identifier prefix */
		std::string prefix_;
	};

} /* namespace vta */

#endif /* VTA_HH_ */

/* eof */