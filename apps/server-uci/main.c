/**
 * @file
 * @brief command line tool that simulates a BACnet server device on the
 * network using the BACnet Stack and all the example object types.
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2006
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/apdu.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bactext.h"
#include "bacnet/dcc.h"
#include "bacnet/getevent.h"
#include "bacnet/iam.h"
#include "bacnet/npdu.h"
#include "bacnet/version.h"
/* some demo stuff needed */
#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/sys/filename.h"
#include "bacnet/basic/sys/mstimer.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/datalink/dlenv.h"
#include "bacnet/datetime.h"
/* include the device object */
#include "bacnet/basic/object/device.h"

#if 0
/* objects that have tasks inside them */
#if (BACNET_PROTOCOL_REVISION >= 14)
#include "bacnet/basic/object/lo.h"
#include "bacnet/basic/object/channel.h"
#endif
#if (BACNET_PROTOCOL_REVISION >= 24)
#include "bacnet/basic/object/color_object.h"
#include "bacnet/basic/object/color_temperature.h"
#endif

#endif
#include "bacnet/basic/object/lc.h"
#include "bacnet/basic/object/trendlog.h"
#if 0
#include "bacnet/basic/object/structured_view.h"
#endif
#include "bacnet/basic/object/schedule.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#endif /* defined(INTRINSIC_REPORTING) */
#if defined(BACFILE)
#include "bacnet/basic/object/bacfile.h"
#endif /* defined(BACFILE) */
#if defined(BAC_UCI)
#include "bacnet/basic/ucix/ucix.h"
#endif /* defined(BAC_UCI) */


static enum {
    DATALINK_NONE = 0,
    DATALINK_ARCNET,
    DATALINK_ETHERNET,
    DATALINK_BIP,
    DATALINK_BIP6,
    DATALINK_MSTP,
    DATALINK_BSC
} Datalink_Transport;


/* (Doxygen note: The next two lines pull all the following Javadoc
 *  into the ServerDemo module.) */
/** @addtogroup ServerDemo */
/*@{*/

/* current version of the BACnet stack */
static const char *BACnet_Version = BACNET_VERSION_TEXT;
/* task timer for various BACnet timeouts */
static struct mstimer BACnet_Task_Timer;
/* task timer for TSM timeouts */
static struct mstimer BACnet_TSM_Timer;
/* task timer for address binding timeouts */
static struct mstimer BACnet_Address_Timer;
#if defined(INTRINSIC_REPORTING)
/* task timer for notification recipient timeouts */
static struct mstimer BACnet_Notification_Timer;
#endif
/* task timer for objects */
static struct mstimer BACnet_Object_Timer;
/** Buffer used for receiving */
#if 0
static uint8_t Rx_Buf[MAX_MPDU] = { 0 };
/* configure an example structured view object subordinate list */
#if (BACNET_PROTOCOL_REVISION >= 4)
#define LIGHTING_OBJECT_WATTS OBJECT_ACCUMULATOR
#else
#define LIGHTING_OBJECT_WATTS OBJECT_ANALOG_INPUT
#endif
#if (BACNET_PROTOCOL_REVISION >= 6)
#define LIGHTING_OBJECT_ADR OBJECT_LOAD_CONTROL
#else
#define LIGHTING_OBJECT_ADR OBJECT_MULTISTATE_OUTPUT
#endif
#if (BACNET_PROTOCOL_REVISION >= 6)
#define LIGHTING_OBJECT_ADR OBJECT_LOAD_CONTROL
#else
#define LIGHTING_OBJECT_ADR OBJECT_MULTISTATE_OUTPUT
#endif
#if (BACNET_PROTOCOL_REVISION >= 14)
#define LIGHTING_OBJECT_SCENE OBJECT_CHANNEL
#define LIGHTING_OBJECT_LIGHT OBJECT_LIGHTING_OUTPUT
#else
#define LIGHTING_OBJECT_SCENE OBJECT_ANALOG_VALUE
#define LIGHTING_OBJECT_LIGHT OBJECT_ANALOG_OUTPUT
#endif
#if (BACNET_PROTOCOL_REVISION >= 16)
#define LIGHTING_OBJECT_RELAY OBJECT_BINARY_LIGHTING_OUTPUT
#else
#define LIGHTING_OBJECT_RELAY OBJECT_BINARY_OUTPUT
#endif
static BACNET_SUBORDINATE_DATA Lighting_Subordinate[] = {
    { 0, LIGHTING_OBJECT_WATTS, 1, "watt-hours", 0, 0, NULL },
    { 0, LIGHTING_OBJECT_ADR, 1, "demand-response", 0, 0, NULL },
    { 0, LIGHTING_OBJECT_SCENE, 1, "scene", 0, 0, NULL },
    { 0, LIGHTING_OBJECT_LIGHT, 1, "light", 0, 0, NULL },
    { 0, LIGHTING_OBJECT_RELAY, 1, "relay", 0, 0, NULL },
#if (BACNET_PROTOCOL_REVISION >= 24)
    { 0, OBJECT_COLOR, 1, "color", 0, 0, NULL },
    { 0, OBJECT_COLOR_TEMPERATURE, 1, "color-temperature", 0, 0, NULL },
#endif
};
static BACNET_WRITE_GROUP_NOTIFICATION Write_Group_Notification = { 0 };

/**
 * @brief Update the strcutured view static data with device ID and linked lists
 * @param device_id Device Instance to assign to every subordinate
 */
static void Structured_View_Update(void)
{
    uint32_t device_id, instance;
    BACNET_DEVICE_OBJECT_REFERENCE represents = { 0 };
    size_t i;

    device_id = Device_Object_Instance_Number();
    for (i = 0; i < ARRAY_SIZE(Lighting_Subordinate); i++) {
        /* link the lists */
        if (i < (ARRAY_SIZE(Lighting_Subordinate) - 1)) {
            Lighting_Subordinate[i].next = &Lighting_Subordinate[i + 1];
        }
        /* update the device instance to internal */
        Lighting_Subordinate[i].Device_Instance = device_id;
        /* update the common node data */
        Lighting_Subordinate[i].Node_Type = BACNET_NODE_ROOM;
        Lighting_Subordinate[i].Relationship = BACNET_RELATIONSHIP_CONTAINS;
    }
    instance = Structured_View_Index_To_Instance(0);
    Structured_View_Subordinate_List_Set(instance, Lighting_Subordinate);
    /* In some cases, the Structure View object will abstractly represent
       this entity by itself, and this property will either be absent,
       unconfigured, or point to itself. */
    represents.deviceIdentifier.type = OBJECT_NONE;
    represents.deviceIdentifier.instance = BACNET_MAX_INSTANCE;
    represents.objectIdentifier.type = OBJECT_DEVICE;
    represents.objectIdentifier.instance = Device_Object_Instance_Number();
    Structured_View_Represents_Set(instance, &represents);
    Structured_View_Node_Type_Set(instance, BACNET_NODE_ROOM);
}

#endif
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
#if defined(BACDL_BSC)
static uint8_t BSC_Rx_Buf[BSC_MPDU_MAX] = { 0 };
#endif

/** Initialize the handlers we will utilize.
 * @see Device_Init, apdu_set_unconfirmed_handler, apdu_set_confirmed_handler
 */
static void Init_Service_Handlers(void)
{
#if 0
    BACNET_CREATE_OBJECT_DATA object_data = { 0 };
    unsigned int i = 0;

#endif
    Device_Init(NULL);
#if 0
    /* create some dynamically created objects as examples */
    object_data.object_instance = BACNET_MAX_INSTANCE;
    for (i = 0; i <= BACNET_OBJECT_TYPE_LAST; i++) {
        object_data.object_type = i;
        if (Device_Create_Object(&object_data)) {
            printf(
                "Created object %s-%u\n", bactext_object_type_name(i),
                (unsigned)object_data.object_instance);
        }
    }
    /* update structured view with this device instance */
    Structured_View_Update();
#endif
    /* we need to handle who-is to support dynamic device binding */
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_IS, handler_who_is);
    apdu_set_unconfirmed_handler(SERVICE_UNCONFIRMED_WHO_HAS, handler_who_has);


    /*  BACnet Testing Observed Incident oi00107
        Server only devices should not indicate that they EXECUTE I-Am
        Revealed by BACnet Test Client v1.8.16 ( www.bac-test.com/bacnet-test-client-download )
            BITS: BIT00040
        Any discussions can be directed to edward@bac-test.com
        Please feel free to remove this comment when my changes accepted after suitable time for
        review by all interested parties. Say 6 months -> September 2016 */
    /* In this demo, we are the server only ( BACnet "B" device ) so we do not indicate
       that we can execute the I-Am message */
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
#if defined(BACFILE)
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_ATOMIC_READ_FILE, handler_atomic_read_file);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_ATOMIC_WRITE_FILE, handler_atomic_write_file);
#endif
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
        SERVICE_CONFIRMED_DEVICE_COMMUNICATION_CONTROL,
        handler_device_communication_control);
    /* handle the data coming back from private requests */
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_PRIVATE_TRANSFER,
        handler_unconfirmed_private_transfer);
    /* handle the data coming back from confirmed requests */
    apdu_set_confirmed_ack_handler(
        SERVICE_CONFIRMED_READ_PROPERTY,
        trend_log_read_property_ack_handler);
    /* handle the Simple ack coming back from SubscribeCOV */
    apdu_set_confirmed_simple_ack_handler(
        SERVICE_CONFIRMED_SUBSCRIBE_COV,
        trend_log_writepropertysimpleackhandler);
    /* handle the data coming back from COV subscriptions */
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_COV_NOTIFICATION,
        trend_log_confirmed_cov_notification_handler);
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_COV_NOTIFICATION,
        trend_log_unconfirmed_cov_notification_handler);
#if 0
    apdu_set_unconfirmed_handler(
        SERVICE_UNCONFIRMED_WRITE_GROUP, handler_write_group);
    /* add WriteGroup iterator to the Channel objects */
    Write_Group_Notification.callback = Channel_Write_Group;
    handler_write_group_notification_add(&Write_Group_Notification);
#endif
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
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_CREATE_OBJECT, handler_create_object);
    apdu_set_confirmed_handler(
        SERVICE_CONFIRMED_DELETE_OBJECT, handler_delete_object);
    /* configure the cyclic timers */
    mstimer_set(&BACnet_Task_Timer, 1000UL);
    mstimer_set(&BACnet_TSM_Timer, 50UL);
    mstimer_set(&BACnet_Address_Timer, 60UL * 1000UL);
    mstimer_set(&BACnet_Object_Timer, 100UL);
#if defined(INTRINSIC_REPORTING)
    mstimer_set(&BACnet_Notification_Timer, NC_RESCAN_RECIPIENTS_SECS * 1000UL);
#endif
}

static void print_usage(const char *filename)
{
    printf("Usage: %s\n", filename);
    printf("       [--version]\n");
}

static void print_help(const char *filename)
{
    printf("%s\n", filename);
    printf("Simulate a BACnet server device\n"
            );
}

/** Main function of server demo.
 *
 * @see Device_Set_Object_Instance_Number, dlenv_init, Send_I_Am,
 *      datalink_receive, npdu_handler,
 *      dcc_timer_seconds, datalink_maintenance_timer,
 *      handler_cov_task, tsm_timer_milliseconds
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
    uint32_t elapsed_milliseconds = 0;
    uint32_t elapsed_seconds = 0;
    BACNET_CHARACTER_STRING DeviceName;
#if defined(BACNET_TIME_MASTER)
    BACNET_DATE_TIME bdatetime;
#endif
#if 0
#if defined(BAC_UCI)
    int uciId = 0;
    struct uci_context *ctx;
#endif
#endif

    int argi = 0;
    const char *filename = NULL;

    filename = filename_remove_path(argv[0]);
    for (argi = 1; argi < argc; argi++) {
        if (strcmp(argv[argi], "--help") == 0) {
            print_usage(filename);
            print_help(filename);
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
#if 0
#if defined(BAC_UCI)
    ctx = ucix_init("bacnet_dev");
    if (!ctx) {
        fprintf(stderr, "Failed to load config file bacnet_dev\n");
    }
    uciId = ucix_get_option_int(ctx, "bacnet_dev", "0", "Id", 0);
    if (uciId != 0) {
        Device_Set_Object_Instance_Number(uciId);
    } else {
#endif /* defined(BAC_UCI) */
        /* allow the device ID to be set */
        if (argc > 1) {
            Device_Set_Object_Instance_Number(strtol(argv[1], NULL, 0));
        }

#if defined(BAC_UCI)
    }
    ucix_cleanup(ctx);
#endif /* defined(BAC_UCI) */
#endif

    printf(
        "BACnet Server uci\n"
        "BACnet Stack Version %s\n",
        BACnet_Version);
    /* load any static address bindings to show up
       in our device bindings list */
    address_init();
    Init_Service_Handlers();
    printf(
        "BACnet Device ID: %u\n",
         Device_Object_Instance_Number());
    /* initialize timesync callback function. */
    handler_timesync_set_callback_set(&datetime_timesync);
#if 0
#if defined(BAC_UCI)
    const char *uciname;
    ctx = ucix_init("bacnet_dev");
    if (!ctx) {
        fprintf(stderr, "Failed to load config file bacnet_dev\n");
    }
    uciname = ucix_get_option(ctx, "bacnet_dev", "0", "Name");
    if (uciname != 0) {
        Device_Object_Name_ANSI_Init(uciname);
    } else {
#endif /* defined(BAC_UCI) */
        if (argc > 2) {
            Device_Object_Name_ANSI_Init(argv[2]);
        }
#if defined(BAC_UCI)
    }
    ucix_cleanup(ctx);
#endif /* defined(BAC_UCI) */
#endif

    if (Device_Object_Name(Device_Object_Instance_Number(), &DeviceName)) {
        printf("BACnet Device Name: %s\n", DeviceName.value);
    }

    Datalink_Transport = dlenv_init();
    printf("Datalink Interface: %s ", datalink_get_interface());
    switch (Datalink_Transport) {
        case DATALINK_ARCNET:
#if defined(BACDL_ARCNET)
            printf("Arcnet Max APDU: %d\n", ARCNET_MPDU_MAX);
#endif
            break;
        case DATALINK_ETHERNET:
#if defined(BACDL_ETHERNET)
            printf("Ethernet Max APDU: %d\n", ETHERNET_MPDU_MAX);
#endif
            break;
        case DATALINK_BIP:
#if defined(BACDL_BIP)
            printf("IPv4 Max APDU: %d\n", BIP_MPDU_MAX);
#endif
            break;
        case DATALINK_BIP6:
#if defined(BACDL_BIP6)
            printf("IPv6 Max APDU: %d\n", BIP6_MPDU_MAX);
#endif
            break;
        case DATALINK_MSTP:
#if defined(BACDL_MSTP)
            printf("MSTP Max APDU: %d\n", DLMSTP_MPDU_MAX);
#endif
            break;
        case DATALINK_BSC:
#if defined(BACDL_BSC)
            printf("BSC Socket Max APDU: %d\n", BSC_MPDU_MAX);
#endif
            break;
        default:
            printf("No Datalink APDU: %d\n", MAX_APDU);
            break;
        }

    atexit(datalink_cleanup);
    /* broadcast an I-Am on startup */
    Send_I_Am(&Handler_Transmit_Buffer[0]);
    /* loop forever */
    for (;;) {
        /* input */
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
#if defined(BACDL_BSC)
            case DATALINK_BSC:
                pdu_len = datalink_receive(&src, &BSC_Rx_Buf[0], BSC_MPDU_MAX, timeout);
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
#if defined(BACDL_BSC)
                case DATALINK_BSC:
                    npdu_handler(&src, &BSC_Rx_Buf[0], pdu_len);
                    break;
#endif
                default:
                    break;
            }
        }
        if (mstimer_expired(&BACnet_Task_Timer)) {
            mstimer_reset(&BACnet_Task_Timer);
            elapsed_milliseconds = mstimer_interval(&BACnet_Task_Timer);
            elapsed_seconds = elapsed_milliseconds / 1000;
            /* 1 second tasks */
            dcc_timer_seconds(elapsed_seconds);
            datalink_maintenance_timer(elapsed_seconds);
            dlenv_maintenance_timer(elapsed_seconds);
            handler_cov_timer_seconds(elapsed_seconds);
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
        if (mstimer_expired(&BACnet_TSM_Timer)) {
            mstimer_reset(&BACnet_TSM_Timer);
            elapsed_milliseconds = mstimer_interval(&BACnet_TSM_Timer);
            tsm_timer_milliseconds(elapsed_milliseconds);
        }
        if (mstimer_expired(&BACnet_Address_Timer)) {
            mstimer_reset(&BACnet_Address_Timer);
            elapsed_milliseconds = mstimer_interval(&BACnet_Address_Timer);
            elapsed_seconds = elapsed_milliseconds / 1000;
            address_cache_timer(elapsed_seconds);
        }
        handler_cov_task();
#if defined(INTRINSIC_REPORTING)
        if (mstimer_expired(&BACnet_Notification_Timer)) {
            mstimer_reset(&BACnet_Notification_Timer);
            Notification_Class_find_recipient();
        }
#endif
        /* output */
        if (mstimer_expired(&BACnet_Object_Timer)) {
            mstimer_reset(&BACnet_Object_Timer);
            elapsed_milliseconds = mstimer_interval(&BACnet_Object_Timer);
            Device_Timer(elapsed_milliseconds);
        }
    }

    return 0;
}

/* @} */

/* End group ServerDemo */
