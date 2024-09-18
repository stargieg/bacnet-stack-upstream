/**
 * @file
 * @brief Optional run-time assignment of BACnet datalink transport
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2004
 * @copyright SPDX-License-Identifier: MIT
 * @ingroup DataLink
 */
#ifndef BACNET_DATALINK_H
#define BACNET_DATALINK_H

#include "bacnet/basic/sys/bacnet_stack_exports.h"
#include "bacnet/config.h"
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"

#if defined(BACDL_ETHERNET)
#include "bacnet/datalink/ethernet.h"
#endif
#if defined(BACDL_ARCNET)
#include "bacnet/datalink/arcnet.h"
#endif
#if defined(BACDL_MSTP)
#include "bacnet/datalink/dlmstp.h"
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


#define MAX_HEADER (8)
#define MAX_MPDU (MAX_HEADER + MAX_PDU)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BACNET_STACK_EXPORT
bool datalink_init(char *ifname);

BACNET_STACK_EXPORT
int datalink_send_pdu(
    BACNET_ADDRESS *dest,
    BACNET_NPDU_DATA *npdu_data,
    uint8_t *pdu,
    unsigned pdu_len);

BACNET_STACK_EXPORT
uint16_t datalink_receive(
    BACNET_ADDRESS *src, uint8_t *pdu, uint16_t max_pdu, unsigned timeout);

BACNET_STACK_EXPORT
void datalink_cleanup(void);

BACNET_STACK_EXPORT
void datalink_get_broadcast_address(BACNET_ADDRESS *dest);

BACNET_STACK_EXPORT
void datalink_get_my_address(BACNET_ADDRESS *my_address);

BACNET_STACK_EXPORT
void datalink_set_interface(char *ifname);

BACNET_STACK_EXPORT
int datalink_set(char *datalink_string);

BACNET_STACK_EXPORT
int datalink_get(void);

BACNET_STACK_EXPORT
char *
datalink_get_interface(void);

BACNET_STACK_EXPORT
void datalink_maintenance_timer(uint16_t seconds);

#ifdef __cplusplus
}
#endif /* __cplusplus */

/** @defgroup DataLink The BACnet Network (DataLink) Layer
 * <b>6 THE NETWORK LAYER </b><br>
 * The purpose of the BACnet network layer is to provide the means by which
 * messages can be relayed from one BACnet network to another, regardless of
 * the BACnet data link technology in use on that network. Whereas the data
 * link layer provides the capability to address messages to a single device
 * or broadcast them to all devices on the local network, the network layer
 * allows messages to be directed to a single remote device, broadcast on a
 * remote network, or broadcast globally to all devices on all networks.
 * A BACnet Device is uniquely located by a network number and a MAC address.
 *
 * Each client or server application must define exactly one of these
 * DataLink settings, which will control which parts of the code will be built:
 * - BACDL_ETHERNET -- for Clause 7 ISO 8802-3 ("Ethernet") LAN
 * - BACDL_ARCNET   -- for Clause 8 ARCNET LAN
 * - BACDL_MSTP     -- for Clause 9 MASTER-SLAVE/TOKEN PASSING (MS/TP) LAN
 * - BACDL_BIP      -- for ANNEX J - BACnet/IPv4
 * - BACDL_BIP6     -- for ANNEX U - BACnet/IPv6
 * - BACDL_ALL      -- Unspecified for the build, so the transport can be
 *                     chosen at runtime from among these choices.
 * - BACDL_NONE      -- Unspecified for the build for unit testing
 * - BACDL_CUSTOM    -- For externally linked datalink_xxx functions
 * - Clause 10 POINT-TO-POINT (PTP) and Clause 11 EIA/CEA-709.1 ("LonTalk") LAN
 *   are not currently supported by this project.
 */
/** @defgroup DLTemplates DataLink Template Functions
 * @ingroup DataLink
 * Most of the functions in this group are function templates which are assigned
 * to a specific DataLink network layer implementation either at compile time or
 * at runtime.
 */
#endif
