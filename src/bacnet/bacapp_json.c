/**
 * @file
 * @brief Utilities for the BACnet_Application_Data_Value with json
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2005
 * @copyright SPDX-License-Identifier: GPL-2.0-or-later WITH GCC-exception-2.0
 */
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h> /* for strtol */
#include <ctype.h> /* for isalnum */
#include <errno.h>
#include <math.h>
#if (__STDC_VERSION__ >= 199901L) && defined(__STDC_ISO_10646__)
#include <wchar.h>
#include <wctype.h>
#endif
#include "bacnet/bacenum.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacint.h"
#include "bacnet/bacreal.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacapp.h"
#include "bacnet/bactext.h"
#include "bacnet/datetime.h"
#include "bacnet/bacstr.h"
#include "bacnet/bacaction.h"
#include "bacnet/lighting.h"
#include "bacnet/hostnport.h"
#include "bacnet/weeklyschedule.h"
#include "bacnet/calendar_entry.h"
#include "bacnet/special_event.h"
#include "bacnet/basic/sys/platform.h"
#include "bacnet/bacapp.h"

/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param property - property identifier
 * @return number of characters written
 */
static int bacapp_snprintf_property_identifier(
    char *str, size_t str_len, BACNET_PROPERTY_ID property)
{
    int ret_val = 0;
    const char *char_str;

    char_str = bactext_property_name_default(property, NULL);
    if (char_str) {
        ret_val = bacapp_snprintf(str, str_len, "%s", char_str);
    } else {
        ret_val = bacapp_snprintf(str, str_len, "%lu", (unsigned long)property);
    }

    return ret_val;
}

#if defined(BACAPP_NULL)
/**
 * @brief Print an null value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @return number of characters written
 */
static int bacapp_snprintf_null(char *str, size_t str_len)
{
    return bacapp_snprintf(str, str_len, "Null");
}
#endif

#if defined(BACAPP_BOOLEAN)
/**
 * @brief Print an boolean value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_boolean(char *str, size_t str_len, bool value)
{
    if (value) {
        return bacapp_snprintf(str, str_len, "TRUE");
    } else {
        return bacapp_snprintf(str, str_len, "FALSE");
    }
}
#endif
#if defined(BACAPP_UNSIGNED)
/**
 * @brief Print an unsigned integer value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_unsigned_integer(
    char *str, size_t str_len, BACNET_UNSIGNED_INTEGER value)
{
    return bacapp_snprintf(str, str_len, "%lu", (unsigned long)value);
}
#endif

#if defined(BACAPP_SIGNED)
/**
 * @brief Print an signed integer value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int
bacapp_snprintf_signed_integer(char *str, size_t str_len, int32_t value)
{
    return bacapp_snprintf(str, str_len, "%ld", (long)value);
}
#endif

#if defined(BACAPP_REAL)
/**
 * @brief Print an real value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_real(char *str, size_t str_len, float value)
{
    return bacapp_snprintf(str, str_len, "%f", (double)value);
}
#endif
#if defined(BACAPP_DOUBLE)
/**
 * @brief Print an double value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_double(char *str, size_t str_len, double value)
{
    return bacapp_snprintf(str, str_len, "%f", value);
}
#endif

#if 0
/**
 * @brief Print an octet string value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - octet string value to print
 * @return number of characters written
 */
int bacapp_snprintf_octet_string(
    char *str, size_t str_len, const BACNET_OCTET_STRING *value)
{
    int ret_val = 0;
    int len = 0;
    int slen = 0;
    int i = 0;
    const uint8_t *octet_str;

    slen = bacapp_snprintf(str, str_len, "X'");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    len = octetstring_length(value);
    if (len > 0) {
        octet_str = octetstring_value_const(value);
        for (i = 0; i < len; i++) {
            slen = bacapp_snprintf(str, str_len, "%02X", *octet_str);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            octet_str++;
        }
    }
    slen = bacapp_snprintf(str, str_len, "'");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif
#if 0
/**
 * @brief Print a character string value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - character string value to print
 * @return number of characters written
 */
int bacapp_snprintf_character_string(
    char *str, size_t str_len, const BACNET_CHARACTER_STRING *value)
{
    int ret_val = 0;
    int len = 0;
    int slen = 0;
    int i = 0;
    const char *char_str;
#if (__STDC_VERSION__ >= 199901L) && defined(__STDC_ISO_10646__)
    /* Wide character (decoded from multi-byte character). */
    wchar_t wc;
    /* Wide character length in bytes. */
    int wclen;
#endif

    len = characterstring_length(value);
    char_str = characterstring_value_const(value);
    slen = bacapp_snprintf(str, str_len, "\"");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
#if (__STDC_VERSION__ >= 199901L) && defined(__STDC_ISO_10646__)
    if (characterstring_encoding(value) == CHARACTER_UTF8) {
        while (len > 0) {
            wclen = mbtowc(&wc, char_str, MB_CUR_MAX);
            if (wclen == -1) {
                /* Encoding error, reset state: */
                mbtowc(NULL, NULL, MB_CUR_MAX);
                /* After handling an invalid byte,
                    retry with the next one. */
                wclen = 1;
                wc = L'?';
            } else if (wclen == 0) {
                /* Null wide character */
                wc = L'.';
                wclen = 1;
            } else {
                if (!iswprint(wc)) {
                    wc = L'.';
                }
            }
            /* For portability, cast wchar_t to wint_t */
            slen = bacapp_snprintf(str, str_len, "%lc", (wint_t)wc);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            if (len > wclen) {
                len -= wclen;
                char_str += wclen;
            } else {
                len = 0;
            }
        }
    } else
#endif
    {
        for (i = 0; i < len; i++) {
            if (isprint(*((const unsigned char *)char_str))) {
                slen = bacapp_snprintf(str, str_len, "%c", *char_str);
            } else {
                slen = bacapp_snprintf(str, str_len, "%c", '.');
            }
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            char_str++;
        }
    }
    slen = bacapp_snprintf(str, str_len, "\"");
    ret_val += slen;

    return ret_val;
}
#endif

#if defined(BACAPP_BIT_STRING)
/**
 * @brief Print a bit string value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - bit string value to print
 * @return number of characters written
 */
static int bacapp_snprintf_bit_string(
    char *str, size_t str_len, const BACNET_BIT_STRING *value)
{
    int ret_val = 0;
    int len = 0;
    int slen = 0;
    int i = 0;

    len = bitstring_bits_used(value);
    slen = bacapp_snprintf(str, str_len, "[");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    for (i = 0; i < len; i++) {
        bool bit;
        bit = bitstring_bit(value, (uint8_t)i);
        slen = bacapp_snprintf(str, str_len, "%s", bit ? "true" : "false");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        if (i < (len - 1)) {
            slen = bacapp_snprintf(str, str_len, ",");
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        }
    }
    slen = bacapp_snprintf(str, str_len, "]");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_ENUMERATED)
/**
 * @brief Print an enumerated value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param object_type - object type identifier
 * @param property - object property identifier
 * @param value - enumerated value to print
 * @return number of characters written
 */
static int bacapp_snprintf_enumerated(
    char *str,
    size_t str_len,
    BACNET_OBJECT_TYPE object_type,
    BACNET_PROPERTY_ID property,
    uint32_t value)
{
    int ret_val = 0;

    switch (property) {
        case PROP_PROPERTY_LIST:
            ret_val = bacapp_snprintf_property_identifier(str, str_len, value);
            break;
        case PROP_OBJECT_TYPE:
            if (value <= BACNET_OBJECT_TYPE_RESERVED_MIN) {
                ret_val = bacapp_snprintf(
                    str, str_len, "\"%s\"", bactext_object_type_name(value));
            } else if (value <= BACNET_OBJECT_TYPE_RESERVED_MAX) {
                ret_val = bacapp_snprintf(
                    str, str_len, "\"reserved %lu\"", (unsigned long)value);
            } else {
                ret_val = bacapp_snprintf(
                    str, str_len, "\"proprietary-%lu\"", (unsigned long)value);
            }
            break;
        case PROP_EVENT_STATE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_event_state_name(value));
            break;
        case PROP_UNITS:
        case PROP_CONTROLLED_VARIABLE_UNITS:
        case PROP_DERIVATIVE_CONSTANT_UNITS:
        case PROP_INTEGRAL_CONSTANT_UNITS:
        case PROP_PROPORTIONAL_CONSTANT_UNITS:
        case PROP_OUTPUT_UNITS:
        case PROP_CAR_LOAD_UNITS:
            if (bactext_engineering_unit_name_proprietary((unsigned)value)) {
                ret_val = bacapp_snprintf(
                    str, str_len, "\"proprietary-%lu\"", (unsigned long)value);
            } else {
                ret_val = bacapp_snprintf(
                    str, str_len, "\"%s\"", bactext_engineering_unit_name(value));
            }
            break;
        case PROP_POLARITY:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_binary_polarity_name(value));
            break;
        case PROP_PRESENT_VALUE:
        case PROP_RELINQUISH_DEFAULT:
        case PROP_FEEDBACK_VALUE:
            switch (object_type) {
                case OBJECT_BINARY_INPUT:
                case OBJECT_BINARY_OUTPUT:
                case OBJECT_BINARY_VALUE:
                    ret_val = bacapp_snprintf(
                        str, str_len, "\"%s\"",
                        bactext_binary_present_value_name(value));
                    break;
                case OBJECT_BINARY_LIGHTING_OUTPUT:
                    ret_val = bacapp_snprintf(
                        str, str_len, "\"%s\"",
                        bactext_binary_lighting_pv_name(value));
                    break;
                case OBJECT_ACCESS_DOOR:
                    ret_val = bacapp_snprintf(
                        str, str_len, "%s", bactext_door_value_name(value));
                    break;
                default:
                    ret_val = bacapp_snprintf(
                        str, str_len, "%lu", (unsigned long)value);
                    break;
            }
            break;
        case PROP_RELIABILITY:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_reliability_name(value));
            break;
        case PROP_SYSTEM_STATUS:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_device_status_name(value));
            break;
        case PROP_SEGMENTATION_SUPPORTED:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_segmentation_name(value));
            break;
        case PROP_NODE_TYPE:
        case PROP_SUBORDINATE_NODE_TYPES:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_node_type_name(value));
            break;
        case PROP_SUBORDINATE_RELATIONSHIPS:
        case PROP_DEFAULT_SUBORDINATE_RELATIONSHIP:
            if (bactext_node_relationship_name_proprietary((unsigned)value)) {
                ret_val = bacapp_snprintf(
                    str, str_len, "proprietary-%lu", (unsigned long)value);
            } else {
                ret_val = bacapp_snprintf(
                    str, str_len, "%s", bactext_node_relationship_name(value));
            }
            break;
        case PROP_TRANSITION:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_lighting_transition(value));
            break;
        case PROP_IN_PROGRESS:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_lighting_in_progress(value));
            break;
        case PROP_LOGGING_TYPE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_logging_type_name(value));
            break;
        case PROP_MODE:
        case PROP_ACCEPTED_MODES:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_life_safety_mode_name(value));
            break;
        case PROP_OPERATION_EXPECTED:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_life_safety_operation_name(value));
            break;
        case PROP_TRACKING_VALUE:
            switch (object_type) {
                case OBJECT_LIFE_SAFETY_POINT:
                case OBJECT_LIFE_SAFETY_ZONE:
                    ret_val = bacapp_snprintf(
                        str, str_len, "\"%s\"",
                        bactext_life_safety_state_name(value));
                    break;
                default:
                    ret_val = bacapp_snprintf(
                        str, str_len, "%lu", (unsigned long)value);
                    break;
            }
            break;
        case PROP_PROGRAM_CHANGE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_program_request_name(value));
            break;
        case PROP_PROGRAM_STATE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_program_state_name(value));
            break;
        case PROP_REASON_FOR_HALT:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_program_error_name(value));
            break;
        case PROP_NETWORK_NUMBER_QUALITY:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_network_number_quality_name(value));
            break;
        case PROP_NETWORK_TYPE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_network_port_type_name(value));
            break;
        case PROP_PROTOCOL_LEVEL:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_protocol_level_name(value));
            break;
        case PROP_EVENT_TYPE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_event_type_name(value));
            break;
        case PROP_NOTIFY_TYPE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_notify_type_name(value));
            break;
        case PROP_TIMER_STATE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_timer_state_name(value));
            break;
        case PROP_LAST_STATE_CHANGE:
            ret_val = bacapp_snprintf(
                str, str_len, "\"%s\"", bactext_timer_transition_name(value));
            break;
        case PROP_ACTION:
            ret_val =
                bacapp_snprintf(str, str_len, "%s", bactext_action_name(value));
            break;
        case PROP_FILE_ACCESS_METHOD:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_file_access_method_name(value));
            break;
        case PROP_LOCK_STATUS:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_lock_status_name(value));
            break;
        case PROP_DOOR_ALARM_STATE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_door_alarm_state_name(value));
            break;
        case PROP_DOOR_STATUS:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_door_status_name(value));
            break;
        case PROP_SECURED_STATUS:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_door_secured_status_name(value));
            break;
        case PROP_ACCESS_EVENT:
        case PROP_LAST_ACCESS_EVENT:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_access_event_name(value));
            break;
        case PROP_AUTHENTICATION_STATUS:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_authentication_status_name(value));
            break;
        case PROP_AUTHORIZATION_MODE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_authorization_mode_name(value));
            break;
        case PROP_CREDENTIAL_STATUS:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_binary_present_value_name(value));
            break;
        case PROP_CREDENTIAL_DISABLE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s",
                bactext_access_credential_disable_name(value));
            break;
        case PROP_REASON_FOR_DISABLE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s",
                bactext_access_credential_disable_reason_name(value));
            break;
        case PROP_USER_TYPE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_access_user_type_name(value));
            break;
        case PROP_OCCUPANCY_STATE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s",
                bactext_access_zone_occupancy_state_name(value));
            break;
        case PROP_SILENCED:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_silenced_state_name(value));
            break;
        case PROP_WRITE_STATUS:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_write_status_name(value));
            break;
        case PROP_BACNET_IP_MODE:
        case PROP_BACNET_IPV6_MODE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_ip_mode_name(value));
            break;
        case PROP_SC_HUB_CONNECTOR_STATE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_sc_hub_connector_state_name(value));
            break;
        case PROP_MAINTENANCE_REQUIRED:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_maintenance_name(value));
            break;
        case PROP_FAULT_SIGNALS:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_escalator_fault_name(value));
            break;
        case PROP_OPERATION_DIRECTION:
            ret_val = bacapp_snprintf(
                str, str_len, "%s",
                bactext_escalator_operation_direction_name(value));
            break;
        case PROP_BACKUP_AND_RESTORE_STATE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_backup_state_name(value));
            break;
        case PROP_BASE_DEVICE_SECURITY_POLICY:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_security_level_name(value));
            break;
        case PROP_GROUP_MODE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_lift_group_mode_name(value));
            break;
        case PROP_CAR_MODE:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_lift_car_mode_name(value));
            break;
        case PROP_CAR_DRIVE_STATUS:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_lift_car_drive_status_name(value));
            break;
        case PROP_CAR_DOOR_COMMAND:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_lift_car_door_command_name(value));
            break;
        case PROP_CAR_ASSIGNED_DIRECTION:
        case PROP_CAR_MOVING_DIRECTION:
            ret_val = bacapp_snprintf(
                str, str_len, "%s", bactext_lift_car_direction_name(value));
            break;
        default:
            ret_val =
                bacapp_snprintf(str, str_len, "%lu", (unsigned long)value);
            break;
    }

    return ret_val;
}
#endif

#if defined(BACAPP_DATE)
/**
 * @brief Print a date value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param bdate - date value to print
 * @return number of characters written
 * @note 135.1-4.4 Notational Rules for Parameter Values
 * (j) dates are represented enclosed in parenthesis:
 *     (Monday, 24-January-1998).
 *     Any "wild card" or unspecified field is shown by an asterisk (X'2A'):
 *     (Monday, *-January-1998).
 *     The omission of day of week implies that the day is unspecified:
 *     (24-January-1998);
 */
static int
bacapp_snprintf_date(char *str, size_t str_len, const BACNET_DATE *bdate)
{
    int ret_val = 0;
    int slen = 0;
    const char *weekday_text, *month_text;

    weekday_text = bactext_day_of_week_name(bdate->wday);
    month_text = bactext_month_name(bdate->month);
    slen = bacapp_snprintf(str, str_len, "%s, %s", weekday_text, month_text);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (bdate->day == 255) {
        slen = bacapp_snprintf(str, str_len, " (unspecified), ");
    } else {
        slen = bacapp_snprintf(str, str_len, " %u, ", (unsigned)bdate->day);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (bdate->year == 2155) {
        slen = bacapp_snprintf(str, str_len, "(unspecified)");
    } else {
        slen = bacapp_snprintf(str, str_len, "%u", (unsigned)bdate->year);
    }
    ret_val += slen;

    return ret_val;
}
#endif

#if defined(BACAPP_LOG_RECORD)
/**
 * @brief Print a date value to a string as numeric
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param bdate - date value to print
 * @return number of characters written
 * @note Numeric will be in the format: yyyy-mm-dd
 */
static int bacapp_snprintf_date_numeric(
    char *str, size_t str_len, const BACNET_DATE *bdate)
{
    int ret_val = 0;
    int slen = 0;

    if (bdate->year == 2155) {
        slen = bacapp_snprintf(str, str_len, "****-");
    } else {
        slen = bacapp_snprintf(str, str_len, "%04u-", (unsigned)bdate->year);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (bdate->month == 255) {
        slen = bacapp_snprintf(str, str_len, "**-");
    } else {
        slen = bacapp_snprintf(str, str_len, "%02u-", (unsigned)bdate->month);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (bdate->day == 255) {
        slen = bacapp_snprintf(str, str_len, "**");
    } else {
        slen = bacapp_snprintf(str, str_len, "%02u", (unsigned)bdate->day);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_TIME)
/**
 * @brief Print a time value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param btime - date value to print
 * @return number of characters written
 * @note 135.1-4.4 Notational Rules for Parameter Values
 * (k) times are represented as hours, minutes, seconds, hundredths
 *     in the format hh:mm:ss.xx: 2:05:44.00, 16:54:59.99.
 *     Any "wild card" field is shown by an asterisk (X'2A'): 16:54:*.*;
 */
static int
bacapp_snprintf_time(char *str, size_t str_len, const BACNET_TIME *btime)
{
    int ret_val = 0;
    int slen = 0;

    if (btime->hour == 255) {
        slen = bacapp_snprintf(str, str_len, "**:");
    } else {
        slen = bacapp_snprintf(str, str_len, "%02u:", (unsigned)btime->hour);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (btime->min == 255) {
        slen = bacapp_snprintf(str, str_len, "**:");
    } else {
        slen = bacapp_snprintf(str, str_len, "%02u:", (unsigned)btime->min);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (btime->sec == 255) {
        slen = bacapp_snprintf(str, str_len, "**.");
    } else {
        slen = bacapp_snprintf(str, str_len, "%02u.", (unsigned)btime->sec);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (btime->hundredths == 255) {
        slen = bacapp_snprintf(str, str_len, "**");
    } else {
        slen =
            bacapp_snprintf(str, str_len, "%02u", (unsigned)btime->hundredths);
    }
    ret_val += slen;

    return ret_val;
}
#endif

#if defined(BACAPP_OBJECT_ID) || defined(BACAPP_DEVICE_OBJECT_REFERENCE)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_object_id(
    char *str, size_t str_len, const BACNET_OBJECT_ID *object_id)
{
    int ret_val = 0;
    int slen = 0;

    slen = bacapp_snprintf(str, str_len, "(");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (object_id->type <= BACNET_OBJECT_TYPE_RESERVED_MIN) {
        slen = bacapp_snprintf(
            str, str_len, "%s, ", bactext_object_type_name(object_id->type));
    } else if (object_id->type < BACNET_OBJECT_TYPE_RESERVED_MAX) {
        slen = bacapp_snprintf(
            str, str_len, "reserved %u, ", (unsigned)object_id->type);
    } else {
        slen = bacapp_snprintf(
            str, str_len, "proprietary %u, ", (unsigned)object_id->type);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(
        str, str_len, "%lu)", (unsigned long)object_id->instance);
    ret_val += slen;

    return ret_val;
}
#endif

#if defined(BACAPP_DATETIME)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_datetime(
    char *str, size_t str_len, const BACNET_DATE_TIME *value)
{
    int ret_val = 0;
    int slen = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf_date(str, str_len, &value->date);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, "-");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf_time(str, str_len, &value->time);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    ret_val += bacapp_snprintf(str, str_len, "}");

    return ret_val;
}
#endif

#if defined(BACAPP_LOG_RECORD)
/**
 * @brief Print a value to a string as numeric
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_datetime_numeric(
    char *str, size_t str_len, const BACNET_DATE_TIME *value)
{
    int ret_val = 0;
    int slen = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf_date_numeric(str, str_len, &value->date);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, "-");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf_time(str, str_len, &value->time);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    ret_val += bacapp_snprintf(str, str_len, "}");

    return ret_val;
}
#endif

#if defined(BACAPP_DATERANGE) || defined(BACAPP_CALENDAR_ENTRY)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_daterange(
    char *str, size_t str_len, const BACNET_DATE_RANGE *value)
{
    int ret_val = 0;
    int slen = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf_date(str, str_len, &value->startdate);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, "..");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf_date(str, str_len, &value->enddate);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    ret_val += bacapp_snprintf(str, str_len, "}");

    return ret_val;
}
#endif

#if defined(BACAPP_SPECIAL_EVENT) || defined(BACAPP_CALENDAR_ENTRY)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 * (j) dates are represented enclosed in parenthesis:
 *     (Monday, 24-January-1998).
 *     Any "wild card" or unspecified field is shown by an asterisk (X'2A'):
 *     (Monday, *-January-1998).
 *     The omission of day of week implies that the day is unspecified:
 *     (24-January-1998);
 */
static int bacapp_snprintf_weeknday(
    char *str, size_t str_len, const BACNET_WEEKNDAY *value)
{
    int ret_val = 0;
    int slen = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* 1=Jan 13=odd 14=even FF=any */
    if (value->month == 255) {
        slen = bacapp_snprintf(str, str_len, "*, ");
    } else if (value->month == 13) {
        slen = bacapp_snprintf(str, str_len, "odd, ");
    } else if (value->month == 14) {
        slen = bacapp_snprintf(str, str_len, "even, ");
    } else {
        slen = bacapp_snprintf(str, str_len, "%u, ", (unsigned)value->month);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* 1=days 1-7, 2=days 8-14, 3=days 15-21, 4=days 22-28,
       5=days 29-31, 6=last 7 days, FF=any week */
    if (value->weekofmonth == 255) {
        slen = bacapp_snprintf(str, str_len, "*, ");
    } else if (value->weekofmonth == 6) {
        slen = bacapp_snprintf(str, str_len, "last, ");
    } else {
        slen =
            bacapp_snprintf(str, str_len, "%u, ", (unsigned)value->weekofmonth);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* 1=Monday-7=Sunday, FF=any */
    if (value->dayofweek == 255) {
        slen = bacapp_snprintf(str, str_len, "*");
    } else {
        slen = bacapp_snprintf(
            str, str_len, "%s", bactext_day_of_week_name(value->dayofweek));
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    ret_val += bacapp_snprintf(str, str_len, "}");

    return ret_val;
}
#endif

#if defined(BACAPP_DEVICE_OBJECT_PROPERTY_REFERENCE)
/**
 * @brief Print a BACnetDeviceObjectPropertyReference for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_device_object_property_reference(
    char *str,
    size_t str_len,
    const BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *value)
{
    int slen;
    int ret_val = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* object-identifier       [0] BACnetObjectIdentifier */
    slen = bacapp_snprintf_object_id(str, str_len, &value->objectIdentifier);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* property-identifier     [1] BACnetPropertyIdentifier */
    slen = bacapp_snprintf_property_identifier(
        str, str_len, value->propertyIdentifier);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* property-array-index    [2] Unsigned OPTIONAL,*/
    if (value->arrayIndex == BACNET_ARRAY_ALL) {
        slen = bacapp_snprintf(str, str_len, "-1");
    } else {
        slen = bacapp_snprintf(
            str, str_len, "%lu", (unsigned long)value->arrayIndex);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* device-identifier       [3] BACnetObjectIdentifier OPTIONAL */
    slen = bacapp_snprintf_object_id(str, str_len, &value->deviceIdentifier);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    ret_val += bacapp_snprintf(str, str_len, "}");

    return ret_val;
}
#endif

#if defined(BACAPP_DEVICE_OBJECT_REFERENCE)
/**
 * @brief Print a BACnetDeviceObjectReference for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_device_object_reference(
    char *str, size_t str_len, const BACNET_DEVICE_OBJECT_REFERENCE *value)
{
    int slen;
    int ret_val = 0;

    /* BACnetDeviceObjectReference */
    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (value->deviceIdentifier.type == OBJECT_DEVICE) {
        slen =
            bacapp_snprintf_object_id(str, str_len, &value->deviceIdentifier);
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        slen = bacapp_snprintf(str, str_len, ",");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    }
    slen = bacapp_snprintf_object_id(str, str_len, &value->objectIdentifier);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += slen;

    return ret_val;
}
#endif

#if defined(BACAPP_OBJECT_PROPERTY_REFERENCE)
/**
 * @brief Print a BACnetObjectPropertyReference for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_object_property_reference(
    char *str, size_t str_len, const BACNET_OBJECT_PROPERTY_REFERENCE *value)
{
    int slen;
    int ret_val = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (value->object_identifier.type != OBJECT_NONE) {
        /* object-identifier [0] BACnetObjectIdentifier */
        slen =
            bacapp_snprintf_object_id(str, str_len, &value->object_identifier);
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        slen = bacapp_snprintf(str, str_len, ",");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    }
    /* property-identifier [1] BACnetPropertyIdentifier */
    slen = bacapp_snprintf_property_identifier(
        str, str_len, value->property_identifier);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (value->property_array_index != BACNET_ARRAY_ALL) {
        /* property-array-index [2] Unsigned OPTIONAL */
        slen = bacapp_snprintf(
            str, str_len, ", %lu", (unsigned long)value->property_array_index);
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    ret_val += bacapp_snprintf(str, str_len, "}");

    return ret_val;
}
#endif

#if defined(BACAPP_ACCESS_RULE)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to be printed
 * @return number of characters written to the string
 */
static int bacapp_snprintf_access_rule(
    char *str, size_t str_len, const BACNET_ACCESS_RULE *value)
{
    int slen;
    int ret_val = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /*  specified (0), always (1) */
    if (value->time_range_specifier == TIME_RANGE_SPECIFIER_SPECIFIED) {
        slen = bacapp_snprintf(str, str_len, "specified, ");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        slen = bacapp_snprintf_device_object_property_reference(
            str, str_len, &value->time_range);
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    } else {
        slen = bacapp_snprintf(str, str_len, "always, ");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    }
    /* specified (0), all (1) */
    if (value->location_specifier == LOCATION_SPECIFIER_SPECIFIED) {
        slen = bacapp_snprintf(str, str_len, "specified, ");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        slen = bacapp_snprintf_device_object_reference(
            str, str_len, &value->location);
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    } else {
        slen = bacapp_snprintf(str, str_len, "all");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    }
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_COLOR_COMMAND)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to be printed
 * @return number of characters written to the string
 */
static int bacapp_snprintf_color_command(
    char *str, size_t str_len, const BACNET_COLOR_COMMAND *value)
{
    int slen;
    int ret_val = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, "(");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(
        str, str_len, "%s", bactext_color_operation_name(value->operation));
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ")");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    /* FIXME: add the Color Command optional values */

    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    return ret_val;
}
#endif

#if defined(BACAPP_CHANNEL_VALUE)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to be printed
 * @return number of characters written to the string
 */
static int bacapp_snprintf_channel_value(
    char *str, size_t str_len, const BACNET_CHANNEL_VALUE *value)
{
    int ret_val = 0;

    switch (value->tag) {
        case BACNET_APPLICATION_TAG_NULL:
            ret_val = bacapp_snprintf_null(str, str_len);
            break;
        case BACNET_APPLICATION_TAG_BOOLEAN:
#if defined(CHANNEL_BOOLEAN)
            ret_val =
                bacapp_snprintf_boolean(str, str_len, value->type.Boolean);
#endif
            break;
        case BACNET_APPLICATION_TAG_UNSIGNED_INT:
#if defined(CHANNEL_UNSIGNED)
            ret_val = bacapp_snprintf_unsigned_integer(
                str, str_len, value->type.Unsigned_Int);
#endif
            break;
        case BACNET_APPLICATION_TAG_SIGNED_INT:
#if defined(CHANNEL_SIGNED)
            ret_val = bacapp_snprintf_signed_integer(
                str, str_len, value->type.Signed_Int);
#endif
            break;
        case BACNET_APPLICATION_TAG_REAL:
#if defined(CHANNEL_REAL)
            ret_val = bacapp_snprintf_real(str, str_len, value->type.Real);
#endif
            break;
        case BACNET_APPLICATION_TAG_DOUBLE:
#if defined(CHANNEL_DOUBLE)
            ret_val = bacapp_snprintf_double(str, str_len, value->type.Double);
#endif
            break;
        case BACNET_APPLICATION_TAG_ENUMERATED:
#if defined(CHANNEL_ENUMERATED)
            ret_val = bacapp_snprintf(
                str, str_len, "%lu", (unsigned long)value->type.Enumerated);
#endif
            break;
        case BACNET_APPLICATION_TAG_LIGHTING_COMMAND:
#if defined(CHANNEL_LIGHTING_COMMAND)
            ret_val = lighting_command_to_ascii(
                &value->type.Lighting_Command, str, str_len);
#endif
            break;
        case BACNET_APPLICATION_TAG_COLOR_COMMAND:
#if defined(CHANNEL_COLOR_COMMAND)
            ret_val = bacapp_snprintf_color_command(
                str, str_len, &value->type.Color_Command);
#endif
            break;
        case BACNET_APPLICATION_TAG_XY_COLOR:
#if defined(CHANNEL_XY_COLOR)
            ret_val = xy_color_to_ascii(&value->type.XY_Color, str, str_len);
#endif
            break;
        default:
            break;
    }

    return ret_val;
}
#endif

#if defined(BACAPP_CHANNEL)
/**
 * @brief For a given application value, copy to the channel value
 * @param  cvalue - BACNET_CHANNEL_VALUE value
 * @param  value - BACNET_APPLICATION_DATA_VALUE value
 * @return  true if values are able to be copied
 */
bool bacapp_channel_value_copy(
    BACNET_CHANNEL_VALUE *cvalue, const BACNET_APPLICATION_DATA_VALUE *value)
{
    bool status = false;

    if (!value || !cvalue) {
        return false;
    }
    switch (value->tag) {
#if defined(BACAPP_NULL)
        case BACNET_APPLICATION_TAG_NULL:
            cvalue->tag = value->tag;
            status = true;
            break;
#endif
#if defined(BACAPP_BOOLEAN) && defined(CHANNEL_BOOLEAN)
        case BACNET_APPLICATION_TAG_BOOLEAN:
            cvalue->tag = value->tag;
            cvalue->type.Boolean = value->type.Boolean;
            status = true;
            break;
#endif
#if defined(BACAPP_UNSIGNED) && defined(CHANNEL_UNSIGNED)
        case BACNET_APPLICATION_TAG_UNSIGNED_INT:
            cvalue->tag = value->tag;
            cvalue->type.Unsigned_Int = value->type.Unsigned_Int;
            status = true;
            break;
#endif
#if defined(BACAPP_SIGNED) && defined(CHANNEL_SIGNED)
        case BACNET_APPLICATION_TAG_SIGNED_INT:
            cvalue->tag = value->tag;
            cvalue->type.Signed_Int = value->type.Signed_Int;
            status = true;
            break;
#endif
#if defined(BACAPP_REAL) && defined(CHANNEL_REAL)
        case BACNET_APPLICATION_TAG_REAL:
            cvalue->tag = value->tag;
            cvalue->type.Real = value->type.Real;
            status = true;
            break;
#endif
#if defined(BACAPP_DOUBLE) && defined(CHANNEL_DOUBLE)
        case BACNET_APPLICATION_TAG_DOUBLE:
            cvalue->tag = value->tag;
            cvalue->type.Double = value->type.Double;
            status = true;
            break;
#endif
#if defined(BACAPP_OCTET_STRING) && defined(CHANNEL_OCTET_STRING)
        case BACNET_APPLICATION_TAG_OCTET_STRING:
            cvalue->tag = value->tag;
            octetstring_copy(
                &cvalue->type.Octet_String, &value->type.Octet_String);
            status = true;
            break;
#endif
#if defined(BACAPP_CHARACTER_STRING) && defined(CHANNEL_CHARACTER_STRING)
        case BACNET_APPLICATION_TAG_CHARACTER_STRING:
            cvalue->tag = value->tag;
            characterstring_copy(
                &cvalue->type.Character_String, &value->type.Character_String);
            status = true;
            break;
#endif
#if defined(BACAPP_BIT_STRING) && defined(CHANNEL_BIT_STRING)
        case BACNET_APPLICATION_TAG_BIT_STRING:
            cvalue->tag = value->tag;
            bitstring_copy(&cvalue->type.Bit_String, &value->type.Bit_String);
            status = true;
            break;
#endif
#if defined(BACAPP_ENUMERATED) && defined(CHANNEL_ENUMERATED)
        case BACNET_APPLICATION_TAG_ENUMERATED:
            cvalue->tag = value->tag;
            cvalue->type.Enumerated = value->type.Enumerated;
            status = true;
            break;
#endif
#if defined(BACAPP_DATE) && defined(CHANNEL_DATE)
        case BACNET_APPLICATION_TAG_DATE:
            cvalue->tag = value->tag;
            datetime_date_copy(&cvalue->type.Date, &value->type.Date);
            apdu_len = encode_application_date(apdu, &value->type.Date);
            status = true;
            break;
#endif
#if defined(BACAPP_TIME) && defined(CHANNEL_TIME)
        case BACNET_APPLICATION_TAG_TIME:
            cvalue->tag = value->tag;
            datetime_time_copy(&cvalue->type.Time, &value->type.Time);
            break;
#endif
#if defined(BACAPP_OBJECT_ID) && defined(CHANNEL_OBJECT_ID)
        case BACNET_APPLICATION_TAG_OBJECT_ID:
            cvalue->tag = value->tag;
            cvalue->type.Object_Id.type = value->type.Object_Id.type;
            cvalue->type.Object_Id.instance = value->type.Object_Id.instance;
            status = true;
            break;
#endif
#if defined(BACAPP_LIGHTING_COMMAND) && defined(CHANNEL_LIGHTING_COMMAND)
        case BACNET_APPLICATION_TAG_LIGHTING_COMMAND:
            cvalue->tag = value->tag;
            lighting_command_copy(
                &cvalue->type.Lighting_Command, &value->type.Lighting_Command);
            status = true;
            break;
#endif
#if defined(BACAPP_COLOR_COMMAND) && defined(CHANNEL_COLOR_COMMAND)
        case BACNET_APPLICATION_TAG_COLOR_COMMAND:
            cvalue->tag = value->tag;
            color_command_copy(
                &cvalue->type.Color_Command, &value->type.Color_Command);
            status = true;
            break;
#endif
#if defined(BACAPP_COLOR_COMMAND) && defined(CHANNEL_XY_COLOR)
        case BACNET_APPLICATION_TAG_XY_COLOR:
            cvalue->tag = value->tag;
            xy_color_copy(&cvalue->type.XY_Color, &value->type.XY_Color);
            status = true;
            break;
#endif
        default:
            break;
    }

    return status;
}
#endif

#if defined(BACAPP_LOG_RECORD)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to be printed
 * @return number of characters written to the string
 */
static int bacapp_snprintf_log_record(
    char *str, size_t str_len, const BACNET_LOG_RECORD *value)
{
    int ret_val = 0, slen;
    BACNET_BIT_STRING bitstring;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf_datetime_numeric(str, str_len, &value->timestamp);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (value->tag < BACNET_LOG_DATUM_MAX) {
        slen = bacapp_snprintf(
            str, str_len, ", %s:", bactext_log_datum_name(value->tag));
    } else {
        slen =
            bacapp_snprintf(str, str_len, ", <tag=%u>:", (unsigned)value->tag);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    switch (value->tag) {
        case BACNET_LOG_DATUM_STATUS:
            bitstring_init(&bitstring);
            bitstring_set_bits_used(&bitstring, 1, 8 - 3);
            bitstring_set_octet(&bitstring, 0, value->log_datum.log_status);
            slen = bacapp_snprintf_bit_string(str, str_len, &bitstring);
            break;
        case BACNET_LOG_DATUM_BOOLEAN:
            slen = bacapp_snprintf_boolean(
                str, str_len, value->log_datum.boolean_value);
            break;
        case BACNET_LOG_DATUM_REAL:
            slen =
                bacapp_snprintf_real(str, str_len, value->log_datum.real_value);
            break;
        case BACNET_LOG_DATUM_ENUMERATED:
            slen = bacapp_snprintf(
                str, str_len, "%lu",
                (unsigned long)value->log_datum.enumerated_value);
            break;
        case BACNET_LOG_DATUM_UNSIGNED:
            slen = bacapp_snprintf_unsigned_integer(
                str, str_len, value->log_datum.unsigned_value);
            break;
        case BACNET_LOG_DATUM_SIGNED:
            slen = bacapp_snprintf_signed_integer(
                str, str_len, value->log_datum.integer_value);
            break;
        case BACNET_LOG_DATUM_NULL:
            slen = bacapp_snprintf_null(str, str_len);
            break;
        case BACNET_LOG_DATUM_FAILURE:
            slen = bacapp_snprintf(
                str, str_len, "%s,%s",
                bactext_error_class_name(value->log_datum.failure.error_class),
                bactext_error_code_name(value->log_datum.failure.error_code));
            break;
        case BACNET_LOG_DATUM_TIME_CHANGE:
            slen = bacapp_snprintf_real(
                str, str_len, value->log_datum.time_change);
            break;
        default:
            slen = 0;
            break;
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* FIXME: optional status flags */
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_WEEKLY_SCHEDULE)
/**
 * @brief Print a weekly schedule value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param ws - weekly schedule value to print
 * @param arrayIndex - index of the weekly schedule to print
 * @return number of characters written
 */
static int bacapp_snprintf_weeklyschedule(
    char *str,
    size_t str_len,
    const BACNET_WEEKLY_SCHEDULE *ws,
    BACNET_ARRAY_INDEX arrayIndex)
{
    int slen;
    int ret_val = 0;
    int wi, ti;
    BACNET_OBJECT_PROPERTY_VALUE dummyPropValue;
    BACNET_APPLICATION_DATA_VALUE dummyDataValue;

    const char *weekdaynames[7] = { "Mon", "Tue", "Wed", "Thu",
                                    "Fri", "Sat", "Sun" };
    const int loopend = ((arrayIndex == BACNET_ARRAY_ALL) ? 7 : 1);

    /* Find what inner type it uses */
    int inner_tag = -1;
    for (wi = 0; wi < loopend; wi++) {
        const BACNET_DAILY_SCHEDULE *ds = &ws->weeklySchedule[wi];
        for (ti = 0; ti < ds->TV_Count; ti++) {
            int tag = ds->Time_Values[ti].Value.tag;
            if (inner_tag == -1) {
                inner_tag = tag;
            } else if (inner_tag != tag) {
                inner_tag = -2;
            }
        }
    }

    if (inner_tag == -1) {
        slen = bacapp_snprintf(str, str_len, "(Null; ");
    } else if (inner_tag == -2) {
        slen = bacapp_snprintf(str, str_len, "(MIXED_TYPES; ");
    } else {
        slen = bacapp_snprintf(
            str, str_len, "(%s; ", bactext_application_tag_name(inner_tag));
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    for (wi = 0; wi < loopend; wi++) {
        const BACNET_DAILY_SCHEDULE *ds = &ws->weeklySchedule[wi];
        if (arrayIndex == BACNET_ARRAY_ALL) {
            slen = bacapp_snprintf(str, str_len, "%s: [", weekdaynames[wi]);
        } else {
            slen = bacapp_snprintf(
                str, str_len, "%s: [",
                (arrayIndex >= 1 && arrayIndex <= 7)
                    ? weekdaynames[arrayIndex - 1]
                    : "???");
        }
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        for (ti = 0; ti < ds->TV_Count; ti++) {
            slen =
                bacapp_snprintf_time(str, str_len, &ds->Time_Values[ti].Time);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            slen = bacapp_snprintf(str, str_len, " ");
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            bacnet_primitive_to_application_data_value(
                &dummyDataValue, &ds->Time_Values[ti].Value);
            dummyPropValue.value = &dummyDataValue;
            dummyPropValue.object_property = PROP_PRESENT_VALUE;
            dummyPropValue.object_type = OBJECT_SCHEDULE;
            dummyPropValue.array_index = 0;
            slen = bacapp_snprintf_value(str, str_len, &dummyPropValue);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            if (ti < ds->TV_Count - 1) {
                slen = bacapp_snprintf(str, str_len, ", ");
                ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            }
        }
        if (wi < loopend - 1) {
            slen = bacapp_snprintf(str, str_len, "]; ");
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        }
    }
    slen = bacapp_snprintf(str, str_len, "])");
    ret_val += slen;
    return ret_val;
}
#endif

#if defined(BACAPP_HOST_N_PORT)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_host_n_port(
    char *str, size_t str_len, const BACNET_HOST_N_PORT *value)
{
    int slen, len, i;
    const char *char_str;
    int ret_val = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    if (value->host_ip_address) {
        const uint8_t *octet_str;
        octet_str = octetstring_value_const(&value->host.ip_address);
        slen = bacapp_snprintf(
            str, str_len, "%u.%u.%u.%u:%u", (unsigned)octet_str[0],
            (unsigned)octet_str[1], (unsigned)octet_str[2],
            (unsigned)octet_str[3], (unsigned)value->port);
        ret_val += slen;
    } else if (value->host_name) {
        const BACNET_CHARACTER_STRING *name;
        name = &value->host.name;
        len = characterstring_length(name);
        char_str = characterstring_value(name);
        slen = bacapp_snprintf(str, str_len, "\"");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        for (i = 0; i < len; i++) {
            if (isprint(*((const unsigned char *)char_str))) {
                slen = bacapp_snprintf(str, str_len, "%c", *char_str);
            } else {
                slen = bacapp_snprintf(str, str_len, "%c", '.');
            }
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            char_str++;
        }
        slen = bacapp_snprintf(str, str_len, "\"");
        ret_val += slen;
    }
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_SPECIAL_EVENT) || defined(BACAPP_CALENDAR_ENTRY)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_calendar_entry(
    char *str, size_t str_len, const BACNET_CALENDAR_ENTRY *value)
{
    int slen;
    int ret_val = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    switch (value->tag) {
        case BACNET_CALENDAR_DATE:
            slen = bacapp_snprintf_date(str, str_len, &value->type.Date);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            break;
        case BACNET_CALENDAR_DATE_RANGE:
            slen =
                bacapp_snprintf_daterange(str, str_len, &value->type.DateRange);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            break;
        case BACNET_CALENDAR_WEEK_N_DAY:
            slen =
                bacapp_snprintf_weeknday(str, str_len, &value->type.WeekNDay);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            break;
        default:
            /* do nothing */
            break;
    }
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_SPECIAL_EVENT)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_primitive_data_value(
    char *str, size_t str_len, const BACNET_PRIMITIVE_DATA_VALUE *value)
{
    int ret_val = 0;

    switch (value->tag) {
#if defined(BACAPP_NULL)
        case BACNET_APPLICATION_TAG_NULL:
            ret_val = bacapp_snprintf_null(str, str_len);
            break;
#endif
#if defined(BACAPP_BOOLEAN)
        case BACNET_APPLICATION_TAG_BOOLEAN:
            ret_val =
                bacapp_snprintf_boolean(str, str_len, value->type.Boolean);
            break;
#endif
#if defined(BACAPP_UNSIGNED)
        case BACNET_APPLICATION_TAG_UNSIGNED_INT:
            ret_val = bacapp_snprintf_unsigned_integer(
                str, str_len, value->type.Unsigned_Int);
            break;
#endif
#if defined(BACAPP_SIGNED)
        case BACNET_APPLICATION_TAG_SIGNED_INT:
            ret_val = bacapp_snprintf_signed_integer(
                str, str_len, value->type.Signed_Int);
            break;
#endif
#if defined(BACAPP_REAL)
        case BACNET_APPLICATION_TAG_REAL:
            ret_val = bacapp_snprintf_real(str, str_len, value->type.Real);
            break;
#endif
#if defined(BACAPP_DOUBLE)
        case BACNET_APPLICATION_TAG_DOUBLE:
            ret_val = bacapp_snprintf_double(str, str_len, value->type.Double);
            break;
#endif
#if defined(BACAPP_ENUMERATED)
        case BACNET_APPLICATION_TAG_ENUMERATED:
            ret_val = bacapp_snprintf_enumerated(
                str, str_len, OBJECT_COMMAND, PROP_ACTION,
                value->type.Enumerated);
            break;
#endif
        case BACNET_APPLICATION_TAG_EMPTYLIST:
            break;
        default:
            break;
    }
    return ret_val;
}
#endif

#if defined(BACAPP_SPECIAL_EVENT)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param ws - value to print
 * @param arrayIndex - index of the to print
 * @return number of characters written
 */
static int bacapp_snprintf_daily_schedule(
    char *str, size_t str_len, const BACNET_DAILY_SCHEDULE *value)
{
    int slen;
    int ret_val = 0;
    uint16_t i;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    for (i = 0; i < value->TV_Count; i++) {
        if (i != 0) {
            slen = bacapp_snprintf(str, str_len, ", ");
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        }
        slen = bacapp_snprintf_time(str, str_len, &value->Time_Values[i].Time);
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        slen = bacapp_snprintf(str, str_len, ",");
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
        slen = bacapp_snprintf_primitive_data_value(
            str, str_len, &value->Time_Values[i].Value);
        ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    }
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_SPECIAL_EVENT)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param ws - value to print
 * @param arrayIndex - index of the to print
 * @return number of characters written
 */
static int bacapp_snprintf_special_event(
    char *str, size_t str_len, const BACNET_SPECIAL_EVENT *value)
{
    int slen;
    int ret_val = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    switch (value->periodTag) {
        case BACNET_SPECIAL_EVENT_PERIOD_CALENDAR_ENTRY:
            slen = bacapp_snprintf_calendar_entry(
                str, str_len, &value->period.calendarEntry);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            break;
        case BACNET_SPECIAL_EVENT_PERIOD_CALENDAR_REFERENCE:
            slen = bacapp_snprintf_object_id(
                str, str_len, &value->period.calendarReference);
            ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
            break;
        default:
            break;
    }
    slen = bacapp_snprintf_daily_schedule(str, str_len, &value->timeValues);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_ACTION_COMMAND)
/**
 * @brief Print a weekly schedule value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param ws - weekly schedule value to print
 * @param arrayIndex - index of the weekly schedule to print
 * @return number of characters written
 */
static int bacapp_snprintf_action_property_value(
    char *str, size_t str_len, const BACNET_ACTION_PROPERTY_VALUE *value)
{
    int ret_val = 0;

    switch (value->tag) {
#if defined(BACACTION_NULL)
        case BACNET_APPLICATION_TAG_NULL:
            ret_val = bacapp_snprintf_null(str, str_len);
            break;
#endif
#if defined(BACACTION_BOOLEAN)
        case BACNET_APPLICATION_TAG_BOOLEAN:
            ret_val =
                bacapp_snprintf_boolean(str, str_len, value->type.Boolean);
            break;
#endif
#if defined(BACACTION_UNSIGNED)
        case BACNET_APPLICATION_TAG_UNSIGNED_INT:
            ret_val = bacapp_snprintf_unsigned_integer(
                str, str_len, value->type.Unsigned_Int);
            break;
#endif
#if defined(BACACTION_SIGNED)
        case BACNET_APPLICATION_TAG_SIGNED_INT:
            ret_val = bacapp_snprintf_signed_integer(
                str, str_len, value->type.Signed_Int);
            break;
#endif
#if defined(BACACTION_REAL)
        case BACNET_APPLICATION_TAG_REAL:
            ret_val = bacapp_snprintf_real(str, str_len, value->type.Real);
            break;
#endif
#if defined(BACACTION_DOUBLE)
        case BACNET_APPLICATION_TAG_DOUBLE:
            ret_val = bacapp_snprintf_double(str, str_len, value->type.Double);
            break;
#endif
#if defined(BACACTION_ENUMERATED)
        case BACNET_APPLICATION_TAG_ENUMERATED:
            ret_val = bacapp_snprintf_enumerated(
                str, str_len, OBJECT_COMMAND, PROP_ACTION,
                value->type.Enumerated);
            break;
#endif
        case BACNET_APPLICATION_TAG_EMPTYLIST:
            break;
        default:
            break;
    }

    return ret_val;
}
#endif

#if defined(BACAPP_ACTION_COMMAND)
/**
 * @brief Print a value to a string for EPICS
 * @param str - destination string, or NULL for length only
 * @param str_len - length of the destination string, or 0 for length only
 * @param value - value to print
 * @return number of characters written
 */
static int bacapp_snprintf_action_command(
    char *str, size_t str_len, const BACNET_ACTION_LIST *value)
{
    int slen;
    int ret_val = 0;
    BACNET_ACTION_PROPERTY_VALUE action_value = { 0 };

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* deviceIdentifier [0] BACnetObjectIdentifier OPTIONAL */
    slen = bacapp_snprintf_object_id(str, str_len, &value->Device_Id);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* objectIdentifier [1] BACnetObjectIdentifier */
    slen = bacapp_snprintf_object_id(str, str_len, &value->Device_Id);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* propertyIdentifier [2] BACnetPropertyIdentifier */
    slen = bacapp_snprintf_property_identifier(
        str, str_len, value->Property_Identifier);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* propertyArrayIndex [3] Unsigned OPTIONAL */
    if (value->Property_Array_Index == BACNET_ARRAY_ALL) {
        slen = bacapp_snprintf(str, str_len, "-1,");
    } else {
        slen = bacapp_snprintf(
            str, str_len, "%lu,", (unsigned long)value->Property_Array_Index);
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* propertyValue [4] ABSTRACT-SYNTAX.&Type */
    if (value->Property_Value.data_len > 0) {
        slen = bacnet_action_property_value_decode(
            value->Property_Value.data, value->Property_Value.data_len,
            &action_value);
        if (slen > 0) {
            slen = bacapp_snprintf_action_property_value(
                str, str_len, &action_value);
        } else {
            slen = bacapp_snprintf(str, str_len, "{invalid-action-value}");
        }
    } else {
        slen = bacapp_snprintf(str, str_len, "NULL");
    }
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* priority [5] Unsigned (1..16) OPTIONAL */
    slen = bacapp_snprintf(str, str_len, "%lu", (unsigned long)value->Priority);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* postDelay [6] Unsigned OPTIONAL */
    slen =
        bacapp_snprintf(str, str_len, "%lu", (unsigned long)value->Post_Delay);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* quitOnFailure [7] BOOLEAN */
    slen = bacapp_snprintf_boolean(str, str_len, value->Quit_On_Failure);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, ",");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    /* writeSuccessful [8] BOOLEAN */
    slen = bacapp_snprintf_boolean(str, str_len, value->Write_Successful);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

#if defined(BACAPP_AUTHENTICATION)
static int bacapp_snprintf_authentication_factor(
    char *str, size_t str_len, const BACNET_AUTHENTICATION_FACTOR *value)
{
    int slen;
    int ret_val = 0;

    slen = bacapp_snprintf(str, str_len, "{");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(
        str, str_len, "%s,%lu,",
        bactext_authentication_factor_type_name(value->format_type),
        (unsigned long)value->format_class);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf_octet_string(str, str_len, &value->value);
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
    slen = bacapp_snprintf(str, str_len, "}");
    ret_val += bacapp_snprintf_shift(slen, &str, &str_len);

    return ret_val;
}
#endif

/**
 * @brief Extract the value into a text string
 * @param str - the buffer to store the extracted value, or NULL for length
 * @param str_len - the size of the buffer, or 0 for length only
 * @param object_value - ptr to BACnet object value from which to extract str
 * @return number of bytes (excluding terminating NULL byte) that were stored
 *  to the output string.
 */
int bacapp_json_snprintf_value(
    char *str, size_t str_len, const BACNET_OBJECT_PROPERTY_VALUE *object_value)
{
    const BACNET_APPLICATION_DATA_VALUE *value;
    BACNET_PROPERTY_ID property = PROP_ALL;
    BACNET_OBJECT_TYPE object_type = MAX_BACNET_OBJECT_TYPE;
    int ret_val = 0;
#if defined(BACAPP_BDT_ENTRY) || defined(BACAPP_FDT_ENTRY) || \
    defined(BACAPP_SHED_LEVEL)
    int slen = 0;
#endif

    if (object_value && object_value->value) {
        value = object_value->value;
        property = object_value->object_property;
        object_type = object_value->object_type;
        switch (value->tag) {
#if defined(BACAPP_NULL)
            case BACNET_APPLICATION_TAG_NULL:
                ret_val = bacapp_snprintf_null(str, str_len);
                break;
#endif
#if defined(BACAPP_BOOLEAN)
            case BACNET_APPLICATION_TAG_BOOLEAN:
                ret_val =
                    bacapp_snprintf_boolean(str, str_len, value->type.Boolean);
                break;
#endif
#if defined(BACAPP_UNSIGNED)
            case BACNET_APPLICATION_TAG_UNSIGNED_INT:
                ret_val = bacapp_snprintf_unsigned_integer(
                    str, str_len, value->type.Unsigned_Int);
                break;
#endif
#if defined(BACAPP_SIGNED)
            case BACNET_APPLICATION_TAG_SIGNED_INT:
                ret_val = bacapp_snprintf_signed_integer(
                    str, str_len, value->type.Signed_Int);
                break;
#endif
#if defined(BACAPP_REAL)
            case BACNET_APPLICATION_TAG_REAL:
                ret_val = bacapp_snprintf_real(str, str_len, value->type.Real);
                break;
#endif
#if defined(BACAPP_DOUBLE)
            case BACNET_APPLICATION_TAG_DOUBLE:
                ret_val =
                    bacapp_snprintf_double(str, str_len, value->type.Double);
                break;
#endif
#if defined(BACAPP_OCTET_STRING)
            case BACNET_APPLICATION_TAG_OCTET_STRING:
                ret_val = bacapp_snprintf_octet_string(
                    str, str_len, &value->type.Octet_String);
                break;
#endif
#if defined(BACAPP_CHARACTER_STRING)
            case BACNET_APPLICATION_TAG_CHARACTER_STRING:
                ret_val = bacapp_snprintf_character_string(
                    str, str_len, &value->type.Character_String);
                break;
#endif
#if defined(BACAPP_BIT_STRING)
            case BACNET_APPLICATION_TAG_BIT_STRING:
                ret_val = bacapp_snprintf_bit_string(
                    str, str_len, &value->type.Bit_String);
                break;
#endif
#if defined(BACAPP_ENUMERATED)
            case BACNET_APPLICATION_TAG_ENUMERATED:
                ret_val = bacapp_snprintf_enumerated(
                    str, str_len, object_type, property,
                    value->type.Enumerated);
                break;
#endif
#if defined(BACAPP_DATE)
            case BACNET_APPLICATION_TAG_DATE:
                ret_val = bacapp_snprintf_date(str, str_len, &value->type.Date);
                break;
#endif
#if defined(BACAPP_TIME)
            case BACNET_APPLICATION_TAG_TIME:
                ret_val = bacapp_snprintf_time(str, str_len, &value->type.Time);
                break;
#endif
#if defined(BACAPP_OBJECT_ID)
            case BACNET_APPLICATION_TAG_OBJECT_ID:
                ret_val = bacapp_snprintf_object_id(
                    str, str_len, &value->type.Object_Id);
                break;
#endif
#if defined(BACAPP_TIMESTAMP)
            case BACNET_APPLICATION_TAG_TIMESTAMP:
                slen = bacapp_timestamp_to_ascii(
                    str, str_len, &value->type.Time_Stamp);
                ret_val += slen;
                break;
#endif
#if defined(BACAPP_DATETIME)
            case BACNET_APPLICATION_TAG_DATETIME:
                ret_val = bacapp_snprintf_datetime(
                    str, str_len, &value->type.Date_Time);
                break;
#endif
#if defined(BACAPP_DATERANGE)
            case BACNET_APPLICATION_TAG_DATERANGE:
                ret_val = bacapp_snprintf_daterange(
                    str, str_len, &value->type.Date_Range);
                break;
#endif
#if defined(BACAPP_LIGHTING_COMMAND)
            case BACNET_APPLICATION_TAG_LIGHTING_COMMAND:
                ret_val = lighting_command_to_ascii(
                    &value->type.Lighting_Command, str, str_len);
                break;
#endif
#if defined(BACAPP_XY_COLOR)
            case BACNET_APPLICATION_TAG_XY_COLOR:
                /* BACnetxyColor */
                ret_val =
                    xy_color_to_ascii(&value->type.XY_Color, str, str_len);
                break;
#endif
#if defined(BACAPP_COLOR_COMMAND)
            case BACNET_APPLICATION_TAG_COLOR_COMMAND:
                /* BACnetColorCommand */
                ret_val = bacapp_snprintf_color_command(
                    str, str_len, &value->type.Color_Command);
                break;
#endif
#if defined(BACAPP_WEEKLY_SCHEDULE)
            case BACNET_APPLICATION_TAG_WEEKLY_SCHEDULE:
                /* BACnetWeeklySchedule */
                ret_val = bacapp_snprintf_weeklyschedule(
                    str, str_len, &value->type.Weekly_Schedule,
                    object_value->array_index);
                break;
#endif
#if defined(BACAPP_HOST_N_PORT)
            case BACNET_APPLICATION_TAG_HOST_N_PORT:
                ret_val = bacapp_snprintf_host_n_port(
                    str, str_len, &value->type.Host_Address);
                break;
#endif
#if defined(BACAPP_DEVICE_OBJECT_PROPERTY_REFERENCE)
            case BACNET_APPLICATION_TAG_DEVICE_OBJECT_PROPERTY_REFERENCE:
                ret_val = bacapp_snprintf_device_object_property_reference(
                    str, str_len,
                    &value->type.Device_Object_Property_Reference);
                break;
#endif
#if defined(BACAPP_DEVICE_OBJECT_REFERENCE)
            case BACNET_APPLICATION_TAG_DEVICE_OBJECT_REFERENCE:
                ret_val = bacapp_snprintf_device_object_reference(
                    str, str_len, &value->type.Device_Object_Reference);
                break;
#endif
#if defined(BACAPP_OBJECT_PROPERTY_REFERENCE)
            case BACNET_APPLICATION_TAG_OBJECT_PROPERTY_REFERENCE:
                ret_val = bacapp_snprintf_object_property_reference(
                    str, str_len, &value->type.Object_Property_Reference);
                break;
#endif
#if defined(BACAPP_DESTINATION)
            case BACNET_APPLICATION_TAG_DESTINATION:
                ret_val = bacnet_destination_to_ascii(
                    &value->type.Destination, str, str_len);
                break;
#endif
#if defined(BACAPP_CALENDAR_ENTRY)
            case BACNET_APPLICATION_TAG_CALENDAR_ENTRY:
                ret_val = bacapp_snprintf_calendar_entry(
                    str, str_len, &value->type.Calendar_Entry);
                break;
#endif
#if defined(BACAPP_SPECIAL_EVENT)
            case BACNET_APPLICATION_TAG_SPECIAL_EVENT:
                ret_val = bacapp_snprintf_special_event(
                    str, str_len, &value->type.Special_Event);
                break;
#endif
#if defined(BACAPP_BDT_ENTRY)
            case BACNET_APPLICATION_TAG_BDT_ENTRY:
                slen = bacapp_snprintf(str, str_len, "{");
                ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
                slen = bacnet_bdt_entry_to_ascii(
                    str, str_len, &value->type.BDT_Entry);
                ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
                slen = bacapp_snprintf(str, str_len, "}");
                ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
                break;
#endif
#if defined(BACAPP_FDT_ENTRY)
            case BACNET_APPLICATION_TAG_FDT_ENTRY:
                slen = bacapp_snprintf(str, str_len, "{");
                ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
                slen = bacnet_fdt_entry_to_ascii(
                    str, str_len, &value->type.FDT_Entry);
                ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
                slen = bacapp_snprintf(str, str_len, "}");
                ret_val += bacapp_snprintf_shift(slen, &str, &str_len);
                break;
#endif
#if defined(BACAPP_ACTION_COMMAND)
            case BACNET_APPLICATION_TAG_ACTION_COMMAND:
                ret_val = bacapp_snprintf_action_command(
                    str, str_len, &value->type.Action_Command);
                break;
#endif
#if defined(BACAPP_SCALE)
            case BACNET_APPLICATION_TAG_SCALE:
                if (value->type.Scale.float_scale) {
                    ret_val = bacapp_snprintf(
                        str, str_len, "%f",
                        (double)value->type.Scale.type.real_scale);
                } else {
                    ret_val = bacapp_snprintf(
                        str, str_len, "%ld",
                        (long)value->type.Scale.type.integer_scale);
                }
                break;
#endif
#if defined(BACAPP_SHED_LEVEL)
            case BACNET_APPLICATION_TAG_SHED_LEVEL:
                ret_val = bacapp_snprintf_shed_level(
                    str, str_len, &value->type.Shed_Level);
                break;
#endif
#if defined(BACAPP_ACCESS_RULE)
            case BACNET_APPLICATION_TAG_ACCESS_RULE:
                ret_val = bacapp_snprintf_access_rule(
                    str, str_len, &value->type.Access_Rule);
                break;
#endif
#if defined(BACAPP_CHANNEL_VALUE)
            case BACNET_APPLICATION_TAG_CHANNEL_VALUE:
                ret_val = bacapp_snprintf_channel_value(
                    str, str_len, &value->type.Channel_Value);
                break;
#endif
#if defined(BACAPP_TIMER_VALUE)
            case BACNET_APPLICATION_TAG_TIMER_VALUE:
                ret_val = bacnet_timer_value_to_ascii(
                    &value->type.Timer_Value, str, str_len);
                break;
#endif
#if defined(BACAPP_RECIPIENT)
            case BACNET_APPLICATION_TAG_RECIPIENT:
                ret_val = bacnet_recipient_to_ascii(
                    &value->type.Recipient, str, str_len);
                break;
#endif
#if defined(BACAPP_ADDRESS_BINDING)
            case BACNET_APPLICATION_TAG_ADDRESS_BINDING:
                ret_val = bacnet_address_binding_to_json(
                    &value->type.Address_Binding, str, str_len);
                break;
#endif
#if defined(BACAPP_NO_VALUE)
            case BACNET_APPLICATION_TAG_NO_VALUE:
                ret_val = bacnet_timer_value_no_value_to_ascii(str, str_len);
                break;
#endif
#if defined(BACAPP_AUTHENTICATION)
            case BACNET_APPLICATION_TAG_AUTHENTICATION_FORMAT:
                ret_val = bacapp_snprintf(
                    str, str_len, "{%s,%lu,%lu}",
                    bactext_authentication_factor_type_name(
                        value->type.Authentication_Format.format_type),
                    (unsigned long)value->type.Authentication_Format.vendor_id,
                    (unsigned long)
                        value->type.Authentication_Format.vendor_format);
                break;
            case BACNET_APPLICATION_TAG_AUTHENTICATION_FACTOR:
                /* BACnetAuthenticationFactor */
                ret_val = bacapp_snprintf_authentication_factor(
                    str, str_len, &value->type.Authentication_Factor);
                break;
#endif
#if defined(BACAPP_LOG_RECORD)
            case BACNET_APPLICATION_TAG_LOG_RECORD:
                ret_val = bacapp_snprintf_log_record(
                    str, str_len, &value->type.Log_Record);
                break;
#endif
            case BACNET_APPLICATION_TAG_EMPTYLIST:
                ret_val = bacapp_snprintf(str, str_len, "{}");
                break;
            default:
                ret_val = bacapp_snprintf(
                    str, str_len, "UnknownType(tag=%d)", value->tag);
                break;
        }
    }

    return ret_val;
}

#ifdef BACAPP_PRINT_ENABLED
/**
 * Print the extracted value from the requested BACnet object property to the
 * specified stream. If stream is NULL, do not print anything. If extraction
 * failed, do not print anything. Return the status of the extraction.
 *
 * @param stream - the I/O stream send the printed value.
 * @param object_value - ptr to BACnet object value from which to extract str
 *
 * @return true if the value was sent to the stream
 */
bool bacapp_json_print_value(
    FILE *stream, const BACNET_OBJECT_PROPERTY_VALUE *object_value)
{
    bool retval = false;
    int str_len = 0;

    /* get the string length first */
    str_len = bacapp_json_snprintf_value(NULL, 0, object_value);
    if (str_len > 0) {
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
        char str[str_len + 1];
#else
        char *str;
        str = calloc(sizeof(char), str_len + 1);
        if (!str) {
            return false;
        }
#endif
        bacapp_json_snprintf_value(str, str_len + 1, object_value);
        if (stream) {
            fprintf(stream, "%s", str);
        }
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 199901L
        /* nothing to do with stack based RAM */
#else
        if (str) {
            free(str);
        }
#endif
        retval = true;
    }

    return retval;
}
#else
bool bacapp_json_print_value(
    FILE *stream, const BACNET_OBJECT_PROPERTY_VALUE *object_value)
{
    (void)stream;
    (void)object_value;
    return false;
}
#endif
