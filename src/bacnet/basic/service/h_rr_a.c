/**************************************************************************
 *
 * Copyright (C) 2009 Steve Karg <skarg@users.sourceforge.net>
 *
 * SPDX-License-Identifier: MIT
 *
 *********************************************************************/
#include <stddef.h>
#include <stdint.h>
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/npdu.h"
#include "bacnet/apdu.h"
#include "bacnet/bactext.h"
#include "bacnet/readrange.h"
#include "bacnet/bacapp_json.h"
/* some demo stuff needed */
#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/basic/sys/debug.h"
#include "bacnet/basic/object/trendlog.h"

/** @file h_rr_a.c  Handles Read Range Acknowledgments. */

/* for Trendlog */
static void PrintReadRangeDataTrendlog(BACNET_READ_RANGE_DATA *data)
{
#ifdef BACAPP_PRINT_ENABLED
    BACNET_OBJECT_PROPERTY_VALUE object_value;
#endif
    BACNET_APPLICATION_DATA_VALUE value;
    BACNET_TRENDLOG_RECORD entry;
    BACNET_TRENDLOG_RECORD *p;
    int status = 0;

    if (data) {
#ifdef BACAPP_PRINT_ENABLED
        object_value.object_type = data->object_type;
        object_value.object_instance = data->object_instance;
        object_value.object_property = data->object_property;
        object_value.array_index = data->array_index;
#endif

        /* FIXME: what if application_data_len is bigger than 255? */
        /* value? need to loop until all of the len is gone... */
        status = rr_decode_trendlog_entries(
            data->application_data, data->application_data_len, &entry);
#ifdef BACAPP_PRINT_ENABLED
        if (status < 1) {
            return;
        }
        //json open list
        fprintf(stdout, "{\n \"list\": [\n");
        for (p = &entry; p != NULL; p = p->next) {
            //json open array
            fprintf(stdout, "  [");
            //print timestamp
            object_value.value = &value;
            value.tag = BACNET_APPLICATION_TAG_TIMESTAMP;
            value.type.Time_Stamp.tag = TIME_STAMP_DATETIME;
            value.type.Time_Stamp.value.dateTime = p->timestamp;
            fprintf(stdout, "\"");
            bacapp_print_value(stdout, &object_value);
            fprintf(stdout, "\",");

            //print log value or status bits [log-disabled, buffer-purged, log-interrupted]
            object_value.value = &p->value;
            bacapp_json_print_value(stdout, &object_value);
            fprintf(stdout, ",");

            //print log status bits [in-alarm, fault, overriden, out-of-service]
            object_value.value = &value;
            value.tag = BACNET_APPLICATION_TAG_BIT_STRING;
            value.type.Bit_String = p->status;
            //TODO replace {} with [] in bacapp_snprintf_value #BACNET_APPLICATION_TAG_BIT_STRING
            bacapp_json_print_value(stdout, &object_value);

            if (p->next)
                fprintf(stdout, "],\n");
            else
                //json last element
                fprintf(stdout, "]\n");
        }
        //json close list
        fprintf(stdout, " ]\n}\n");
#endif
    }
}

/* for debugging... */
static void PrintReadRangeData(BACNET_READ_RANGE_DATA *data)
{
#ifdef BACAPP_PRINT_ENABLED
    BACNET_OBJECT_PROPERTY_VALUE object_value; /* for bacapp printing */
#endif
    BACNET_APPLICATION_DATA_VALUE value = { 0 };
    int len = 0;
    uint8_t *application_data;
    int application_data_len;
    bool first_value = true;
    bool print_brace = false;

    if (data) {
        debug_printf_stdout(
            "%s #%lu\r\n", bactext_object_type_name(data->object_type),
            (unsigned long)data->object_instance);
        debug_printf_stdout("{\r\n");
        if ((data->object_property < 512) ||
            (data->object_property > 4194303)) {
            /* Enumerated values 0-511 and 4194304+ are reserved
               for definition by ASHRAE.*/
            debug_printf_stdout(
                "    %s", bactext_property_name(data->object_property));
        } else {
            /* Enumerated values 512-4194303 may be used
                by others subject to the procedures and
                constraints described in Clause 23. */
            debug_printf_stdout(
                "    proprietary %lu", (unsigned long)data->object_property);
        }
        if (data->array_index == BACNET_ARRAY_ALL) {
            debug_printf_stdout(": ");
        } else {
            debug_printf_stdout("[%lu]: ", (unsigned long)data->array_index);
        }
        application_data = data->application_data;
        application_data_len = data->application_data_len;
        /* loop until all of the len is gone... */
        for (;;) {
            len = bacapp_decode_known_array_property(
                application_data, (uint8_t)application_data_len, &value,
                data->object_type, data->object_property, data->array_index);
            if (len < 0) {
                /* error decoding */
                break;
            }
            if (!first_value) {
                debug_printf_stdout("        ");
            }
            if (first_value && (len < application_data_len)) {
                first_value = false;
                debug_printf_stdout("{");
                print_brace = true;
            }
#ifdef BACAPP_PRINT_ENABLED
            object_value.object_type = data->object_type;
            object_value.object_instance = data->object_instance;
            object_value.object_property = data->object_property;
            object_value.array_index = data->array_index;
            object_value.value = &value;
            bacapp_print_value(stdout, &object_value);
#endif
            if (len > 0) {
                if (len < application_data_len) {
                    application_data += len;
                    application_data_len -= len;
                    /* there's more! */
                    debug_printf_stdout(",\r\n");
                } else {
                    break;
                }
            } else {
                break;
            }
        }
        if (print_brace) {
            debug_printf_stdout("}");
        }
        debug_printf_stdout("\r\n}\r\n");
    }
}

void handler_read_range_ack(
    uint8_t *service_request,
    uint16_t service_len,
    BACNET_ADDRESS *src,
    BACNET_CONFIRMED_SERVICE_ACK_DATA *service_data)
{
    int len = 0;
    BACNET_READ_RANGE_DATA data;

    (void)src;
    (void)service_data; /* we could use these... */
    len = rr_ack_decode_service_request(service_request, service_len, &data);
    if (len > 0) {
        PrintReadRangeData(&data);
    } else {
#if PRINT_ENABLED
        fprintf(stderr, "Received ReadRange Ack!\n");
#endif

    if (len > 0) {
        fprintf(stderr, "Received ReadRange Len: %i\n",len);
        if (data.object_type == OBJECT_TRENDLOG) {
            PrintReadRangeDataTrendlog(&data);
        } else {
            PrintReadRangeData(&data);
        }
    }
}
