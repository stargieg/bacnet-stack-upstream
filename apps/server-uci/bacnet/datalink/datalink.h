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
#define DATALINK_H

#include "bacnet/bacnet_stack_exports.h"
#include "bacnet/config.h"
#include "bacnet/bacdef.h"

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


#define MAX_HEADER (8)
#define MAX_MPDU (MAX_HEADER+MAX_PDU)

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    BACNET_STACK_EXPORT
    bool datalink_init(char *ifname);

    BACNET_STACK_EXPORT
    int datalink_send_pdu(BACNET_ADDRESS *dest,
        BACNET_NPDU_DATA *npdu_data,
        uint8_t *pdu,
        unsigned pdu_len);

    BACNET_STACK_EXPORT
    uint16_t datalink_receive(
        BACNET_ADDRESS *src, uint8_t *npdu,
        uint16_t max_npdu, unsigned timeout);

    BACNET_STACK_EXPORT
    void datalink_cleanup(
        void);

    BACNET_STACK_EXPORT
    void datalink_get_broadcast_address(
        BACNET_ADDRESS * dest);

    BACNET_STACK_EXPORT
    void datalink_get_my_address(
        BACNET_ADDRESS * my_address);

    BACNET_STACK_EXPORT
    int datalink_set(
        char *datalink_string);

    BACNET_STACK_EXPORT
    void datalink_maintenance_timer(uint16_t seconds);
    

#ifdef __cplusplus
}
#endif /* __cplusplus */
