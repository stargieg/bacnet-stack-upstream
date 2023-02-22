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

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h> /* for memmove */
#include <errno.h>
#include <time.h> /* for time */

#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/ucix/ucix.h"
#include "bacnet/config.h" /* the custom stuff */
#include "bacnet/basic/object/device.h" /* me */
#include "bacnet/basic/services.h"
#include "bacnet/datalink/datalink.h"
#include "bacnet/basic/binding/address.h"
#include "bacnet/bacdevobjpropref.h"
#include "bacnet/basic/object/trendlog.h"
#include "bacnet/datetime.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/cov.h"
#include "bacnet/bactext.h"
#include "bacnet/basic/sys/debug.h"
#if defined(BACFILE)
#include "bacnet/basic/object/bacfile.h" /* object list dependency */
#endif

#define PRINTF debug_perror

/* max number of COV properties decoded in a COV notification */
#ifndef MAX_COV_PROPERTIES
#define MAX_COV_PROPERTIES 8
#endif

static const char *sec = "bacnet_tl";
static const char *type = "tl";

struct tl_data_record {
    bacnet_time_t tTimeStamp;      /* When the event occurred */
    uint8_t ucRecType;      /* What type of Event */
    uint8_t ucStatus;       /* Optional Status for read value in b0-b2, b7 = 1 if status is used */
    union {
        uint8_t ucLogStatus;        /* Change of log state flags */
        uint8_t ucBoolean;  /* Stored boolean value */
        float fReal;        /* Stored floating point value */
        uint32_t ulEnum;    /* Stored enumerated value - max 32 bits */
        uint32_t ulUValue;  /* Stored unsigned value - max 32 bits */
        int32_t lSValue;    /* Stored signed value - max 32 bits */
        TL_BITS Bits;       /* Stored bitstring - max 32 bits */
        TL_ERROR Error;     /* Two part error class/code combo */
        float fTime;        /* Interval value for change of time - seconds */
    } Datum;
};

struct object_data {
    bool bEnable;   /* Trend log is active when this is true */
    BACNET_DATE_TIME StartTime;     /* BACnet format start time */
    bacnet_time_t tStartTime;      /* Local time working copy of start time */
    BACNET_DATE_TIME StopTime;      /* BACnet format stop time */
    bacnet_time_t tStopTime;       /* Local time working copy of stop time */
    uint8_t ucTimeFlags;    /* Shorthand info on times */
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE Source; /* Where the data comes from */
    uint32_t ulLogInterval; /* Time between entries in seconds */
    bool bStopWhenFull;     /* Log halts when full if true */
    uint32_t ulRecordCount; /* Count of items currently in the buffer */
    uint32_t ulTotalRecordCount;    /* Count of all items that have ever been inserted into the buffer */
    BACNET_LOGGING_TYPE LoggingType;        /* Polled/cov/triggered */
    bool bAlignIntervals;   /* If true align to the clock */
    uint32_t ulIntervalOffset;      /* Offset from start of period for taking reading in seconds */
    bool bTrigger;  /* Set to 1 to cause a reading to be taken */
    int iIndex;     /* Current insertion point */
    bacnet_time_t tLastDataTime;
    struct tl_data_record Logs[TL_MAX_ENTRIES];
    BACNET_SUBSCRIBE_COV_DATA cov_data;
    uint8_t Request_Invoke_ID;
    BACNET_ADDRESS Target_Address;
    bool Error_Detected;
    unsigned max_apdu;
    bool Simple_Ack_Detected;
    time_t elapsed_seconds;
    time_t last_seconds;
    time_t current_seconds;
    time_t timeout_seconds;
    time_t delta_seconds;
    bool found;
    const char *Object_Name;
    const char *Description;
};

struct object_data_t {
    uint32_t ulLogInterval;
    BACNET_LOGGING_TYPE LoggingType;
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE Source; /* Where the data comes from */
    BACNET_SUBSCRIBE_COV_DATA cov_data;
    const char *Object_Name;
    const char *Description;
};

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_TRENDLOG;


/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Trend_Log_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_ENABLE, PROP_STOP_WHEN_FULL,
    PROP_BUFFER_SIZE, PROP_LOG_BUFFER, PROP_RECORD_COUNT,
    PROP_TOTAL_RECORD_COUNT, PROP_EVENT_STATE, PROP_LOGGING_TYPE,
    PROP_STATUS_FLAGS, -1 };

static const int Trend_Log_Properties_Optional[] = { PROP_DESCRIPTION,
    PROP_START_TIME, PROP_STOP_TIME, PROP_LOG_DEVICE_OBJECT_PROPERTY,
    PROP_LOG_INTERVAL,

    /* Required if COV logging supported
        PROP_COV_RESUBSCRIPTION_INTERVAL,
        PROP_CLIENT_COV_INCREMENT, */

    /* Required if intrinsic reporting supported
        PROP_NOTIFICATION_THRESHOLD,
        PROP_RECORDS_SINCE_NOTIFICATION,
        PROP_LAST_NOTIFY_RECORD,
        PROP_NOTIFICATION_CLASS,
        PROP_EVENT_ENABLE,
        PROP_ACKED_TRANSITIONS,
        PROP_NOTIFY_TYPE,
        PROP_EVENT_TIME_STAMPS, */

    PROP_ALIGN_INTERVALS, PROP_INTERVAL_OFFSET, PROP_TRIGGER, -1 };

static const int Trend_Log_Properties_Proprietary[] = { -1 };

void Trend_Log_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Trend_Log_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Trend_Log_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Trend_Log_Properties_Proprietary;
    }

    return;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need validate that the */
/* given instance exists */
bool Trend_Log_Valid_Instance(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return true;
    }

    return false;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Trend_Log_Count(void)
{
    return Keylist_Count(Object_List);
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Trend_Log_Index_To_Instance(unsigned index)
{
    return Keylist_Key(Object_List, index);
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Trend_Log_Instance_To_Index(uint32_t object_instance)
{
    return Keylist_Index(Object_List, object_instance);
}

/**
 * @brief Get the current time from the Device object
 * @return current time in epoch seconds
 */
static bacnet_time_t Trend_Log_Epoch_Seconds_Now(void)
{
    BACNET_DATE_TIME bdatetime;

    Device_getCurrentDateTime(&bdatetime);
    return datetime_seconds_since_epoch(&bdatetime);
}

/*
 * Note: we use the instance number here and build the name based
 * on the assumption that there is a 1 to 1 correspondence. If there
 * is not we need to convert to index before proceeding.
 */
bool Trend_Log_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    bool status = false;
    struct object_data *pObject;
    char name_text[32];

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Object_Name) {
            status = characterstring_init_ansi(object_name,
                pObject->Object_Name);
        } else {
            snprintf(name_text, sizeof(name_text), "NC %u",
                object_instance);
            status = characterstring_init_ansi(object_name, name_text);
        }
    }

    return status;
}

/**
 * For a given object instance-number, sets the object-name
 * Note that the object name must be unique within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the object-name to be set
 *
 * @return  true if object-name was set
 */
bool Trend_Log_Name_Set(uint32_t object_instance, char *new_name)
{
    bool status = false; /* return value */
    BACNET_CHARACTER_STRING object_name;
    BACNET_OBJECT_TYPE found_type = 0;
    uint32_t found_instance = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && new_name) {
        /* All the object names in a device must be unique */
        characterstring_init_ansi(&object_name, new_name);
        if (Device_Valid_Object_Name(
                &object_name, &found_type, &found_instance)) {
            if ((found_type == Object_Type) &&
                (found_instance == object_instance)) {
                /* writing same name to same object */
                status = true;
            } else {
                /* duplicate name! */
                status = false;
            }
        } else {
            status = true;
            pObject->Object_Name = new_name;
            Device_Inc_Database_Revision();
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the description
 * @param  object_instance - object-instance number of the object
 * @return description text or NULL if not found
 */
char *Trend_Log_Description(uint32_t object_instance)
{
    char *name = NULL;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        name = (char *)pObject->Description;
    }

    return name;
}

/**
 * @brief For a given object instance-number, sets the description
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the description to be set
 * @return  true if object-name was set
 */
bool Trend_Log_Description_Set(uint32_t object_instance, char *new_name)
{
    bool status = false; /* return value */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && new_name) {
        status = true;
        pObject->Description = new_name;
    }

    return status;
}


/* return the length of the apdu encoded or BACNET_STATUS_ERROR for error or
   BACNET_STATUS_ABORT for abort message */
int Trend_Log_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    int len = 0; /* apdu len intermediate value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    struct object_data *pObject;
    uint8_t *apdu = NULL;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
    pObject = Keylist_Data(Object_List, rpdata->object_instance);
    if (!pObject) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        apdu_len = BACNET_STATUS_ERROR;
        return apdu_len;
    }

    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], Object_Type, rpdata->object_instance);
            break;

        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Trend_Log_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_OBJECT_NAME:
            Trend_Log_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(&apdu[0], Object_Type);
            break;

        case PROP_ENABLE:
            apdu_len =
                encode_application_boolean(&apdu[0], pObject->bEnable);
            break;

        case PROP_STOP_WHEN_FULL:
            apdu_len =
                encode_application_boolean(&apdu[0], pObject->bStopWhenFull);
            break;

        case PROP_BUFFER_SIZE:
            apdu_len = encode_application_unsigned(&apdu[0], TL_MAX_ENTRIES);
            break;

        case PROP_LOG_BUFFER:
            /* You can only read the buffer via the ReadRange service */
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_READ_ACCESS_DENIED;
            apdu_len = BACNET_STATUS_ERROR;
            break;

        case PROP_RECORD_COUNT:
            apdu_len += encode_application_unsigned(
                &apdu[0], pObject->ulRecordCount);
            break;

        case PROP_TOTAL_RECORD_COUNT:
            apdu_len += encode_application_unsigned(
                &apdu[0], pObject->ulTotalRecordCount);
            break;

        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
            break;

        case PROP_LOGGING_TYPE:
            apdu_len = encode_application_enumerated(
                &apdu[0], pObject->LoggingType);
            break;

        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, false);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_START_TIME:
            len =
                encode_application_date(&apdu[0], &pObject->StartTime.date);
            apdu_len = len;
            len = encode_application_time(
                &apdu[apdu_len], &pObject->StartTime.time);
            apdu_len += len;
            break;

        case PROP_STOP_TIME:
            len = encode_application_date(&apdu[0], &pObject->StopTime.date);
            apdu_len = len;
            len = encode_application_time(
                &apdu[apdu_len], &pObject->StopTime.time);
            apdu_len += len;
            break;

        case PROP_LOG_DEVICE_OBJECT_PROPERTY:
            /*
             * BACnetDeviceObjectPropertyReference ::= SEQUENCE {
             *     objectIdentifier   [0] BACnetObjectIdentifier,
             *     propertyIdentifier [1] BACnetPropertyIdentifier,
             *     propertyArrayIndex [2] Unsigned OPTIONAL, -- used only with
             * array datatype
             *                                               -- if omitted with
             * an array then
             *                                               -- the entire array
             * is referenced deviceIdentifier   [3] BACnetObjectIdentifier
             * OPTIONAL
             * }
             */
            apdu_len += bacapp_encode_device_obj_property_ref(
                &apdu[0], &pObject->Source);
            break;

        case PROP_LOG_INTERVAL:
            /* We only log to 1 sec accuracy so must multiply by 100 before
             * passing it on */
            apdu_len += encode_application_unsigned(
                &apdu[0], pObject->ulLogInterval * 100);
            break;

        case PROP_ALIGN_INTERVALS:
            apdu_len = encode_application_boolean(
                &apdu[0], pObject->bAlignIntervals);
            break;

        case PROP_INTERVAL_OFFSET:
            /* We only log to 1 sec accuracy so must multiply by 100 before
             * passing it on */
            apdu_len += encode_application_unsigned(
                &apdu[0], pObject->ulIntervalOffset * 100);
            break;

        case PROP_TRIGGER:
            apdu_len =
                encode_application_boolean(&apdu[0], pObject->bTrigger);
            break;

        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) &&
        (rpdata->object_property != PROP_EVENT_TIME_STAMPS) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/* returns true if successful */
bool Trend_Log_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;
    struct object_data *pObject;
    BACNET_DATE start_date, stop_date;
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE TempSource;
    bool bEffectiveEnable;
    struct uci_context *ctxw = NULL;
    char *idx_c = NULL;
    int idx_c_len = 0;

    /* Pin down which log to look at */
    pObject = Keylist_Data(Object_List, wp_data->object_instance);
    if (!pObject)
        return status;

    /* decode the some of the request */
    len = bacapp_decode_application_data(
        wp_data->application_data, wp_data->application_data_len, &value);
    /* FIXME: len < application_data_len: more data? */
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    if ((wp_data->object_property != PROP_EVENT_TIME_STAMPS) &&
        (wp_data->array_index != BACNET_ARRAY_ALL)) {
        /*  only array properties can have array options */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }

    ctxw = ucix_init(sec);
    if (!ctxw)
        fprintf(stderr, "Failed to load config file %s\n",sec);
    idx_c_len = snprintf(NULL, 0, "%d", wp_data->object_instance);
    idx_c = malloc(idx_c_len + 1);
    snprintf(idx_c,idx_c_len + 1,"%d",wp_data->object_instance);

    switch (wp_data->object_property) {
        case PROP_OBJECT_NAME:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Trend_Log_Name_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "name",
                        strndup(value.type.Character_String.value, value.type.Character_String.length));
                    ucix_commit(ctxw,sec);
                }
            }
            break;

        case PROP_ENABLE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                /* Section 12.25.5 can't enable a full log with stop when full
                 * set */
                if ((pObject->bEnable == false) &&
                    (pObject->bStopWhenFull == true) &&
                    (pObject->ulRecordCount == TL_MAX_ENTRIES) &&
                    (value.type.Boolean == true)) {
                    status = false;
                    wp_data->error_class = ERROR_CLASS_OBJECT;
                    wp_data->error_code = ERROR_CODE_LOG_BUFFER_FULL;
                    break;
                }

                /* Only trigger this validation on a potential change of state
                 */
                if (pObject->bEnable != value.type.Boolean) {
                    bEffectiveEnable = TL_Is_Enabled(wp_data->object_instance);
                    pObject->bEnable = value.type.Boolean;
                    /* To do: what actions do we need to take on writing ? */
                    if (value.type.Boolean == false) {
                        if (bEffectiveEnable == true) {
                            /* Only insert record if we really were
                               enabled i.e. times and enable flags */
                            TL_Insert_Status_Rec(
                                wp_data->object_instance, LOG_STATUS_LOG_DISABLED, true);
                        }
                    } else {
                        if (TL_Is_Enabled(wp_data->object_instance)) {
                            /* Have really gone from disabled to enabled as
                             * enable flag and times were correct
                             */
                            TL_Insert_Status_Rec(
                                wp_data->object_instance, LOG_STATUS_LOG_DISABLED, false);
                        }
                    }
                }
            }
            break;

        case PROP_STOP_WHEN_FULL:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                /* Only trigger this on a change of state */
                if (pObject->bStopWhenFull != value.type.Boolean) {
                    pObject->bStopWhenFull = value.type.Boolean;

                    if ((value.type.Boolean == true) &&
                        (pObject->ulRecordCount == TL_MAX_ENTRIES) &&
                        (pObject->bEnable == true)) {
                        /* When full log is switched from normal to stop when
                         * full disable the log and record the fact - see
                         * 135-2008 12.25.12
                         */
                        pObject->bEnable = false;
                        TL_Insert_Status_Rec(
                            wp_data->object_instance, LOG_STATUS_LOG_DISABLED, true);
                    }
                }
            }
            break;

        case PROP_BUFFER_SIZE:
            /* Fixed size buffer so deny write. If buffer size was writable
             * we would probably erase the current log, resize, re-initalise
             * and carry on - however write is not allowed if enable is true.
             */
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;

        case PROP_RECORD_COUNT:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                if (value.type.Unsigned_Int == 0) {
                    /* Time to clear down the log */
                    pObject->ulRecordCount = 0;
                    pObject->iIndex = 0;
                    TL_Insert_Status_Rec(
                        wp_data->object_instance, LOG_STATUS_BUFFER_PURGED, true);
                }
            }
            break;

        case PROP_LOGGING_TYPE:
            /* logic
             * triggered and polled options.
             */
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                pObject->LoggingType =
                    (BACNET_LOGGING_TYPE)value.type.Enumerated;
                if (value.type.Enumerated == LOGGING_TYPE_COV) {
                    if (pObject->cov_data.lifetime == 0) {
                        pObject->cov_data.lifetime = 300;
                        ucix_add_option_int(ctxw, sec, idx_c, "lifetime", 300);
                    }
                    if (pObject->ulLogInterval != 0) {
                        pObject->ulLogInterval = 0;
                        ucix_add_option_int(ctxw, sec, idx_c, "interval", 0);
                    }
                }
                if (value.type.Enumerated == LOGGING_TYPE_POLLED) {
                    /* As per 12.25.27 pick a suitable default if interval
                        * is 0 */
                    if (pObject->ulLogInterval == 0) {
                        pObject->ulLogInterval = 900;
                        ucix_add_option_int(ctxw, sec, idx_c, "interval", 900);
                    }
                    if (pObject->cov_data.lifetime != 0) {
                        pObject->cov_data.lifetime = 0;
                        ucix_add_option_int(ctxw, sec, idx_c, "lifetime", 0);
                    }
                }
                if (value.type.Enumerated == LOGGING_TYPE_TRIGGERED) {
                    /* As per 12.25.27 0 the interval if triggered logging
                        * selected */
                    if (pObject->ulLogInterval != 0) {
                        pObject->ulLogInterval = 0;
                        ucix_add_option_int(ctxw, sec, idx_c, "interval", 0);
                    }
                    if (pObject->cov_data.lifetime != 0) {
                        pObject->cov_data.lifetime = 0;
                        ucix_add_option_int(ctxw, sec, idx_c, "lifetime", 0);
                    }
                }
                ucix_add_option_int(ctxw, sec, idx_c, "type", value.type.Enumerated);
                ucix_commit(ctxw, sec);
            }
            break;

        case PROP_START_TIME:
            /* Copy the date part to safe place */
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_DATE);
            if (!status) {
                break;
            }
            start_date = value.type.Date;
            /* Then decode the time part */
            len =
                bacapp_decode_application_data(wp_data->application_data + len,
                    wp_data->application_data_len - len, &value);

            if (len) {
                status = write_property_type_valid(
                    wp_data, &value, BACNET_APPLICATION_TAG_TIME);
                if (!status) {
                    break;
                }
                /* First record the current enable state of the log */
                bEffectiveEnable = TL_Is_Enabled(wp_data->object_instance);
                /* Safe to copy the date now */
                pObject->StartTime.date = start_date;
                pObject->StartTime.time = value.type.Time;

                if (datetime_wildcard_present(&pObject->StartTime)) {
                    /* Mark start time as wild carded */
                    pObject->ucTimeFlags |= TL_T_START_WILD;
                    pObject->tStartTime = 0;
                } else {
                    /* Clear wild card flag and set time in local format */
                    pObject->ucTimeFlags &= ~TL_T_START_WILD;
                    pObject->tStartTime =
                        TL_BAC_Time_To_Local(&pObject->StartTime);
                }

                if (bEffectiveEnable != TL_Is_Enabled(wp_data->object_instance)) {
                    /* Enable status has changed because of time update */
                    if (bEffectiveEnable == true) {
                        /* Say we went from enabled to disabled */
                        TL_Insert_Status_Rec(
                            wp_data->object_instance, LOG_STATUS_LOG_DISABLED, true);
                    } else {
                        /* Say we went from disabled to enabled */
                        TL_Insert_Status_Rec(
                            wp_data->object_instance, LOG_STATUS_LOG_DISABLED, false);
                    }
                }
            }
            break;

        case PROP_STOP_TIME:
            /* Copy the date part to safe place */
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_DATE);
            if (!status) {
                break;
            }
            stop_date = value.type.Date;
            /* Then decode the time part */
            len =
                bacapp_decode_application_data(wp_data->application_data + len,
                    wp_data->application_data_len - len, &value);

            if (len) {
                status = write_property_type_valid(
                    wp_data, &value, BACNET_APPLICATION_TAG_TIME);
                if (!status) {
                    break;
                }
                /* First record the current enable state of the log */
                bEffectiveEnable = TL_Is_Enabled(wp_data->object_instance);
                /* Safe to copy the date now */
                pObject->StopTime.date = stop_date;
                pObject->StopTime.time = value.type.Time;

                if (datetime_wildcard_present(&pObject->StopTime)) {
                    /* Mark stop time as wild carded */
                    pObject->ucTimeFlags |= TL_T_STOP_WILD;
                    pObject->tStopTime = datetime_seconds_since_epoch_max();
                } else {
                    /* Clear wild card flag and set time in local format */
                    pObject->ucTimeFlags &= ~TL_T_STOP_WILD;
                    pObject->tStopTime =
                        TL_BAC_Time_To_Local(&pObject->StopTime);
                }

                if (bEffectiveEnable != TL_Is_Enabled(wp_data->object_instance)) {
                    /* Enable status has changed because of time update */
                    if (bEffectiveEnable == true) {
                        /* Say we went from enabled to disabled */
                        TL_Insert_Status_Rec(
                            wp_data->object_instance, LOG_STATUS_LOG_DISABLED, true);
                    } else {
                        /* Say we went from disabled to enabled */
                        TL_Insert_Status_Rec(
                            wp_data->object_instance, LOG_STATUS_LOG_DISABLED, false);
                    }
                }
            }
            break;

        case PROP_LOG_DEVICE_OBJECT_PROPERTY:
            len = bacapp_decode_device_obj_property_ref(
                wp_data->application_data, &TempSource);
            if ((len < 0) ||
                (len > wp_data->application_data_len)) /* Hmm, that didn't go */
            /* as planned... */
            {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_OTHER;
                break;
            }

            /* We only support references to objects in ourself for now */
            if ((TempSource.deviceIdentifier.type == OBJECT_DEVICE) &&
                (TempSource.deviceIdentifier.instance !=
                    Device_Object_Instance_Number())) {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code =
                    ERROR_CODE_OPTIONAL_FUNCTIONALITY_NOT_SUPPORTED;
                break;
            }

            /* Quick comparison if structures are packed ... */
            if (memcmp(&TempSource, &pObject->Source,
                    sizeof(BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE)) != 0) {
                /* Clear buffer if property being logged is changed */
                pObject->ulRecordCount = 0;
                pObject->iIndex = 0;
                TL_Insert_Status_Rec(wp_data->object_instance, LOG_STATUS_BUFFER_PURGED, true);
            }
            pObject->Source = TempSource;
            ucix_add_option_int(ctxw, sec, idx_c, "device_id", pObject->Source.deviceIdentifier.instance);
            ucix_add_option_int(ctxw, sec, idx_c, "object_instance", pObject->Source.objectIdentifier.instance);
            ucix_add_option_int(ctxw, sec, idx_c, "object_type", pObject->Source.objectIdentifier.type);
            ucix_commit(ctxw, sec);

            status = true;
            break;

        case PROP_LOG_INTERVAL:
            if (pObject->LoggingType == LOGGING_TYPE_POLLED) {
                status = write_property_type_valid(
                    wp_data, &value, BACNET_APPLICATION_TAG_UNSIGNED_INT);
                if (status) {
                    /* We only log to 1 sec accuracy so must divide by 100
                        * before passing it on */
                    if (0 < value.type.Unsigned_Int / 100) {
                        pObject->ulLogInterval = value.type.Unsigned_Int / 100;
                    } else {
                        pObject->ulLogInterval =
                            1; /* Interval of 0 is not a good idea */
                    }
                    ucix_add_option_int(ctxw, sec, idx_c, "interval", value.type.Enumerated);
                    ucix_commit(ctxw, sec);
                }
            } else {
                /* Read only if triggered or COV log so flag error and bail out */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            }
            break;

        case PROP_ALIGN_INTERVALS:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                pObject->bAlignIntervals = value.type.Boolean;
            }
            break;

        case PROP_INTERVAL_OFFSET:
            /* We only log to 1 sec accuracy so must divide by 100 before
             * passing it on */
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                pObject->ulIntervalOffset = value.type.Unsigned_Int / 100;
            }
            break;

        case PROP_TRIGGER:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                /* We will not allow triggered operation if polling with
                 * aligning to the clock as that will produce non aligned
                 * readings which goes against the reason for selscting this
                 * mode
                 */
                if ((pObject->LoggingType == LOGGING_TYPE_POLLED) &&
                    (pObject->bAlignIntervals == true)) {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code =
                        ERROR_CODE_NOT_CONFIGURED_FOR_TRIGGERED_LOGGING;
                    status = false;
                } else {
                    pObject->bTrigger = value.type.Boolean;
                }
            }
            break;

        case PROP_DESCRIPTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Trend_Log_Description_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "description",
                        Trend_Log_Description(wp_data->object_instance));
                    ucix_commit(ctxw, sec);
                }
            }
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
    }
    if (ctxw)
        ucix_cleanup(ctxw);

    return status;
}

bool TrendLogGetRRInfo(
    BACNET_READ_RANGE_DATA *pRequest, /* Info on the request */
    RR_PROP_INFO *pInfo)
{ /* Where to put the information */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, pRequest->object_instance);
    if (!pObject) {
        pRequest->error_class = ERROR_CLASS_OBJECT;
        pRequest->error_code = ERROR_CODE_UNKNOWN_OBJECT;
    } else if (pRequest->object_property == PROP_LOG_BUFFER) {
        pInfo->RequestTypes = RR_BY_POSITION | RR_BY_TIME | RR_BY_SEQUENCE;
        pInfo->Handler = rr_trend_log_encode;
        return (true);
    } else {
        pRequest->error_class = ERROR_CLASS_SERVICES;
        pRequest->error_code = ERROR_CODE_PROPERTY_IS_NOT_A_LIST;
    }

    return (false);
}

/*****************************************************************************
 * Insert a status record into a trend log - does not check for enable/log   *
 * full, time slots and so on as these type of entries have to go in         *
 * irrespective of such things which means that valid readings may get       *
 * pushed out of the log to make room.                                       *
 *****************************************************************************/

void TL_Insert_Status_Rec(int iLog, BACNET_LOG_STATUS eStatus, bool bState)
{
    struct object_data *pObject;
    struct tl_data_record TempRec;

    pObject = Keylist_Data(Object_List, iLog);
    if (!pObject)
        return;

    TempRec.tTimeStamp = Trend_Log_Epoch_Seconds_Now();
    TempRec.ucRecType = TL_TYPE_STATUS;
    TempRec.ucStatus = 0;
    TempRec.Datum.ucLogStatus = 0;
    /* Note we set the bits in correct order so that we can place them directly
     * into the bitstring structure later on when we have to encode them */
    switch (eStatus) {
        case LOG_STATUS_LOG_DISABLED:
            if (bState) {
                TempRec.Datum.ucLogStatus = 1 << LOG_STATUS_LOG_DISABLED;
            }
            break;
        case LOG_STATUS_BUFFER_PURGED:
            if (bState) {
                TempRec.Datum.ucLogStatus = 1 << LOG_STATUS_BUFFER_PURGED;
            }
            break;
        case LOG_STATUS_LOG_INTERRUPTED:
            TempRec.Datum.ucLogStatus = 1 << LOG_STATUS_LOG_INTERRUPTED;
            break;
        default:
            break;
    }

    pObject->Logs[pObject->iIndex++] = TempRec;
    if (pObject->iIndex >= TL_MAX_ENTRIES) {
        pObject->iIndex = 0;
    }

    pObject->ulTotalRecordCount++;

    if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
        pObject->ulRecordCount++;
    }
}

/*****************************************************************************
 * Use the combination of the enable flag and the enable times to determine  *
 * if the log is really enabled now. See 135-2008 sections 12.25.5 - 12.25.7 *
 *****************************************************************************/

bool TL_Is_Enabled(int iLog)
{
    struct object_data *pObject;
    bacnet_time_t tNow;
    bool bStatus;

    bStatus = true;
    pObject = Keylist_Data(Object_List, iLog);
    if (!pObject)
        return false;
#if 0
    printf("\nFlags - %u, Start - %u, Stop - %u\n",
        (unsigned int) pObject->ucTimeFlags,
        (unsigned int) pObject->tStartTime,
        (unsigned int) pObject->tStopTime);
#endif
    if (pObject->bEnable == false) {
        /* Not enabled so time is irrelevant */
        bStatus = false;
    } else if ((pObject->ucTimeFlags == 0) &&
        (pObject->tStopTime < pObject->tStartTime)) {
        /* Start time was after stop time as per 12.25.6 and 12.25.7 */
        bStatus = false;
    } else if (pObject->ucTimeFlags != (TL_T_START_WILD | TL_T_STOP_WILD)) {
        /* enabled and either 1 wild card or none */
        tNow = Trend_Log_Epoch_Seconds_Now();
#if 0
        printf("\nFlags - %u, Current - %u, Start - %u, Stop - %u\n",
            (unsigned int) pObject->ucTimeFlags, (unsigned int) Now,
            (unsigned int) pObject->tStartTime,
            (unsigned int) pObject->tStopTime);
#endif
        if ((pObject->ucTimeFlags & TL_T_START_WILD) != 0) {
            /* wild card start time */
            if (tNow > pObject->tStopTime) {
                bStatus = false;
            }
        } else if ((pObject->ucTimeFlags & TL_T_STOP_WILD) != 0) {
            /* wild card stop time */
            if (tNow < pObject->tStartTime) {
                bStatus = false;
            }
        } else {
#if 0
            printf("\nCurrent - %u, Start - %u, Stop - %u\n",
                (unsigned int) Now, (unsigned int) pObject->tStartTime,
                (unsigned int) pObject->tStopTime);
#endif
            /* No wildcards so use both times */
            if ((tNow < pObject->tStartTime) ||
                (tNow > pObject->tStopTime)) {
                bStatus = false;
            }
        }
    }

    return (bStatus);
}

/*****************************************************************************
 * Convert a BACnet time into a local time in seconds since the local epoch  *
 *****************************************************************************/

bacnet_time_t TL_BAC_Time_To_Local(BACNET_DATE_TIME *bdatetime)
{
    return datetime_seconds_since_epoch(bdatetime);
}

/*****************************************************************************
 * Convert a local time in seconds since the local epoch into a BACnet time  *
 *****************************************************************************/

void TL_Local_Time_To_BAC(BACNET_DATE_TIME *bdatetime, bacnet_time_t seconds)
{
    datetime_since_epoch_seconds(bdatetime, seconds);
}

/****************************************************************************
 * Build a list of Trend Log entries from the Log Buffer property as        *
 * required for the ReadsRange functionality.                               *
 *                                                                          *
 * We have to support By Position, By Sequence and By Time requests.        *
 *                                                                          *
 * We do assume the list cannot change whilst we are accessing it so would  *
 * not be multithread safe if there are other tasks that write to the log.  *
 *                                                                          *
 * We take the simple approach here to filling the buffer by taking a max   *
 * size for a single entry and then stopping if there is less than that     *
 * left in the buffer. You could build each entry in a separate buffer and  *
 * determine the exact length before copying but this is time consuming,    *
 * requires more memory and would probably only let you sqeeeze one more    *
 * entry in on occasion. The value is calculated as 10 bytes for the time   *
 * stamp + 6 bytes for our largest data item (bit string capped at 32 bits) *
 * + 3 bytes for the status flags + 4 for the context tags to give 23.      *
 ****************************************************************************/

#define TL_MAX_ENC 23 /* Maximum size of encoded log entry, see above */

int rr_trend_log_encode(uint8_t *apdu, BACNET_READ_RANGE_DATA *pRequest)
{
    struct object_data *pObject;
    /* Initialise result flags to all false */
    bitstring_init(&pRequest->ResultFlags);
    bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_FIRST_ITEM, false);
    bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_LAST_ITEM, false);
    bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_MORE_ITEMS, false);
    pRequest->ItemCount = 0; /* Start out with nothing */

    pObject = Keylist_Data(Object_List, pRequest->object_instance);
    /* Bail out now if nowt - should never happen for a Trend Log but ... */
    if (pObject->ulRecordCount == 0) {
        return (0);
    }

    if ((pRequest->RequestType == RR_BY_POSITION) ||
        (pRequest->RequestType == RR_READ_ALL)) {
        return (TL_encode_by_position(apdu, pRequest));
    } else if (pRequest->RequestType == RR_BY_SEQUENCE) {
        return (TL_encode_by_sequence(apdu, pRequest));
    }

    return (TL_encode_by_time(apdu, pRequest));
}

/****************************************************************************
 * Handle encoding for the By Position and All options.                     *
 * Does All option by converting to a By Position request starting at index *
 * 1 and of maximum log size length.                                        *
 ****************************************************************************/

int TL_encode_by_position(uint8_t *apdu, BACNET_READ_RANGE_DATA *pRequest)
{
    int iLen = 0;
    int32_t iTemp = 0;
    struct object_data *pObject;

    uint32_t uiIndex = 0; /* Current entry number */
    uint32_t uiFirst = 0; /* Entry number we started encoding from */
    uint32_t uiLast = 0; /* Entry number we finished encoding on */
    uint32_t uiTarget = 0; /* Last entry we are required to encode */
    uint32_t uiRemaining = 0; /* Amount of unused space in packet */

    /* See how much space we have */
    uiRemaining = MAX_APDU - pRequest->Overhead;
    pObject = Keylist_Data(Object_List, pRequest->object_instance);
    if (!pObject)
        return (0);
    if (pRequest->RequestType == RR_READ_ALL) {
        /*
         * Read all the list or as much as will fit in the buffer by selecting
         * a range that covers the whole list and falling through to the next
         * section of code
         */
        pRequest->Count = pObject->ulRecordCount; /* Full list */
        pRequest->Range.RefIndex = 1; /* Starting at the beginning */
    }

    if (pRequest->Count <
        0) { /* negative count means work from index backwards */
        /*
         * Convert from end index/negative count to
         * start index/positive count and then process as
         * normal. This assumes that the order to return items
         * is always first to last, if this is not true we will
         * have to handle this differently.
         *
         * Note: We need to be careful about how we convert these
         * values due to the mix of signed and unsigned types - don't
         * try to optimise the code unless you understand all the
         * implications of the data type conversions!
         */

        iTemp = pRequest->Range.RefIndex; /* pull out and convert to signed */
        iTemp +=
            pRequest->Count + 1; /* Adjust backwards, remember count is -ve */
        if (iTemp <
            1) { /* if count is too much, return from 1 to start index */
            pRequest->Count = pRequest->Range.RefIndex;
            pRequest->Range.RefIndex = 1;
        } else { /* Otherwise adjust the start index and make count +ve */
            pRequest->Range.RefIndex = iTemp;
            pRequest->Count = -pRequest->Count;
        }
    }

    /* From here on in we only have a starting point and a positive count */

    if (pRequest->Range.RefIndex >
        pObject->ulRecordCount) { /* Nothing to return as we are past the end
                                      of the list */
        return (0);
    }

    uiTarget = pRequest->Range.RefIndex + pRequest->Count -
        1; /* Index of last required entry */
    if (uiTarget >
        pObject->ulRecordCount) { /* Capped at end of list if necessary */
        uiTarget = pObject->ulRecordCount;
    }

    uiIndex = pRequest->Range.RefIndex;
    uiFirst = uiIndex; /* Record where we started from */
    while (uiIndex <= uiTarget) {
        if (uiRemaining < TL_MAX_ENC) {
            /*
             * Can't fit any more in! We just set the result flag to say there
             * was more and drop out of the loop early
             */
            bitstring_set_bit(
                &pRequest->ResultFlags, RESULT_FLAG_MORE_ITEMS, true);
            break;
        }

        iTemp = TL_encode_entry(&apdu[iLen], pRequest->object_instance, uiIndex);

        uiRemaining -= iTemp; /* Reduce the remaining space */
        iLen += iTemp; /* and increase the length consumed */
        uiLast = uiIndex; /* Record the last entry encoded */
        uiIndex++; /* and get ready for next one */
        pRequest->ItemCount++; /* Chalk up another one for the response count */
    }

    /* Set remaining result flags if necessary */
    if (uiFirst == 1) {
        bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_FIRST_ITEM, true);
    }

    if (uiLast == pObject->ulRecordCount) {
        bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_LAST_ITEM, true);
    }

    return (iLen);
}

/****************************************************************************
 * Handle encoding for the By Sequence option.                              *
 * The fact that the buffer always has at least a single entry is used      *
 * implicetly in the following as we don't have to handle the case of an    *
 * empty buffer.                                                            *
 ****************************************************************************/

int TL_encode_by_sequence(uint8_t *apdu, BACNET_READ_RANGE_DATA *pRequest)
{
    int iLen = 0;
    int32_t iTemp = 0;
    struct object_data *pObject;

    uint32_t uiIndex = 0; /* Current entry number */
    uint32_t uiFirst = 0; /* Entry number we started encoding from */
    uint32_t uiLast = 0; /* Entry number we finished encoding on */
    uint32_t uiSequence = 0; /* Tracking sequenc number when encoding */
    uint32_t uiRemaining = 0; /* Amount of unused space in packet */
    uint32_t uiFirstSeq = 0; /* Sequence number for 1st record in log */

    uint32_t uiBegin = 0; /* Starting Sequence number for request */
    uint32_t uiEnd = 0; /* Ending Sequence number for request */
    bool bWrapReq =
        false; /* Has request sequence range spanned the max for uint32_t? */
    bool bWrapLog =
        false; /* Has log sequence range spanned the max for uint32_t? */

    /* See how much space we have */
    uiRemaining = MAX_APDU - pRequest->Overhead;
    pObject = Keylist_Data(Object_List, pRequest->object_instance);
    if (!pObject)
        return (0);
    /* Figure out the sequence number for the first record, last is
     * ulTotalRecordCount */
    uiFirstSeq =
        pObject->ulTotalRecordCount - (pObject->ulRecordCount - 1);

    /* Calculate start and end sequence numbers from request */
    if (pRequest->Count < 0) {
        uiBegin = pRequest->Range.RefSeqNum + pRequest->Count + 1;
        uiEnd = pRequest->Range.RefSeqNum;
    } else {
        uiBegin = pRequest->Range.RefSeqNum;
        uiEnd = pRequest->Range.RefSeqNum + pRequest->Count - 1;
    }
    /* See if we have any wrap around situations */
    if (uiBegin > uiEnd) {
        bWrapReq = true;
    }
    if (uiFirstSeq > pObject->ulTotalRecordCount) {
        bWrapLog = true;
    }

    if ((bWrapReq == false) && (bWrapLog == false)) { /* Simple case no wraps */
        /* If no overlap between request range and buffer contents bail out */
        if ((uiEnd < uiFirstSeq) ||
            (uiBegin > pObject->ulTotalRecordCount)) {
            return (0);
        }

        /* Truncate range if necessary so it is guaranteed to lie
         * between the first and last sequence numbers in the buffer
         * inclusive.
         */
        if (uiBegin < uiFirstSeq) {
            uiBegin = uiFirstSeq;
        }

        if (uiEnd > pObject->ulTotalRecordCount) {
            uiEnd = pObject->ulTotalRecordCount;
        }
    } else { /* There are wrap arounds to contend with */
        /* First check for non overlap condition as it is common to all */
        if ((uiBegin > pObject->ulTotalRecordCount) &&
            (uiEnd < uiFirstSeq)) {
            return (0);
        }

        if (bWrapLog == false) { /* Only request range wraps */
            if (uiEnd < uiFirstSeq) {
                uiEnd = pObject->ulTotalRecordCount;
                if (uiBegin < uiFirstSeq) {
                    uiBegin = uiFirstSeq;
                }
            } else {
                uiBegin = uiFirstSeq;
                if (uiEnd > pObject->ulTotalRecordCount) {
                    uiEnd = pObject->ulTotalRecordCount;
                }
            }
        } else if (bWrapReq == false) { /* Only log wraps */
            if (uiBegin > pObject->ulTotalRecordCount) {
                if (uiBegin > uiFirstSeq) {
                    uiBegin = uiFirstSeq;
                }
            } else {
                if (uiEnd > pObject->ulTotalRecordCount) {
                    uiEnd = pObject->ulTotalRecordCount;
                }
            }
        } else { /* Both wrap */
            if (uiBegin < uiFirstSeq) {
                uiBegin = uiFirstSeq;
            }

            if (uiEnd > pObject->ulTotalRecordCount) {
                uiEnd = pObject->ulTotalRecordCount;
            }
        }
    }

    /* We now have a range that lies completely within the log buffer
     * and we need to figure out where that starts in the buffer.
     */
    uiIndex = uiBegin - uiFirstSeq + 1;
    uiSequence = uiBegin;
    uiFirst = uiIndex; /* Record where we started from */
    while (uiSequence != uiEnd + 1) {
        if (uiRemaining < TL_MAX_ENC) {
            /*
             * Can't fit any more in! We just set the result flag to say there
             * was more and drop out of the loop early
             */
            bitstring_set_bit(
                &pRequest->ResultFlags, RESULT_FLAG_MORE_ITEMS, true);
            break;
        }

        iTemp = TL_encode_entry(&apdu[iLen], pRequest->object_instance, uiIndex);

        uiRemaining -= iTemp; /* Reduce the remaining space */
        iLen += iTemp; /* and increase the length consumed */
        uiLast = uiIndex; /* Record the last entry encoded */
        uiIndex++; /* and get ready for next one */
        uiSequence++;
        pRequest->ItemCount++; /* Chalk up another one for the response count */
    }

    /* Set remaining result flags if necessary */
    if (uiFirst == 1) {
        bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_FIRST_ITEM, true);
    }

    if (uiLast == pObject->ulRecordCount) {
        bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_LAST_ITEM, true);
    }

    pRequest->FirstSequence = uiBegin;

    return (iLen);
}

/****************************************************************************
 * Handle encoding for the By Time option.                                  *
 * The fact that the buffer always has at least a single entry is used      *
 * implicetly in the following as we don't have to handle the case of an    *
 * empty buffer.                                                            *
 ****************************************************************************/

int TL_encode_by_time(uint8_t *apdu, BACNET_READ_RANGE_DATA *pRequest)
{
    int iLen = 0;
    int32_t iTemp = 0;
    int iCount = 0;
    struct object_data *pObject;

    uint32_t uiIndex = 0; /* Current entry number */
    uint32_t uiFirst = 0; /* Entry number we started encoding from */
    uint32_t uiLast = 0; /* Entry number we finished encoding on */
    uint32_t uiRemaining = 0; /* Amount of unused space in packet */
    uint32_t uiFirstSeq = 0; /* Sequence number for 1st record in log */
    bacnet_time_t tRefTime = 0; /* The time from the request in local format */

    /* See how much space we have */
    uiRemaining = MAX_APDU - pRequest->Overhead;
    pObject = Keylist_Data(Object_List, pRequest->object_instance);
    if (!pObject)
        return (0);

    tRefTime = TL_BAC_Time_To_Local(&pRequest->Range.RefTime);
    /* Find correct position for oldest entry in log */
    if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
        uiIndex = 0;
    } else {
        uiIndex = pObject->iIndex;
    }

    if (pRequest->Count < 0) {
        /* Start at end of log and look for record which has
         * timestamp greater than or equal to the reference.
         */
        iCount = pObject->ulRecordCount - 1;
        /* Start out with the sequence number for the last record */
        uiFirstSeq = pObject->ulTotalRecordCount;
        for (;;) {
            if (pObject->Logs
                    [(uiIndex + iCount) % TL_MAX_ENTRIES]
                        .tTimeStamp < tRefTime) {
                break;
            }

            uiFirstSeq--;
            iCount--;
            if (iCount < 0) {
                return (0);
            }
        }

        /* We have an and point for our request,
         * now work backwards to find where we should start from
         */

        pRequest->Count = -pRequest->Count; /* Conveert to +ve count */
        /* If count would bring us back beyond the limits
         * Of the buffer then pin it to the start of the buffer
         * otherwise adjust starting point and sequence number
         * appropriately.
         */
        iTemp = pRequest->Count - 1;
        if (iTemp > iCount) {
            uiFirstSeq -= iCount;
            pRequest->Count = iCount + 1;
            iCount = 0;
        } else {
            uiFirstSeq -= iTemp;
            iCount -= iTemp;
        }
    } else {
        /* Start at beginning of log and look for 1st record which has
         * timestamp greater than the reference time.
         */
        iCount = 0;
        /* Figure out the sequence number for the first record, last is
         * ulTotalRecordCount */
        uiFirstSeq =
            pObject->ulTotalRecordCount - (pObject->ulRecordCount - 1);
        for (;;) {
            if (pObject->Logs
                    [(uiIndex + iCount) % TL_MAX_ENTRIES]
                        .tTimeStamp > tRefTime) {
                break;
            }

            uiFirstSeq++;
            iCount++;
            if ((uint32_t)iCount == pObject->ulRecordCount) {
                return (0);
            }
        }
    }

    /* We now have a starting point for the operation and a +ve count */

    uiIndex = iCount + 1; /* Convert to BACnet 1 based reference */
    uiFirst = uiIndex; /* Record where we started from */
    iCount = pRequest->Count;
    while (iCount != 0) {
        if (uiRemaining < TL_MAX_ENC) {
            /*
             * Can't fit any more in! We just set the result flag to say there
             * was more and drop out of the loop early
             */
            bitstring_set_bit(
                &pRequest->ResultFlags, RESULT_FLAG_MORE_ITEMS, true);
            break;
        }

        iTemp = TL_encode_entry(&apdu[iLen], pRequest->object_instance, uiIndex);

        uiRemaining -= iTemp; /* Reduce the remaining space */
        iLen += iTemp; /* and increase the length consumed */
        uiLast = uiIndex; /* Record the last entry encoded */
        uiIndex++; /* and get ready for next one */
        pRequest->ItemCount++; /* Chalk up another one for the response count */
        iCount--; /* And finally cross another one off the requested count */

        if (uiIndex >
            pObject
                ->ulRecordCount) { /* Finish up if we hit the end of the log */
            break;
        }
    }

    /* Set remaining result flags if necessary */
    if (uiFirst == 1) {
        bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_FIRST_ITEM, true);
    }

    if (uiLast == pObject->ulRecordCount) {
        bitstring_set_bit(&pRequest->ResultFlags, RESULT_FLAG_LAST_ITEM, true);
    }

    pRequest->FirstSequence = uiFirstSeq;

    return (iLen);
}

int TL_encode_entry(uint8_t *apdu, int iLog, int iEntry)
{
    int iLen = 0;
    struct tl_data_record *pSource = NULL;
    BACNET_BIT_STRING TempBits;
    uint8_t ucCount = 0;
    BACNET_DATE_TIME TempTime;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, iLog);
    if (!pObject)
        return (0);

    /* Convert from BACnet 1 based to 0 based array index and then
     * handle wrap around of the circular buffer */

    if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
        pSource = &pObject->Logs[(iEntry - 1) % TL_MAX_ENTRIES];
    } else {
        pSource =
            &pObject->Logs[(pObject->iIndex + iEntry - 1) % TL_MAX_ENTRIES];
    }

    iLen = 0;
    /* First stick the time stamp in with tag [0] */
    TL_Local_Time_To_BAC(&TempTime, pSource->tTimeStamp);
    iLen += bacapp_encode_context_datetime(apdu, 0, &TempTime);

    /* Next comes the actual entry with tag [1] */
    iLen += encode_opening_tag(&apdu[iLen], 1);
    /* The data entry is tagged individually [0] - [10] to indicate which type
     */
    switch (pSource->ucRecType) {
        case TL_TYPE_STATUS:
            /* Build bit string directly from the stored octet */
            bitstring_init(&TempBits);
            bitstring_set_bits_used(&TempBits, 1, 5);
            bitstring_set_octet(&TempBits, 0, pSource->Datum.ucLogStatus);
            iLen += encode_context_bitstring(
                &apdu[iLen], pSource->ucRecType, &TempBits);
            break;

        case TL_TYPE_BOOL:
            iLen += encode_context_boolean(
                &apdu[iLen], pSource->ucRecType, pSource->Datum.ucBoolean);
            break;

        case TL_TYPE_REAL:
            iLen += encode_context_real(
                &apdu[iLen], pSource->ucRecType, pSource->Datum.fReal);
            break;

        case TL_TYPE_ENUM:
            iLen += encode_context_enumerated(
                &apdu[iLen], pSource->ucRecType, pSource->Datum.ulEnum);
            break;

        case TL_TYPE_UNSIGN:
            iLen += encode_context_unsigned(
                &apdu[iLen], pSource->ucRecType, pSource->Datum.ulUValue);
            break;

        case TL_TYPE_SIGN:
            iLen += encode_context_signed(
                &apdu[iLen], pSource->ucRecType, pSource->Datum.lSValue);
            break;

        case TL_TYPE_BITS:
            /* Rebuild bitstring directly from stored octets - which we
             * have limited to 32 bits maximum as allowed by the standard
             */
            bitstring_init(&TempBits);
            bitstring_set_bits_used(&TempBits,
                (pSource->Datum.Bits.ucLen >> 4) & 0x0F,
                pSource->Datum.Bits.ucLen & 0x0F);
            for (ucCount = pSource->Datum.Bits.ucLen >> 4; ucCount > 0;
                 ucCount--) {
                bitstring_set_octet(&TempBits, ucCount - 1,
                    pSource->Datum.Bits.ucStore[ucCount - 1]);
            }

            iLen += encode_context_bitstring(
                &apdu[iLen], pSource->ucRecType, &TempBits);
            break;

        case TL_TYPE_NULL:
            iLen += encode_context_null(&apdu[iLen], pSource->ucRecType);
            break;

        case TL_TYPE_ERROR:
            iLen += encode_opening_tag(&apdu[iLen], TL_TYPE_ERROR);
            iLen += encode_application_enumerated(
                &apdu[iLen], pSource->Datum.Error.usClass);
            iLen += encode_application_enumerated(
                &apdu[iLen], pSource->Datum.Error.usCode);
            iLen += encode_closing_tag(&apdu[iLen], TL_TYPE_ERROR);
            break;

        case TL_TYPE_DELTA:
            iLen += encode_context_real(
                &apdu[iLen], pSource->ucRecType, pSource->Datum.fTime);
            break;

        case TL_TYPE_ANY:
            /* Should never happen as we don't support this at the moment */
            break;

        default:
            break;
    }

    iLen += encode_closing_tag(&apdu[iLen], 1);
    /* Check if status bit string is required and insert with tag [2] */
    //if ((pSource->ucStatus & 128) == 128) {
        //iLen += encode_opening_tag(&apdu[iLen], 2);
        //bitstring_init(&TempBits);
        //bitstring_set_bits_used(&TempBits, 1, 4);
        /* only insert the 1st 4 bits */
        //bitstring_set_octet(&TempBits, 0, (pSource->ucStatus & 0x0F));
        //iLen += encode_context_bitstring(&apdu[iLen], 2, &TempBits);
        //iLen += encode_application_bitstring(&apdu[iLen], &TempBits);
        //iLen += encode_closing_tag(&apdu[iLen], 2);
    //}

    return (iLen);
}

static int local_read_property(uint8_t *value,
    uint8_t *status,
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *Source,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    int len = 0;
    BACNET_READ_PROPERTY_DATA rpdata;

    if (value != NULL) {
        /* configure our storage */
        rpdata.application_data = value;
        rpdata.application_data_len = MAX_APDU;
        rpdata.object_type = Source->objectIdentifier.type;
        rpdata.object_instance = Source->objectIdentifier.instance;
        rpdata.object_property = Source->propertyIdentifier;
        rpdata.array_index = Source->arrayIndex;
        /* Try to fetch the required property */
        len = Device_Read_Property(&rpdata);
        if (len < 0) {
            *error_class = rpdata.error_class;
            *error_code = rpdata.error_code;
        }
    }

    if ((len >= 0) && (status != NULL)) {
        /* Fetch the status flags if required */
        rpdata.application_data = status;
        rpdata.application_data_len = MAX_APDU;
        rpdata.object_property = PROP_STATUS_FLAGS;
        rpdata.array_index = BACNET_ARRAY_ALL;
        len = Device_Read_Property(&rpdata);
        if (len < 0) {
            *error_class = rpdata.error_class;
            *error_code = rpdata.error_code;
        }
    }

    return (len);
}

static void write_property_to_rec(BACNET_APPLICATION_DATA_VALUE value,
    int iCount, uint8_t StatusBuf[3]
    )
{
    int iLen = 0;
    BACNET_BIT_STRING TempBits;
    uint8_t tag_number = 0;
    uint32_t len_value_type = 0;
    uint8_t ucCount;
    struct object_data *pObject;
    struct tl_data_record TempRec;
    pObject = Keylist_Data_Index(Object_List, iCount);
    switch (value.tag) {
    case BACNET_APPLICATION_TAG_NULL:
        TempRec.ucRecType = TL_TYPE_NULL;
        break;

    case BACNET_APPLICATION_TAG_BOOLEAN:
        /* Record the current time in the log entry and also in the info block
        * for the log so we can figure out when the next reading is due */
        TempRec.tTimeStamp = Trend_Log_Epoch_Seconds_Now();
        pObject->tLastDataTime = TempRec.tTimeStamp;
        TempRec.ucStatus = 0;
        TempRec.ucRecType = TL_TYPE_BOOL;
        TempRec.Datum.ucBoolean = value.type.Boolean;
        /* Finally insert the status flags into the record */
        iLen = decode_tag_number_and_value(
            StatusBuf, &tag_number, &len_value_type);
        decode_bitstring(&StatusBuf[iLen], len_value_type, &TempBits);
        TempRec.ucStatus = 128 | bitstring_octet(&TempBits, 0);
        pObject->Logs[pObject->iIndex++] = TempRec;
        if (pObject->iIndex >= TL_MAX_ENTRIES) {
            pObject->iIndex = 0;
        }
        pObject->ulTotalRecordCount++;
        if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
            pObject->ulRecordCount++;
        }
        break;

    case BACNET_APPLICATION_TAG_UNSIGNED_INT:
        /* Record the current time in the log entry and also in the info block
        * for the log so we can figure out when the next reading is due */
        TempRec.tTimeStamp = Trend_Log_Epoch_Seconds_Now();
        pObject->tLastDataTime = TempRec.tTimeStamp;
        TempRec.ucStatus = 0;
        TempRec.ucRecType = TL_TYPE_UNSIGN;
        TempRec.Datum.ulUValue = value.type.Unsigned_Int;
        /* Finally insert the status flags into the record */
        iLen = decode_tag_number_and_value(
            StatusBuf, &tag_number, &len_value_type);
        decode_bitstring(&StatusBuf[iLen], len_value_type, &TempBits);
        TempRec.ucStatus = 128 | bitstring_octet(&TempBits, 0);
        pObject->Logs[pObject->iIndex++] = TempRec;
        if (pObject->iIndex >= TL_MAX_ENTRIES) {
            pObject->iIndex = 0;
        }
        pObject->ulTotalRecordCount++;
        if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
            pObject->ulRecordCount++;
        }
        break;

    case BACNET_APPLICATION_TAG_SIGNED_INT:
        /* Record the current time in the log entry and also in the info block
        * for the log so we can figure out when the next reading is due */
        TempRec.tTimeStamp = Trend_Log_Epoch_Seconds_Now();
        pObject->tLastDataTime = TempRec.tTimeStamp;
        TempRec.ucStatus = 0;
        TempRec.ucRecType = TL_TYPE_SIGN;
        TempRec.Datum.lSValue = value.type.Signed_Int;
        /* Finally insert the status flags into the record */
        iLen = decode_tag_number_and_value(
            StatusBuf, &tag_number, &len_value_type);
        decode_bitstring(&StatusBuf[iLen], len_value_type, &TempBits);
        TempRec.ucStatus = 128 | bitstring_octet(&TempBits, 0);
        pObject->Logs[pObject->iIndex++] = TempRec;
        if (pObject->iIndex >= TL_MAX_ENTRIES) {
            pObject->iIndex = 0;
        }
        pObject->ulTotalRecordCount++;
        if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
            pObject->ulRecordCount++;
        }
        break;

    case BACNET_APPLICATION_TAG_REAL:
        /* Record the current time in the log entry and also in the info block
        * for the log so we can figure out when the next reading is due */
        TempRec.tTimeStamp = Trend_Log_Epoch_Seconds_Now();
        pObject->tLastDataTime = TempRec.tTimeStamp;
        TempRec.ucStatus = 0;
        TempRec.ucRecType = TL_TYPE_REAL;
        TempRec.Datum.fReal = value.type.Real;
        /* Finally insert the status flags into the record */
        iLen = decode_tag_number_and_value(
            StatusBuf, &tag_number, &len_value_type);
        decode_bitstring(&StatusBuf[iLen], len_value_type, &TempBits);
        TempRec.ucStatus = 128 | bitstring_octet(&TempBits, 0);
        pObject->Logs[pObject->iIndex++] = TempRec;
        if (pObject->iIndex >= TL_MAX_ENTRIES) {
            pObject->iIndex = 0;
        }
        pObject->ulTotalRecordCount++;
        if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
            pObject->ulRecordCount++;
        }
        break;
    case BACNET_APPLICATION_TAG_BIT_STRING:
        /* Record the current time in the log entry and also in the info block
        * for the log so we can figure out when the next reading is due */
        TempRec.tTimeStamp = Trend_Log_Epoch_Seconds_Now();
        pObject->tLastDataTime = TempRec.tTimeStamp;
        TempRec.ucStatus = 0;
        TempRec.ucRecType = TL_TYPE_BITS;
        /* We truncate any bitstrings at 32 bits to conserve space */
        if (bitstring_bits_used(&value.type.Bit_String) < 32) {
            /* Store the bytes used and the bits free in the last byte
            */
            TempRec.Datum.Bits.ucLen = bitstring_bytes_used(&value.type.Bit_String)
                << 4;
            TempRec.Datum.Bits.ucLen |=
                (8 - (bitstring_bits_used(&value.type.Bit_String) % 8)) & 7;
            /* Fetch the octets with the bits directly */
            for (ucCount = 0; ucCount < bitstring_bytes_used(&value.type.Bit_String);
                ucCount++) {
                TempRec.Datum.Bits.ucStore[ucCount] =
                    bitstring_octet(&value.type.Bit_String, ucCount);
            }
        } else {
            /* We will only use the first 4 octets to save space */
            TempRec.Datum.Bits.ucLen = 4 << 4;
            for (ucCount = 0; ucCount < 4; ucCount++) {
                TempRec.Datum.Bits.ucStore[ucCount] =
                    bitstring_octet(&value.type.Bit_String, ucCount);
            }
        }
        /* Finally insert the status flags into the record */
        iLen = decode_tag_number_and_value(
            StatusBuf, &tag_number, &len_value_type);
        decode_bitstring(&StatusBuf[iLen], len_value_type, &TempBits);
        TempRec.ucStatus = 128 | bitstring_octet(&TempBits, 0);
        pObject->Logs[pObject->iIndex++] = TempRec;
        if (pObject->iIndex >= TL_MAX_ENTRIES) {
            pObject->iIndex = 0;
        }
        pObject->ulTotalRecordCount++;
        if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
            pObject->ulRecordCount++;
        }
        break;

    case BACNET_APPLICATION_TAG_ENUMERATED:
        /* Record the current time in the log entry and also in the info block
        * for the log so we can figure out when the next reading is due */
        TempRec.tTimeStamp = Trend_Log_Epoch_Seconds_Now();
        pObject->tLastDataTime = TempRec.tTimeStamp;
        TempRec.ucStatus = 0;
        TempRec.ucRecType = TL_TYPE_ENUM;
        TempRec.Datum.ulEnum = value.type.Enumerated;
        /* Finally insert the status flags into the record */
        iLen = decode_tag_number_and_value(
            StatusBuf, &tag_number, &len_value_type);
        decode_bitstring(&StatusBuf[iLen], len_value_type, &TempBits);
        TempRec.ucStatus = 128 | bitstring_octet(&TempBits, 0);
        pObject->Logs[pObject->iIndex++] = TempRec;
        if (pObject->iIndex >= TL_MAX_ENTRIES) {
            pObject->iIndex = 0;
        }
        pObject->ulTotalRecordCount++;
        if (pObject->ulRecordCount < TL_MAX_ENTRIES) {
            pObject->ulRecordCount++;
        }
        break;
    default:
        break;
    }
}

/** Handler for a ReadProperty ACK.
 * @ingroup DSRP
 * Doesn't actually do anything, except, for debugging, to
 * print out the ACK data of a matching request.
 *
 * @param service_request [in] The contents of the service request.
 * @param service_len [in] The length of the service_request.
 * @param src [in] BACNET_ADDRESS of the source of the message
 * @param service_data [in] The BACNET_CONFIRMED_SERVICE_DATA information
 *                          decoded from the APDU header of this message.
 */
void trend_log_read_property_ack_handler(uint8_t *service_request,
    uint16_t service_len,
    BACNET_ADDRESS *src,
    BACNET_CONFIRMED_SERVICE_ACK_DATA *service_data)
{
    struct object_data *pObject;
    int iCount = 0;
    BACNET_READ_PROPERTY_DATA data;
    int len = 0;
    uint8_t *application_data;
    int application_data_len;
    BACNET_APPLICATION_DATA_VALUE value; /* for decode value data */
    for (iCount = 0; iCount < Keylist_Count(Object_List);
        iCount++) {
        pObject = Keylist_Data_Index(Object_List, iCount);
        if (address_match(&pObject->Target_Address, src) &&
            (service_data->invoke_id == pObject->Request_Invoke_ID)) {
            len =
                rp_ack_decode_service_request(service_request,
                service_len, &data);
            if (len > 0) {
                application_data = data.application_data;
                application_data_len = data.application_data_len;
                /* FIXME: what if application_data_len is bigger than 255? */
                /* value? need to loop until all of the len is gone... */
                for (;;) {
                    len = bacapp_decode_known_property(application_data,
                        (unsigned)application_data_len, &value, data.object_type,
                        data.object_property);

                    if (len < 0) {
                        break;
                    }

                    write_property_to_rec(value,iCount,0);

                    if (len > 0) {
                        if (len < application_data_len) {
                            application_data += len;
                            application_data_len -= len;
                        } else {
                            break;
                        }
                    } else {
                        break;
                    }
                }
            }
        }
    }
}

/****************************************************************************
 * Attempt to fetch the logged property and store it in the Trend Log       *
 ****************************************************************************/

static void TL_fetch_property(int iLog)
{
    uint8_t ValueBuf[MAX_APDU]; /* This is a big buffer in case someone selects
                                   the device object list for example */
    uint8_t StatusBuf[3]; /* Should be tag, bits unused in last octet and 1 byte
                             of data */
    BACNET_ERROR_CLASS error_class = ERROR_CLASS_SERVICES;
    BACNET_ERROR_CODE error_code = ERROR_CODE_OTHER;
    int iLen = 0;
    uint8_t tag_number = 0;
    uint32_t len_value_type = 0;
    struct object_data *pObject;
    unsigned max_apdu = 0;
    BACNET_APPLICATION_DATA_VALUE value; /* for decode value data */

    pObject = Keylist_Data(Object_List, iLog);
    if (!pObject)
        return;

    if (pObject->Source.deviceIdentifier.instance == Device_Object_Instance_Number()) {

        iLen = local_read_property(
            ValueBuf, StatusBuf, &pObject->Source, &error_class, &error_code);
        if (iLen > 0) {
            /* Decode data returned and see if we can fit it into the log */
            iLen =
                decode_tag_number_and_value(ValueBuf, &tag_number, &len_value_type);
            value.tag = tag_number;

            switch (tag_number) {
                case BACNET_APPLICATION_TAG_BOOLEAN:
                    value.type.Boolean = decode_boolean(len_value_type);
                    write_property_to_rec(value,iLog,StatusBuf);
                    break;

                case BACNET_APPLICATION_TAG_UNSIGNED_INT:
                    decode_unsigned(
                        &ValueBuf[iLen], len_value_type, &value.type.Unsigned_Int);
                    write_property_to_rec(value,iLog,StatusBuf);
                    break;

                case BACNET_APPLICATION_TAG_SIGNED_INT:
                    decode_signed(
                        &ValueBuf[iLen], len_value_type, &value.type.Signed_Int);
                    write_property_to_rec(value,iLog,StatusBuf);
                    break;

                case BACNET_APPLICATION_TAG_REAL:
                    decode_real_safe(
                        &ValueBuf[iLen], len_value_type, &value.type.Real);
                    write_property_to_rec(value,iLog,StatusBuf);
                    break;

                case BACNET_APPLICATION_TAG_BIT_STRING:
                    decode_bitstring(&ValueBuf[iLen], len_value_type, &value.type.Bit_String);
                    write_property_to_rec(value,iLog,StatusBuf);
                    break;

                case BACNET_APPLICATION_TAG_ENUMERATED:
                    decode_enumerated(
                        &ValueBuf[iLen], len_value_type, &value.type.Enumerated);
                    write_property_to_rec(value,iLog,StatusBuf);
                    break;

                default:
                    break;
            }
        }
    } else {
        if (!pObject->found) {
            Send_WhoIs( pObject->Source.deviceIdentifier.instance,
            pObject->Source.deviceIdentifier.instance);
        }
        if (!pObject->found) {
            pObject->found = address_bind_request(
                pObject->Source.deviceIdentifier.instance, &max_apdu,
                &pObject->Target_Address);
        }
        if (pObject->found) {
            pObject->Request_Invoke_ID =
                Send_Read_Property_Request(pObject->Source.deviceIdentifier.instance,
                pObject->Source.objectIdentifier.type,
                pObject->Source.objectIdentifier.instance,
                PROP_PRESENT_VALUE,
                BACNET_ARRAY_ALL);
        }
    }

}

void trend_log_writepropertysimpleackhandler(
    BACNET_ADDRESS *src, uint8_t invoke_id)
{
    struct object_data *pObject;
    int iCount = 0;
    for (iCount = 0; iCount < Keylist_Count(Object_List);
        iCount++) {
        pObject = Keylist_Data_Index(Object_List, iCount);
        if (address_match(&pObject->Target_Address, src) &&
        (invoke_id == pObject->Request_Invoke_ID)) {
            pObject->Simple_Ack_Detected = true;
        }
    }
}

void trend_log_unconfirmed_cov_notification_handler(
    uint8_t *service_request, uint16_t service_len, BACNET_ADDRESS *src)
{
    //todo
    //printf("Subscribe unconfirmed COV Notification!\n");
    handler_ucov_notification(service_request, service_len, src);
}

void trend_log_confirmed_cov_notification_handler(uint8_t *service_request,
    uint16_t service_len,
    BACNET_ADDRESS *src,
    BACNET_CONFIRMED_SERVICE_DATA *service_data)
{
    struct object_data *pObject;
    int iCount = 0;
    BACNET_COV_DATA cov_data;
    BACNET_PROPERTY_VALUE property_value[MAX_COV_PROPERTIES];
    BACNET_PROPERTY_VALUE *pProperty_value = NULL;
    int len = 0;
    handler_ccov_notification(service_request, service_len, src, service_data);
    for (iCount = 0; iCount < Keylist_Count(Object_List);
        iCount++) {
        pObject = Keylist_Data_Index(Object_List, iCount);
        if (address_match(&pObject->Target_Address, src)) {
            /* create linked list to store data if more
            than one property value is expected */
            bacapp_property_value_list_init(&property_value[0], MAX_COV_PROPERTIES);
            cov_data.listOfValues = &property_value[0];
            /* decode the service request only */
            len = cov_notify_decode_service_request(
                service_request, service_len, &cov_data);
            if (len > 0) {
                if (cov_data.subscriberProcessIdentifier ==
                pObject->cov_data.subscriberProcessIdentifier) {
                    pProperty_value = &property_value[0];
                    while (pProperty_value) {
                        if (pProperty_value->propertyIdentifier == PROP_PRESENT_VALUE) {
                            write_property_to_rec(pProperty_value->value,iCount,0);
                        }
                        pProperty_value = pProperty_value->next;
                    }
                }
            }
        }
    }
}

/****************************************************************************
 * Check each log to see if any data needs to be recorded.                  *
 ****************************************************************************/

void trend_log_timer(uint16_t uSeconds)
{
    struct object_data *pObject;
    int iCount = 0;
    bacnet_time_t tNow = 0;
    unsigned max_apdu = 0;

    (void)uSeconds;
    /* use OS to get the current time */
    tNow = Trend_Log_Epoch_Seconds_Now();
    for (iCount = 0; iCount < Keylist_Count(Object_List);
        iCount++) {
        pObject = Keylist_Data_Index(Object_List, iCount);
        if (TL_Is_Enabled(iCount)) {
            if (pObject->LoggingType == LOGGING_TYPE_POLLED) {
                /* For polled logs we first need to see if they are clock
                 * aligned or not.
                 */
                if (pObject->bAlignIntervals == true) {
                    /* Aligned logging so use the combination of the interval
                     * and the offset to decide when to log. Also log a reading
                     * if more than interval time has elapsed since last reading
                     * to ensure we don't miss a reading if we aren't called at
                     * the precise second when the match occurs.
                     */
                    /*                if(((tNow % pObject->ulLogInterval) >=
                     * (pObject->ulIntervalOffset %
                     * pObject->ulLogInterval)) && */
                    /*                   ((tNow - pObject->tLastDataTime) >=
                     * pObject->ulLogInterval)) { */
                    if ((tNow % pObject->ulLogInterval) ==
                        (pObject->ulIntervalOffset %
                            pObject->ulLogInterval)) {
                        /* Record value if time synchronised trigger condition
                         * is met and at least one period has elapsed.
                         */
                        TL_fetch_property(iCount);
                    } else if ((tNow - pObject->tLastDataTime) >
                        pObject->ulLogInterval) {
                        /* Also record value if we have waited more than a
                         * period since the last reading. This ensures we take a
                         * reading as soon as possible after a power down if we
                         * have been off for more than a single period.
                         */
                        TL_fetch_property(iCount);
                    }
                } else if (((tNow - pObject->tLastDataTime) >=
                               pObject->ulLogInterval) ||
                    (pObject->bTrigger == true)) {
                    /* If not aligned take a reading when we have either waited
                     * long enough or a trigger is set.
                     */
                    TL_fetch_property(iCount);
                }

                pObject->bTrigger = false; /* Clear this every time */
            } else if (pObject->LoggingType == LOGGING_TYPE_TRIGGERED) {
                /* Triggered logs take a reading when the trigger is set and
                 * then reset the trigger to wait for the next event
                 */
                if (pObject->bTrigger == true) {
                    TL_fetch_property(iCount);
                    pObject->bTrigger = false;
                }
            } else if (pObject->LoggingType == LOGGING_TYPE_COV) {
                /* COV logs */
                /* increment timer - exit if timed out */
                pObject->current_seconds = time(NULL);
                /* at least one second has passed */
                if (pObject->current_seconds != pObject->last_seconds) {
                    /* increment timer - exit if timed out */
                    pObject->delta_seconds = pObject->current_seconds - pObject->last_seconds;
                    pObject->elapsed_seconds += pObject->delta_seconds;
                    tsm_timer_milliseconds((pObject->delta_seconds * 1000));
                    /* keep track of time for next check */
                    pObject->last_seconds = pObject->current_seconds;
                }
                if (!pObject->found) {
                    pObject->found = address_bind_request(
                        pObject->Source.deviceIdentifier.instance, &max_apdu,
                        &pObject->Target_Address);
                }
                if (!pObject->found) {
                    Send_WhoIs( pObject->Source.deviceIdentifier.instance,
                    pObject->Source.deviceIdentifier.instance);
                }
                if (pObject->found) {
                    if (pObject->Request_Invoke_ID == 0) {
                        pObject->Simple_Ack_Detected = false;
                        pObject->Request_Invoke_ID =
                            Send_COV_Subscribe(pObject->Source.deviceIdentifier.instance,
                            &pObject->cov_data);
                        if (!pObject->cov_data.cancellationRequest &&
                            (pObject->timeout_seconds < pObject->cov_data.lifetime)) {
                            /* increase the timeout to the longest lifetime */
                            pObject->timeout_seconds = pObject->cov_data.lifetime;
                        }
                    } else if (tsm_invoke_id_free(pObject->Request_Invoke_ID)) {
                        if (pObject->cov_data.cancellationRequest &&
                        pObject->Simple_Ack_Detected) {
                            pObject->found = NULL;
                        }
                    } else if (tsm_invoke_id_failed(pObject->Request_Invoke_ID)) {
                        tsm_free_invoke_id(pObject->Request_Invoke_ID);
                        pObject->Error_Detected = true;
                        pObject->found = NULL;
                    }
                } else {
                    /* exit if timed out */
                    if (pObject->elapsed_seconds > pObject->timeout_seconds) {
                        pObject->Error_Detected = true;
                        pObject->found = NULL;
                    }
                }
                /* COV - so just wait until lifetime value expires */
                if (pObject->elapsed_seconds > pObject->timeout_seconds) {
                    tsm_free_invoke_id(pObject->Request_Invoke_ID);
                    pObject->Request_Invoke_ID = 0;
                    pObject->elapsed_seconds = 0;
                    pObject->found = NULL;
                }
            }
        }
    }
}

/* structure to hold tuple-list and uci context during iteration */
struct itr_ctx {
	struct uci_context *ctx;
	const char *section;
    struct object_data_t Object;
};

static void uci_list(const char *sec_idx,
	struct itr_ctx *ictx)
{
	int disable,idx;
	disable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx, "default") == 0)
		return;
	if (disable)
		return;
    idx = atoi(sec_idx);
    struct object_data *pObject = NULL;
    int index = 0;
    pObject = calloc(1, sizeof(struct object_data));
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "name");
    if (option && characterstring_init_ansi(&option_str, option))
        pObject->Object_Name = strndup(option,option_str.length);

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "description");
    if (option && characterstring_init_ansi(&option_str, option))
        pObject->Description = strndup(option,option_str.length);
    else
        pObject->Description = strdup(ictx->Object.Description);

    pObject->bAlignIntervals = true;
    pObject->bEnable = true;
    pObject->bStopWhenFull = false;
    pObject->bTrigger = false;
    pObject->LoggingType = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "type", ictx->Object.LoggingType);
    pObject->ulIntervalOffset = 0;
    pObject->iIndex = 0;
    pObject->ulLogInterval = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "interval", ictx->Object.ulLogInterval);
    pObject->ulRecordCount = 0;
    pObject->ulTotalRecordCount = 0;

    pObject->Source.deviceIdentifier.instance =
        ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "device_id", ictx->Object.Source.deviceIdentifier.instance);
    pObject->Source.deviceIdentifier.type = OBJECT_DEVICE;
    pObject->Source.objectIdentifier.instance = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "object_instance", 0);
    pObject->Source.objectIdentifier.type = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "object_type", 0);
    pObject->Source.arrayIndex = BACNET_ARRAY_ALL;
    pObject->Source.propertyIdentifier = PROP_PRESENT_VALUE;

    pObject->ucTimeFlags |= TL_T_STOP_WILD;
    pObject->ucTimeFlags |= TL_T_START_WILD;

    if (pObject->LoggingType == LOGGING_TYPE_COV) {
        int32_t PID = 0;
        PID = Keylist_Count(Object_List); PID++; PID = PID*2;
        pObject->max_apdu = 0;
        //pObject->cov_data.covIncrement = 10.0;
        //pObject->cov_data.covIncrementPresent = 10.0;
        pObject->cov_data.covSubscribeToProperty = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "subscribetoproperty", ictx->Object.cov_data.covSubscribeToProperty);
        pObject->cov_data.monitoredProperty.propertyIdentifier = PROP_PRESENT_VALUE;
        pObject->cov_data.monitoredObjectIdentifier = pObject->Source.objectIdentifier;
        pObject->cov_data.subscriberProcessIdentifier = PID;
        pObject->cov_data.cancellationRequest = false;
        pObject->cov_data.issueConfirmedNotifications = true;
        pObject->cov_data.lifetime = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "lifetime", ictx->Object.cov_data.lifetime);;
        pObject->Request_Invoke_ID = 0;
        pObject->Simple_Ack_Detected = false;
        /* configure the timeout values */
        pObject->elapsed_seconds = 0;
        pObject->last_seconds = time(NULL);
        pObject->current_seconds = 0;
        pObject->timeout_seconds = (apdu_timeout() / 1000) * apdu_retries();
        pObject->delta_seconds = 0;
        pObject->found = false;
        pObject->Error_Detected = false;
    }

    /* add to list */
    index = Keylist_Data_Add(Object_List, idx, pObject);
    if (index >= 0) {
        Device_Inc_Database_Revision();
    }
    return;
}

void Trend_Log_Init(void)
{
    Object_List = Keylist_Create();
    struct uci_context *ctx;
    ctx = ucix_init(sec);
    if (!ctx)
        fprintf(stderr, "Failed to load config file %s\n",sec);
    struct object_data_t tObject;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;

    option = ucix_get_option(ctx, sec, "default", "description");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Description = strndup(option,option_str.length);
    else
        tObject.Description = "Trendlog";
    tObject.ulLogInterval = ucix_get_option_int(ctx, sec, "default", "interval", 900);
    tObject.LoggingType = ucix_get_option_int(ctx, sec, "default", "type", LOGGING_TYPE_POLLED);
    tObject.Source.deviceIdentifier.instance = ucix_get_option_int(ctx, sec, "default", "device_id",
        Device_Object_Instance_Number());
    tObject.cov_data.covSubscribeToProperty = ucix_get_option_int(ctx, sec, "default", "subscribetoproperty", 0);
    tObject.cov_data.lifetime = ucix_get_option_int(ctx, sec, "default", "lifetime", 300);
    struct itr_ctx itr_m;
	itr_m.section = sec;
	itr_m.ctx = ctx;
	itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void (*)(const char *, void *))uci_list, &itr_m);
    if (ctx)
        ucix_cleanup(ctx);
}
