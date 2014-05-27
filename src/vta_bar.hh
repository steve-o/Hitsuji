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
		bar_t (const std::string& prefix, uint16_t rwf_version, int32_t token, uint16_t service_id, const std::string& name, const boost::posix_time::time_period& tp);
		~bar_t();

		virtual bool Calculate (const char* symbol_name) override;
		virtual bool Calculate (const TBSymbolHandle& handle, FlexRecWorkAreaElement* work_area, FlexRecViewElement* view_element) override;
		virtual bool WriteRaw (char* data, size_t* length);
		virtual void Reset() override;

		static int OnFlexRecord(FRTreeCallbackInfo* info);

	private:
		double open_price() const { return boost::accumulators::first (last_price_); }
		double high_price() const { return boost::accumulators::max (last_price_); }
		double low_price() const { return boost::accumulators::min (last_price_); }
		double close_price() const { return boost::accumulators::last (last_price_); }
		uint64_t number_trades() const { return boost::accumulators::count (last_price_); }
		uint64_t accumulated_volume() const { return boost::accumulators::sum (tick_volume_); }

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