/**
 * @file
 * @brief Optional run-time assignment of BACnet datalink transport
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2004
 * @copyright SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0
 * @defgroup DataLink DataLink Network Layer
 * @ingroup DataLink
 */
#include "bacnet/datalink/datalink.h"

#if defined(BACDL_ETHERNET)
#include "bacnet/datalink/ethernet.h"
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
#if defined(BACDL_ARCNET)
#include "bacnet/datalink/arcnet.h"
#endif
#if defined(BACDL_MSTP)
#include "bacnet/datalink/dlmstp.h"
#endif
#if defined(BACDL_BSC)
#include "bacnet/datalink/bsc/bsc-datalink.h"
#endif
#ifdef HAVE_STRINGS_H
#include <strings.h> /* for strcasecmp() */
#endif

static enum {
    DATALINK_NONE = 0,
    DATALINK_ARCNET,
    DATALINK_ETHERNET,
    DATALINK_BIP,
    DATALINK_BIP6,
    DATALINK_MSTP,
    DATALINK_BSC
} Datalink_Transport;

char *Datalink_Ifname;

int datalink_set(char *datalink_string)
{
    if (strcasecmp("bip", datalink_string) == 0) {
        Datalink_Transport = DATALINK_BIP;
    } else if (strcasecmp("bip6", datalink_string) == 0) {
        Datalink_Transport = DATALINK_BIP6;
    } else if (strcasecmp("ethernet", datalink_string) == 0) {
        Datalink_Transport = DATALINK_ETHERNET;
    } else if (strcasecmp("arcnet", datalink_string) == 0) {
        Datalink_Transport = DATALINK_ARCNET;
    } else if (strcasecmp("mstp", datalink_string) == 0) {
        Datalink_Transport = DATALINK_MSTP;
    } else if (strcasecmp("bsc", datalink_string) == 0) {
        Datalink_Transport = DATALINK_BSC;
    } else if (strcasecmp("none", datalink_string) == 0) {
        Datalink_Transport = DATALINK_NONE;
    }
    return Datalink_Transport;
}

int datalink_get(void)
{
    return Datalink_Transport;
}
char *
datalink_get_interface(void)
{
    return Datalink_Ifname;
}

bool datalink_init(char *ifname)
{
    bool status = false;
    Datalink_Ifname = ifname;
    switch (Datalink_Transport) {
        case DATALINK_NONE:
            status = true;
            break;
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            status = arcnet_init(ifname);
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            status = ethernet_init(ifname);
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            status = bip_init(ifname);
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            status = bip6_init(ifname);
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            status = dlmstp_init(ifname);
            break;
#endif
#if defined(BACDL_BSC)
        case DATALINK_BSC:
            status = bsc_init(ifname);
            break;
#endif
        default:
            break;
    }

    return status;
}

int datalink_send_pdu(
    BACNET_ADDRESS *dest,
    BACNET_NPDU_DATA *npdu_data,
    uint8_t *pdu,
    unsigned pdu_len)
{
    int bytes = 0;

    switch (Datalink_Transport) {
        case DATALINK_NONE:
            bytes = pdu_len;
            break;
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            bytes = arcnet_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            bytes = ethernet_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            bytes = bip_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            bytes = bip6_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            bytes = dlmstp_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_BSC)
        case DATALINK_BSC:
            bytes = bsc_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
        default:
            break;
    }

    return bytes;
}

uint16_t datalink_receive(
    BACNET_ADDRESS *src, uint8_t *pdu, uint16_t max_pdu, unsigned timeout)
{
    uint16_t bytes = 0;

    switch (Datalink_Transport) {
        case DATALINK_NONE:
            break;
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            bytes = arcnet_receive(src, pdu, max_pdu, timeout);
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            bytes = ethernet_receive(src, pdu, max_pdu, timeout);
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            bytes = bip_receive(src, pdu, max_pdu, timeout);
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            bytes = bip6_receive(src, pdu, max_pdu, timeout);
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            bytes = dlmstp_receive(src, pdu, max_pdu, timeout);
            break;
#endif
#if defined(BACDL_BSC)
        case DATALINK_BSC:
            bytes = bsc_receive(src, pdu, max_pdu, timeout);
            break;
#endif
        default:
            break;
    }

    return bytes;
}

void datalink_cleanup(void)
{
    switch (Datalink_Transport) {
        case DATALINK_NONE:
            break;
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            arcnet_cleanup();
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            ethernet_cleanup();
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            bip_cleanup();
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            bip6_cleanup();
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            dlmstp_cleanup();
            break;
#endif
#if defined(BACDL_BSC)
        case DATALINK_BSC:
            bsc_cleanup();
            break;
#endif
        default:
            break;
    }
}

void datalink_get_broadcast_address(BACNET_ADDRESS *dest)
{
    switch (Datalink_Transport) {
        case DATALINK_NONE:
            break;
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            arcnet_get_broadcast_address(dest);
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            ethernet_get_broadcast_address(dest);
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            bip_get_broadcast_address(dest);
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            bip6_get_broadcast_address(dest);
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            dlmstp_get_broadcast_address(dest);
            break;
#endif
#if defined(BACDL_BSC)
        case DATALINK_BSC:
            bsc_get_broadcast_address(dest);
            break;
#endif
        default:
            break;
    }
}

void datalink_get_my_address(BACNET_ADDRESS *my_address)
{
    switch (Datalink_Transport) {
        case DATALINK_NONE:
            break;
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            arcnet_get_my_address(my_address);
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            ethernet_get_my_address(my_address);
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            bip_get_my_address(my_address);
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            bip6_get_my_address(my_address);
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            dlmstp_get_my_address(my_address);
            break;
#endif
#if defined(BACDL_BSC)
        case DATALINK_BSC:
            bsc_get_my_address(my_address);
            break;
#endif
        default:
            break;
    }
}

void datalink_maintenance_timer(uint16_t seconds)
{
    switch (Datalink_Transport) {
        case DATALINK_NONE:
            break;
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            bvlc_maintenance_timer(seconds);
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            bvlc6_maintenance_timer(seconds);
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            break;
#endif
#if defined(BACDL_BSC)
        case DATALINK_BSC:
            bsc_maintenance_timer(seconds);
            break;
#endif
        default:
            break;
    }
}