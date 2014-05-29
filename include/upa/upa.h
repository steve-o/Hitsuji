/* The missing header upa.h
 */

#ifndef UPA_MISSING_H_
#define UPA_MISSING_H_

/* Defect with upa7.6.0.L1.win.rrg\Include\rtr/rsslQos.h(132) : warning C4996: '_snprintf' */
#pragma warning(push)
#pragma warning(disable: 4996)

#include <rtr/rsslDataPackage.h>
#include <rtr/rsslMessagePackage.h>
#include <rtr/rsslTransport.h>

#pragma warning(pop)

#endif /* UPA_MISSING_H_ */

/* eof */
