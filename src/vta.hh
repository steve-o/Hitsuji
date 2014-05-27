/* Intraday analytic interface for Vhayu Trade Analytics.
 */

#ifndef VTA_HH_
#define VTA_HH_

#include <cstdint>
#include <string>

/* Boost Posix Time */
#include <boost/date_time/posix_time/posix_time.hpp>

/* Velocity Analytics Plugin Framework */
#include <vpf/vpf.h>
#include <TBPrimitives.h>

namespace vta
{
	class intraday_t
	{
	public:
		intraday_t (const std::string& prefix, uint16_t rwf_version, int32_t token, uint16_t service_id, const std::string& name, const boost::posix_time::time_period& tp)
			: prefix_ (prefix)
			, rwf_version_ (rwf_version)
			, token_ (token)
			, service_id_ (service_id)
			, name_ (name)
			, tp_ (tp)
			, is_null_ (true)
		{
		}
		virtual ~intraday_t()
		{
		}

		virtual bool Calculate (const char* symbol_name) = 0;
		virtual bool Calculate (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element) = 0;
		virtual bool WriteRaw (char* data, size_t* length) = 0;
		virtual void Reset() = 0;
		void set_time_period (const boost::posix_time::time_period& tp) { tp_ = tp; }
		const boost::posix_time::time_period& time_period() const { return tp_; }
		operator bool() const { return !is_null_; }

	protected:
		uint8_t rwf_major_version() const { return rwf_version_ / 256; }
		uint8_t rwf_minor_version() const { return rwf_version_ % 256; }
		int32_t token() const { return token_; }
		uint16_t service_id() const { return service_id_; }
		const std::string& name() const { return name_; }
		void set() { is_null_ = false; }
		void clear() { is_null_ = true; }

/* logging unique identifier prefix */
		const std::string& prefix_;

	private:
		const uint16_t rwf_version_;
		const int32_t token_;
		const uint16_t service_id_;
		const std::string& name_;
		boost::posix_time::time_period tp_;
		bool is_null_;
	};

} /* namespace vta */

#endif /* VTA_HH_ */

/* eof */