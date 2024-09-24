/**
 * @file
 * @brief BACnet application data encode and decode json helper functions
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2007
 * @copyright SPDX-License-Identifier: MIT
 */
#ifndef BACNET_APP_JSON_H
#define BACNET_APP_JSON_H

#include <stdarg.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/bacapp.h"
#include "bacnet/bacaction.h"
#include "bacnet/bacdest.h"
#include "bacnet/bacint.h"
#include "bacnet/bacstr.h"
#include "bacnet/datetime.h"
#include "bacnet/lighting.h"
#include "bacnet/bacdevobjpropref.h"
#include "bacnet/hostnport.h"
#include "bacnet/timestamp.h"
#include "bacnet/weeklyschedule.h"
#include "bacnet/calendar_entry.h"
#include "bacnet/special_event.h"

#ifndef BACAPP_JSON_PRINT_ENABLED
#if PRINT_ENABLED
#define BACAPP_JSON_PRINT_ENABLED
#endif
#endif

/** BACnetScale ::= CHOICE {
        float-scale [0] REAL,
        integer-scale [1] INTEGER
    }
*/

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BACNET_STACK_EXPORT
int bacapp_json_snprintf_value(
    char *str,
    size_t str_len,
    const BACNET_OBJECT_PROPERTY_VALUE *object_value);

BACNET_STACK_EXPORT
bool bacapp_json_print_value(
    FILE *stream, const BACNET_OBJECT_PROPERTY_VALUE *value);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
