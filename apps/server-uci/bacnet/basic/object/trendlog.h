/**************************************************************************
*
* Copyright (C) 2009 Peter Mc Shane
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
#ifndef TRENDLOG_H
#define TRENDLOG_H

#include <stdbool.h>
#include <stdint.h>
#include "bacnet/bacnet_stack_exports.h"
#include "bacnet/bacdef.h"
#include "bacnet/cov.h"
#include "bacnet/datetime.h"
#include "bacnet/rp.h"
#include "bacnet/wp.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Error code for Trend Log storage */
    typedef struct tl_error {
        uint16_t usClass;
        uint16_t usCode;
    } TL_ERROR;

/* Bit string of up to 32 bits for Trend Log storage */

    typedef struct tl_bits {
        uint8_t ucLen;  /* bytes used in upper nibble/bits free in lower nibble */
        uint8_t ucStore[4];
    } TL_BITS;


#define TL_T_START_WILD 1       /* Start time is wild carded */
#define TL_T_STOP_WILD  2       /* Stop Time is wild carded */

#define TL_MAX_ENTRIES 1000     /* Entries per datalog */

/*
 * Data types associated with a BACnet Log Record. We use these for managing the
 * log buffer but they are also the tag numbers to use when encoding/decoding
 * the log datum field.
 */

#define TL_TYPE_STATUS  0
#define TL_TYPE_BOOL    1
#define TL_TYPE_REAL    2
#define TL_TYPE_ENUM    3
#define TL_TYPE_UNSIGN  4
#define TL_TYPE_SIGN    5
#define TL_TYPE_BITS    6
#define TL_TYPE_NULL    7
#define TL_TYPE_ERROR   8
#define TL_TYPE_DELTA   9
#define TL_TYPE_ANY     10      /* We don't support this particular can of worms! */


    BACNET_STACK_EXPORT
    void Trend_Log_Property_Lists(
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);

    BACNET_STACK_EXPORT
    bool Trend_Log_Valid_Instance(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    unsigned Trend_Log_Count(
        void);
    BACNET_STACK_EXPORT
    uint32_t Trend_Log_Index_To_Instance(
        unsigned index);
    BACNET_STACK_EXPORT
    unsigned Trend_Log_Instance_To_Index(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Trend_Log_Object_Instance_Add(
        uint32_t instance);

    BACNET_STACK_EXPORT
    bool Trend_Log_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);

    BACNET_STACK_EXPORT
    int Trend_Log_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata);

    BACNET_STACK_EXPORT
    bool Trend_Log_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data);
    BACNET_STACK_EXPORT
    void Trend_Log_Init(
        void);

    BACNET_STACK_EXPORT
    void TL_Insert_Status_Rec(
        int iLog,
        BACNET_LOG_STATUS eStatus,
        bool bState);

    BACNET_STACK_EXPORT
    bool TL_Is_Enabled(
        int iLog);

    BACNET_STACK_EXPORT
    bacnet_time_t TL_BAC_Time_To_Local(
        BACNET_DATE_TIME * SourceTime);

    BACNET_STACK_EXPORT
    void TL_Local_Time_To_BAC(
        BACNET_DATE_TIME * DestTime,
        bacnet_time_t SourceTime);

    BACNET_STACK_EXPORT
    int TL_encode_entry(
        uint8_t * apdu,
        int iLog,
        int iEntry);

    BACNET_STACK_EXPORT
    int TL_encode_by_position(
        uint8_t * apdu,
        BACNET_READ_RANGE_DATA * pRequest);

    BACNET_STACK_EXPORT
    int TL_encode_by_sequence(
        uint8_t * apdu,
        BACNET_READ_RANGE_DATA * pRequest);

    BACNET_STACK_EXPORT
    int TL_encode_by_time(
        uint8_t * apdu,
        BACNET_READ_RANGE_DATA * pRequest);

    BACNET_STACK_EXPORT
    bool TrendLogGetRRInfo(
        BACNET_READ_RANGE_DATA * pRequest,      /* Info on the request */
        RR_PROP_INFO * pInfo);  /* Where to put the information */

    BACNET_STACK_EXPORT
    int rr_trend_log_encode(
        uint8_t * apdu,
        BACNET_READ_RANGE_DATA * pRequest);

    BACNET_STACK_EXPORT
    void trend_log_timer(
        uint16_t uSeconds);

    BACNET_STACK_EXPORT
    void trend_log_writepropertysimpleackhandler(
    BACNET_ADDRESS *src, uint8_t invoke_id);

    BACNET_STACK_EXPORT
    void trend_log_unconfirmed_cov_notification_handler(
    uint8_t *service_request, uint16_t service_len, BACNET_ADDRESS *src);

    BACNET_STACK_EXPORT
    void trend_log_confirmed_cov_notification_handler(uint8_t *service_request,
    uint16_t service_len,
    BACNET_ADDRESS *src,
    BACNET_CONFIRMED_SERVICE_DATA *service_data);

    BACNET_STACK_EXPORT
    void trend_log_read_property_ack_handler(uint8_t *service_request,
    uint16_t service_len,
    BACNET_ADDRESS *src,
    BACNET_CONFIRMED_SERVICE_ACK_DATA *service_data);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
