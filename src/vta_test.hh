/* A simple test implementation.
 */

#ifndef VTA_TEST_HH_
#define VTA_TEST_HH_

#include "vta.hh"

namespace vta
{
	class test_t : public intraday_t
	{
		typedef intraday_t super;
	public:
		test_t (const std::string& worker_name);
		~test_t();

		virtual bool ParseRequest (const std::string& url, const url_parse::Component& parsed_query) override;
		virtual bool Calculate (const char* symbol_name) override;
		virtual bool Calculate (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element) override;
		virtual bool WriteRaw (uint16_t rwf_version, int32_t token, uint16_t service_id, const std::string& item_name, char* data, size_t* length);
		virtual void Reset() override;
	};

} /* namespace vta */

#endif /* VTA_TEST_HH_ */

/* eof */