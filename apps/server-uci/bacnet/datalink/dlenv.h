/**
 * @file
 * @brief Environment variables used for the BACnet command line tools
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2009
 * @copyright SPDX-License-Identifier: MIT
 * @ingroup DataLink
 */
#ifndef BACNET_DLENV_H
#define BACNET_DLENV_H
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
void bip_dl_debug_enable(void);

BACNET_STACK_EXPORT
void bip_dl_debug_disable(void);

BACNET_STACK_EXPORT
int dlenv_register_as_foreign_device(void);

#if 0
BACNET_STACK_EXPORT
void dlenv_network_port_init(void);
#endif
#if defined(BACDL_ARCNET)
BACNET_STACK_EXPORT
void dlenv_network_port_init_arcnet(void);
#endif
#if defined(BACDL_ETHERNET)
BACNET_STACK_EXPORT
void dlenv_network_port_init_ethernet(void);
#endif
#if defined(BACDL_BIP)
BACNET_STACK_EXPORT
void dlenv_network_port_init_bip(void);
#endif
#if defined(BACDL_BIP6)
BACNET_STACK_EXPORT
void dlenv_network_port_init_bip6(void);
#endif
#if defined(BACDL_MSTP)
BACNET_STACK_EXPORT
void dlenv_network_port_init_mstp(void);
#endif
#if defined(BACDL_BSC)
BACNET_STACK_EXPORT
void dlenv_network_port_init_bsc(void);
#endif


BACNET_STACK_EXPORT
void dlenv_maintenance_timer(uint16_t elapsed_seconds);

BACNET_STACK_EXPORT
void dlenv_bbmd_address_set(const BACNET_IP_ADDRESS *address);

BACNET_STACK_EXPORT
void dlenv_bbmd_ttl_set(uint16_t ttl_secs);

BACNET_STACK_EXPORT
int dlenv_bbmd_result(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
