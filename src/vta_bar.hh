/* OHLCV implementation.
 */

#ifndef VTA_BAR_HH_
#define VTA_BAR_HH_

/* Boost Accumulators */
#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/count.hpp>
#include <boost/accumulators/statistics/max.hpp>
#include <boost/accumulators/statistics/min.hpp>
#include <boost/accumulators/statistics/sum.hpp>
/* Boost Posix Time */
#include <boost/date_time/posix_time/posix_time.hpp>

#include "vta.hh"
#include "accumulators/first.hh"
#include "accumulators/last.hh"

#ifdef max
#	undef max
#endif
#ifdef min
#	undef min
#endif

namespace vta
{
	class bar_t : public intraday_t
	{
		typedef intraday_t super;
	public:
		bar_t (const chromium::StringPiece& worker_name);
		~bar_t();

		virtual bool ParseRequest (const chromium::StringPiece& url, const url_parse::Component& parsed_query) override;
		virtual bool Calculate (const chromium::StringPiece& symbol_name) override;
		virtual bool Calculate (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element) override;
		virtual bool WriteRaw (uint16_t rwf_version, int32_t token, uint16_t service_id, const chromium::StringPiece& item_name, void* data, size_t* length);
		virtual void Reset() override;

/* FlexRecPrimitives callback */
		static int OnFlexRecord(FRTreeCallbackInfo* info);

	private:
		double open_price() const { return boost::accumulators::first (last_price_); }
		double high_price() const { return boost::accumulators::max (last_price_); }
		double low_price() const { return boost::accumulators::min (last_price_); }
		double close_price() const { return boost::accumulators::last (last_price_); }
		uint64_t number_trades() const { return boost::accumulators::count (last_price_); }
		uint64_t accumulated_volume() const { return boost::accumulators::sum (tick_volume_); }
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
						      boost::accumulators::tag::last,
						      boost::accumulators::tag::max,
						      boost::accumulators::tag::min,
						      boost::accumulators::tag::count>> last_price_;
		boost::accumulators::accumulator_set<uint64_t,
			boost::accumulators::features<boost::accumulators::tag::sum>> tick_volume_;
	};

} /* namespace vta */

#endif /* VTA_BAR_HH_ */

/* eof */