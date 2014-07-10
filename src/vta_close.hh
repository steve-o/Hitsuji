/* Close-only OHLCV implementation.
 */

#ifndef VTA_CLOSE_HH_
#define VTA_CLOSE_HH_

/* Boost Accumulators */
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
/* Boost Posix Time */
#include <boost/date_time/posix_time/posix_time.hpp>

#include "vta.hh"
#include "accumulators/first.hh"

namespace vta
{
	class close_t : public intraday_t
	{
		typedef intraday_t super;
	public:
		close_t (const chromium::StringPiece& worker_name);
		~close_t();

		virtual bool ParseRequest (const chromium::StringPiece& url, const url_parse::Component& parsed_query) override;
		virtual bool Calculate (const chromium::StringPiece& symbol_name) override;
		virtual bool Calculate (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element) override;
		virtual bool WriteRaw (uint16_t rwf_version, int32_t token, uint16_t service_id, const chromium::StringPiece& item_name, const chromium::StringPiece& dacs_lock, void* data, size_t* length);
		virtual void Reset() override;

/* FlexRecPrimitives callback */
		static int OnFlexRecord(FRTreeCallbackInfo* info);

	private:
		double close_price() const { return boost::accumulators::first (last_price_); }
		uint64_t number_trades() const { return boost::accumulators::count (last_price_); }
		const boost::posix_time::ptime& open_time() const { return open_time_; }
		const boost::posix_time::ptime& close_time() const { return close_time_; }

/* Pre-allocated parsing state for requested items. */
		std::string value_;
		std::istringstream iss_;

/* Request parameters */
		boost::posix_time::ptime open_time_, close_time_;
/* Analytic state */
		boost::accumulators::accumulator_set<double,
			boost::accumulators::features<boost::accumulators::tag::first,
						      boost::accumulators::tag::count>> last_price_;
	};

} /* namespace vta */

#endif /* VTA_CLOSE_HH_ */

/* eof */