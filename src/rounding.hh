/* Thomson Reuters hosting specific business logic.
 */

#ifndef ROUNDING_HH_
#define ROUNDING_HH_

#include <cmath>
#include <cstdint>

#include <rtr/rsslReal.h>

namespace rounding
{

static inline
uint8_t
hint()
{
	return RSSL_RH_EXPONENT_4;
}

static inline
double
round_half_up (double x)
{
	return std::floor (x + 0.5);
}

/* mantissa of 10E4
 */
static inline
int64_t
mantissa (double x)
{
	return (int64_t) round_half_up (x * 10000.0);
}

/* round a double value to 4 decimal places using round half up
 */
static inline
double
round (double x)
{
	return (double) mantissa (x) / 10000.0;
}

} // namespace rounding

#endif /* ROUNDING_HH_ */

/* eof */