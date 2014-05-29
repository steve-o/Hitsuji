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

namespace vta
{
	class intraday_t
	{
	public:
		intraday_t (uint16_t rwf_version, int32_t token, uint16_t service_id, const std::string& item_name)
			: rwf_version_ (rwf_version)
			, token_ (token)
			, service_id_ (service_id)
			, item_name_ (item_name)
			, has_valid_request_ (false)
		{
/* Set logger ID */
			std::ostringstream ss;
			ss << token << ':';
			prefix_.assign (ss.str());
		}
		virtual ~intraday_t()
		{
		}

		virtual bool Calculate (const char* symbol_name) = 0;
		virtual bool Calculate (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element) = 0;
		virtual bool WriteRaw (char* data, size_t* length) = 0;
		virtual void Reset() = 0;
		operator bool() const { return has_valid_request_; }

	protected:
		uint8_t rwf_major_version() const { return rwf_version_ / 256; }
		uint8_t rwf_minor_version() const { return rwf_version_ % 256; }
		int32_t token() const { return token_; }
		uint16_t service_id() const { return service_id_; }
		const std::string& item_name() const { return item_name_; }
		void set_has_valid_request() { has_valid_request_ = true; }
		void clear_has_valid_request() { has_valid_request_ = false; }

/* logging unique identifier prefix */
		std::string prefix_;

	private:
		const uint16_t rwf_version_;
		const int32_t token_;
		const uint16_t service_id_;
		const std::string& item_name_;
		bool has_valid_request_;
	};

} /* namespace vta */

#endif /* VTA_HH_ */

/* eof */