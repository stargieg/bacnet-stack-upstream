/**************************************************************************
 *
 * Copyright (C) 2006 Steve Karg <skarg@users.sourceforge.net>
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
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <time.h>
#include "bacnet/config.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/apdu.h"
#include "bacnet/dcc.h"
#include "bacnet/iam.h"
#include "bacnet/npdu.h"
#include "bacnet/getevent.h"
#include "bacnet/version.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/sys/filename.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/datalink/dlenv.h"
#include "bacnet/basic/binding/address.h"
/* include the device object */
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/object/lc.h"
#include "bacnet/basic/object/trendlog.h"
#include "bacnet/basic/object/schedule.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#endif /* defined(INTRINSIC_REPORTING) */

static enum {
    DATALINK_NONE = 0,
    DATALINK_ARCNET,
    DATALINK_ETHERNET,
    DATALINK_BIP,
    DATALINK_BIP6,
    DATALINK_MSTP
} Datalink_Transport;

/** @file server/main.c  Example server application using the BACnet Stack. */

/* (Doxygen note: The next two lines pull all the following Javadoc
 *  into the ServerDemo module.) */
/** @addtogroup ServerDemo */
/*@{*/

/* current version of the BACnet stack */
static const char *BACnet_Version = BACNET_VERSION_TEXT;

/** Buffer used for receiving */
//#define MAX_MPDU BIP_MPDU_MAX
//static uint8_t Rx_Buf[MAX_MPDU] = { 0 };
#if defined(BACDL_ARCNET)
static uint8_t ARC_Rx_Buf[ARCNET_MPDU_MAX] = { 0 };
#endif
#if defined(BACDL_ETHERNET)
static uint8_t ETH_Rx_Buf[ETHERNET_MPDU_MAX] = { 0 };
#endif
#if defined(BACDL_BIP)
static uint8_t BIP_Rx_Buf[BIP_MPDU_MAX] = { 0 };
#endif
#if defined(BACDL_BIP6)
static uint8_t BIP6_Rx_Buf[BIP6_MPDU_MAX] = { 0 };
#endif
#if defined(BACDL_MSTP)
static uint8_t MSTP_Rx_Buf[DLMSTP_MPDU_MAX] = { 0 };
#endif

/** Initialize the handlers we will utilize.
 * @see Device_Init, apdu_set_unconfirmed_handler, apdu_set_confirmed_handler
 */
static void Init_Service_Handlers(void)
{
    Device_Init(NULL);
    /* we need to handle who-is to support dynamic device binding */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);

    /* handle i-am to support binding to other devices */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_I_AM, handler_i_am_bind);

    /* set the handler for all the services we don't implement */
    /* It is required to send the proper reject message... */
    apdu_set_unrecognized_service_handler_handler(handler_unrecognized_service);
    /* Set the handlers for any confirmed services that we support. */
    /* We must implement read property - it's required! */
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROPERTY, handler_read_property);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_PROP_MULTIPLE, handler_read_property_multiple);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_WRITE_PROPERTY, handler_write_property);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_WRITE_PROP_MULTIPLE, handler_write_property_multiple);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_READ_RANGE, handler_read_range);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_REINITIALIZE_DEVICE, handler_reinitialize_device);
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_UTC_TIME_SYNCHRONIZATION, handler_timesync_utc);
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_TIME_SYNCHRONIZATION, handler_timesync);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_SUBSCRIBE_COV, handler_cov_subscribe);
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_COV_NOTIFICATION, handler_ucov_notification);
    /* handle communication so we can shutup when asked */
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL, handler_device_communication_control);
    /* handle the data coming back from private requests */
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_PRIVATE_TRANSFER, handler_unconfirmed_private_transfer);
    /* handle the Simple ack coming back from SubscribeCOV */
    apdu_set_confirmed_simple_ack_handler(
        SERVICE_CONFIRMED_SUBSCRIBE_COV, trend_log_writepropertysimpleackhandler);
    /* handle the data coming back from COV subscriptions */
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_COV_NOTIFICATION, trend_log_confirmed_cov_notification_handler);
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_COV_NOTIFICATION, trend_log_unconfirmed_cov_notification_handler);
#if defined(INTRINSIC_REPORTING)
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_ACKNOWLEDGE_ALARM, handler_alarm_ack);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_GET_EVENT_INFORMATION, handler_get_event_information);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_GET_ALARM_SUMMARY, handler_get_alarm_summary);
#endif /* defined(INTRINSIC_REPORTING) */
#if defined(BACNET_TIME_MASTER)
    handler_timesync_init();
#endif
}

static void print_usage(const char *filename)
{
    printf("Usage: %s\n", filename);
    printf("       [--version]\n");
}

/** Main function of server demo.
 *
 * @see Device_Set_Object_Instance_Number, dlenv_init, Send_I_Am,
 *      datalink_receive, npdu_handler,
 *      dcc_timer_seconds, datalink_maintenance_timer,
 *      Load_Control_State_Machine_Handler, handler_cov_task,
 *      tsm_timer_milliseconds
 *
 * @param argc [in] Arg count.
 * @param argv [in] Takes one argument: the Device Instance #.
 * @return 0 on success.
 */
int main(int argc, char *argv[])
{
    BACNET_ADDRESS src = { 0 }; /* address where message came from */
    uint16_t pdu_len = 0;
    unsigned timeout = 1; /* milliseconds */
    time_t last_seconds = 0;
    time_t current_seconds = 0;
    uint32_t elapsed_seconds = 0;
    uint32_t elapsed_milliseconds = 0;
    uint32_t address_binding_tmr = 0;
#if defined(INTRINSIC_REPORTING)
    uint32_t recipient_scan_tmr = 0;
#endif
#if defined(BACNET_TIME_MASTER)
    BACNET_DATE_TIME bdatetime;
#endif
    int argi = 0;
    const char *filename = NULL;

    filename = filename_remove_path(argv[0]);
    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--help") == 0) {
            print_usage(filename);
            return 0;
        }
        if (strcmp(argv[argi], "--version") == 0) {
            printf("%s %s\n", filename, BACNET_VERSION_TEXT);
            printf("Copyright (C) 2014 by Steve Karg and others.\n"
                   "This is free software; see the source for copying "
                   "conditions.\n"
                   "There is NO warranty; not even for MERCHANTABILITY or\n"
                   "FITNESS FOR A PARTICULAR PURPOSE.\n");
            return 0;
        }
    }
    /* load any static address bindings to show up
       in our device bindings list */
    address_init();
    Init_Service_Handlers();
    printf("BACnet Server Demo\n"
           "BACnet Stack Version %s\n"
           "BACnet Device ID: %u\n"
           "Max APDU: %d\n",
        BACnet_Version, Device_Object_Instance_Number(), MAX_APDU);
    BACNET_CHARACTER_STRING DeviceName;
    if (Device_Object_Name(Device_Object_Instance_Number(), &DeviceName)) {
        printf("BACnet Device Name: %s\n", DeviceName.value);
    }

    Datalink_Transport = dlenv_init();
    atexit(datalink_cleanup);
    /* configure the timeout values */
    last_seconds = time(NULL);
    /* broadcast an I-Am on startup */
    Send_I_Am(&Handler_Transmit_Buffer[0]);
    /* loop forever */
    for (;;) {
        /* input */
        current_seconds = time(NULL);

        /* returns 0 bytes on timeout */
        switch (Datalink_Transport) {
    #if defined(BACDL_ARCNET)
            case DATALINK_ARCNET:
                pdu_len = datalink_receive(&src, &ARC_Rx_Buf[0], ARCNET_MPDU_MAX, timeout);
                break;
    #endif
    #if defined(BACDL_ETHERNET)
            case DATALINK_ETHERNET:
                pdu_len = datalink_receive(&src, &ETH_Rx_Buf[0], ETHERNET_MPDU_MAX, timeout);
                break;
    #endif
    #if defined(BACDL_BIP)
            case DATALINK_BIP:
                pdu_len = datalink_receive(&src, &BIP_Rx_Buf[0], BIP_MPDU_MAX, timeout);
                break;
    #endif
    #if defined(BACDL_BIP6)
            case DATALINK_BIP6:
                pdu_len = datalink_receive(&src, &BIP6_Rx_Buf[0], BIP6_MPDU_MAX, timeout);
                break;
    #endif
    #if defined(BACDL_MSTP)
            case DATALINK_MSTP:
                pdu_len = datalink_receive(&src, &MSTP_Rx_Buf[0], DLMSTP_MPDU_MAX, timeout);
                break;
    #endif
            default:
                break;
        }

        /* process */
        if (pdu_len) {
            switch (Datalink_Transport) {
        #if defined(BACDL_ARCNET)
                case DATALINK_ARCNET:
                    npdu_handler(&src, &ARC_Rx_Buf[0], pdu_len);
                    break;
        #endif
        #if defined(BACDL_ETHERNET)
                case DATALINK_ETHERNET:
                    npdu_handler(&src, &ETH_Rx_Buf[0], pdu_len);
                    break;
        #endif
        #if defined(BACDL_BIP)
                case DATALINK_BIP:
                    npdu_handler(&src, &BIP_Rx_Buf[0], pdu_len);
                    break;
        #endif
        #if defined(BACDL_BIP6)
                case DATALINK_BIP6:
                    npdu_handler(&src, &BIP6_Rx_Buf[0], pdu_len);
                    break;
        #endif
        #if defined(BACDL_MSTP)
                case DATALINK_MSTP:
                    npdu_handler(&src, &MSTP_Rx_Buf[0], pdu_len);
                    break;
        #endif
                default:
                    break;
            }
        }
        /* at least one second has passed */
        elapsed_seconds = (uint32_t)(current_seconds - last_seconds);
        if (elapsed_seconds) {
            last_seconds = current_seconds;
            dcc_timer_seconds(elapsed_seconds);
            datalink_maintenance_timer(elapsed_seconds);
            elapsed_milliseconds = elapsed_seconds * 1000;
            handler_cov_timer_seconds(elapsed_seconds);
            tsm_timer_milliseconds(elapsed_milliseconds);
            trend_log_timer(elapsed_seconds);
            schedule_timer(elapsed_seconds);
#if defined(INTRINSIC_REPORTING)
            Device_local_reporting();
#endif
#if defined(BACNET_TIME_MASTER)
            Device_getCurrentDateTime(&bdatetime);
            handler_timesync_task(&bdatetime);
#endif
        }
        handler_cov_task();
        /* scan cache address */
        address_binding_tmr += elapsed_seconds;
        if (address_binding_tmr >= 60) {
            address_cache_timer(address_binding_tmr);
            address_binding_tmr = 0;
        }
#if defined(INTRINSIC_REPORTING)
        /* try to find addresses of recipients */
        recipient_scan_tmr += elapsed_seconds;
        if (recipient_scan_tmr >= NC_RESCAN_RECIPIENTS_SECS) {
            Notification_Class_find_recipient();
            recipient_scan_tmr = 0;
        }
#endif
        /* output */

        /* blink LEDs, Turn on or off outputs, etc */
    }

    return 0;
}

/* @} */

/* End group ServerDemo */
