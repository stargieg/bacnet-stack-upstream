/**************************************************************************
*
* Copyright (C) 2009 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/
#define DLENV_H
#define DATALINK_H
#include <stddef.h>
#include <stdint.h>
#include <stdint.h>
#include "bacnet/basic/sys/bacnet_stack_exports.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/npdu.h"

#if defined(BACDL_ARCNET)
#include "bacnet/datalink/arcnet.h"
#endif
#if defined(BACDL_BIP)
#include "bacnet/datalink/bip.h"
#include "bacnet/datalink/bvlc.h"
#include "bacnet/basic/bbmd/h_bbmd.h"
#endif
#if defined(BACDL_BIP6)
#include "bacnet/datalink/bip6.h"
#include "bacnet/datalink/bvlc6.h"
#include "bacnet/basic/bbmd6/h_bbmd6.h"
#endif
#if defined(BACDL_ETHERNET)
#include "bacnet/datalink/ethernet.h"
#endif
#if defined(BACDL_MSTP)
#include "bacnet/datalink/dlmstp.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    BACNET_STACK_EXPORT
    int dlenv_init(void);

    BACNET_STACK_EXPORT
    void bip_dlenv_debug_enable(
        void);

    BACNET_STACK_EXPORT
    void bip_dlenv_debug_disable(
        void);

    BACNET_STACK_EXPORT
    int dlenv_register_as_foreign_device(
        void);

    BACNET_STACK_EXPORT
    void dlenv_network_port_init(
        void);

    BACNET_STACK_EXPORT
    void dlenv_maintenance_timer(
        uint16_t elapsed_seconds);

    BACNET_STACK_EXPORT
    void dlenv_bbmd_address_set(
        BACNET_IP_ADDRESS *address);

    BACNET_STACK_EXPORT
    void dlenv_bbmd_ttl_set(
        uint16_t ttl_secs);

    BACNET_STACK_EXPORT
    int dlenv_bbmd_result(
        void);

    BACNET_STACK_EXPORT
    void dlenv_cleanup(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
