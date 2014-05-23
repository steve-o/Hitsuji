/* User-configurable settings.
 */

#include "config.hh"

static const char* kDefaultRsslPort = "14002";

hitsuji::config_t::config_t() :
/* default values */
	service_name ("NOCACHE_VTA"),
	rssl_port ("24002"),
	vendor_name ("Thomson Reuters"),
	maximum_data_size (64 * 1024),
	session_capacity (8),
	worker_count (2)
{
/* C++11 initializer lists not supported in MSVC2010 */
}

/* eof */