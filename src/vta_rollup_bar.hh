/* OHLCV implementation rolling up 1 minute bars.
 *
 * VA 4-5pm 1 hour bar, inclusive.
 * (1) MSFT.O?open=1383750000&close=1383753599 = 15240226
 * (2) MSFT.O?open=1383750001&close=1383753600 = 15248724
 * (3) MSFT.O?open=1383750000&close=1383753600 = 15248824
 *
 * VA treats midnight as last second of yesterday, 24:00, not 00:00.
 *
 * TREP-VA's BarGenPreference=0 (default) matches (2)
 * MSFT.O?open=1383750000&close=1383753599#rollup = VA rollup bar.
 *
 * BarGenPreference=1 matches (1)
 * MSFT.O?open=1383750000&close=1383753599#rollup = VA rollup bar.
 */

#ifndef VTA_ROLLUP_BAR_HH_
#define VTA_ROLLUP_BAR_HH_

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
	class rollup_bar_t : public intraday_t
	{
		typedef intraday_t super;
	public:
		rollup_bar_t (const chromium::StringPiece& worker_name);
		~rollup_bar_t();

		virtual bool ParseRequest (const chromium::StringPiece& url, const url_parse::Component& parsed_query) override;
		virtual bool Calculate (const chromium::StringPiece& symbol_name) override;
		virtual bool Calculate (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element) override;
		virtual bool WriteRaw (uint16_t rwf_version, int32_t token, uint16_t service_id, const chromium::StringPiece& item_name, const chromium::StringPiece& dacs_lock, void* data, size_t* length);
		virtual void Reset() override;

/* FlexRecPrimitives callback */
		static int OnFlexRecord(FRTreeCallbackInfo* info);

	private:
		double open_price() const { return boost::accumulators::first (open_price_); }
		double high_price() const { return boost::accumulators::max (high_price_); }
		double low_price() const { return boost::accumulators::min (low_price_); }
		double close_price() const { return boost::accumulators::last (close_price_); }
		uint64_t number_trades() const { return boost::accumulators::sum (num_moves_); }
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
			boost::accumulators::features<boost::accumulators::tag::first>> open_price_;
		boost::accumulators::accumulator_set<double,
			boost::accumulators::features<boost::accumulators::tag::last>> close_price_;
		boost::accumulators::accumulator_set<double,
			boost::accumulators::features<boost::accumulators::tag::max>> high_price_;
		boost::accumulators::accumulator_set<double,
			boost::accumulators::features<boost::accumulators::tag::min>> low_price_;
		boost::accumulators::accumulator_set<uint64_t,
			boost::accumulators::features<boost::accumulators::tag::sum>> tick_volume_, num_moves_;
	};

} /* namespace vta */

#endif /* VTA_ROLLUP_BAR_HH_ */

/* eof */