/*####COPYRIGHTBEGIN####
 -------------------------------------------
 Copyright (C) 2007 Steve Karg

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either version 2
 of the License, or (at your option) any later version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to:
 The Free Software Foundation, Inc.
 59 Temple Place - Suite 330
 Boston, MA  02111-1307, USA.

 As a special exception, if other files instantiate templates or
 use macros or inline functions from this file, or you compile
 this file and link it with other works to produce a work based
 on this file, this file does not by itself cause the resulting
 work to be covered by the GNU General Public License. However
 the source code for this file must still be made available in
 accordance with section (3) of the GNU General Public License.

 This exception does not invalidate any other reasons why a work
 based on this file might be covered by the GNU General Public
 License.
 -------------------------------------------
####COPYRIGHTEND####*/
/** @file datalink.c  Optional run-time assignment of datalink transport */
#include "bacnet/datalink/datalink.h"
#include <strings.h>

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

static enum {
    DATALINK_NONE = 0,
    DATALINK_ARCNET,
    DATALINK_ETHERNET,
    DATALINK_BIP,
    DATALINK_BIP6,
    DATALINK_MSTP
} Datalink_Transport;

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
    } else if (strcasecmp("none", datalink_string) == 0) {
        Datalink_Transport = DATALINK_NONE;
    }
    return Datalink_Transport;
}

bool datalink_init(char *ifname)
{
    bool status = false;

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
        default:
            break;
    }

    return status;
}

int datalink_send_pdu(BACNET_ADDRESS *dest,
    BACNET_NPDU_DATA *npdu_data,
    uint8_t *pdu,
    unsigned pdu_len)
{
    int status = 0;
    switch (Datalink_Transport) {
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            status = arcnet_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            status = ethernet_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            status = bip_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            status = bip6_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            status = dlmstp_send_pdu(dest, npdu_data, pdu, pdu_len);
            break;
#endif
        default:
            break;
    }
    return status;
}

uint16_t datalink_receive(BACNET_ADDRESS *src,
    uint8_t *npdu,
    uint16_t max_npdu,
    unsigned timeout)
{
    uint16_t status = 0;
    switch (Datalink_Transport) {
#if defined(BACDL_ARCNET)
        case DATALINK_ARCNET:
            status = arcnet_receive(src, npdu, max_npdu, timeout);
            break;
#endif
#if defined(BACDL_ETHERNET)
        case DATALINK_ETHERNET:
            status = ethernet_receive(src, npdu, max_npdu, timeout);
            break;
#endif
#if defined(BACDL_BIP)
        case DATALINK_BIP:
            status = bip_receive(src, npdu, max_npdu, timeout);
            break;
#endif
#if defined(BACDL_BIP6)
        case DATALINK_BIP6:
            status = bip6_receive(src, npdu, max_npdu, timeout);
            break;
#endif
#if defined(BACDL_MSTP)
        case DATALINK_MSTP:
            status = dlmstp_receive(src, npdu, max_npdu, timeout);
            break;
#endif
        default:
            break;
    }
    return status;
}

void datalink_cleanup(void)
{
    switch (Datalink_Transport) {
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
        default:
            break;
    }
    return;
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
        default:
            break;
    }
}

void datalink_maintenance_timer(uint16_t seconds)
{
    switch (Datalink_Transport) {
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
        default:
            break;
    }
    return;
}
