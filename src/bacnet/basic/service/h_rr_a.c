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
/* some demo stuff needed */
#include "bacnet/basic/binding/address.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/basic/object/trendlog.h"

/** @file h_rr_a.c  Handles Read Range Acknowledgments. */

/* for debugging... */
static void PrintReadRangeData(BACNET_READ_RANGE_DATA *data)
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
        printf("{\"list\": [\n");
        for (p = &entry; p != NULL; p = p->next) {
            printf(" [\"");
            object_value.value = &value;
            value.tag = BACNET_APPLICATION_TAG_TIMESTAMP;
            value.type.Time_Stamp.value.dateTime = p->timestamp;
            bacapp_print_value(stdout, &object_value);
            printf("\",\"");

            object_value.value = &p->value;
            bacapp_print_value(stdout, &object_value);

            if (p->next)
                printf("\"],\n");
            else
                printf("\"]\n");
        }
        printf("]}\n");
#endif
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

#if PRINT_ENABLED
    fprintf(stderr, "Received ReadRange Ack!\n");
#endif

    if (len > 0) {
        PrintReadRangeData(&data);
    }
}
