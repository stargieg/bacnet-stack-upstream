/**************************************************************************
 *
 * Copyright (C) 2015 Nikola Jelic <nikola.jelic@euroicc.com>
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

#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/bactext.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/ucix/ucix.h"
#include "bacnet/config.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
#include "bacnet/proplist.h"
#include "bacnet/timestamp.h"
#include "bacnet/basic/object/schedule.h"

static const char *sec = "bacnet_sc";
static const char *type = "sc";

struct object_data {
    /* Effective Period: Start and End Date */
    BACNET_DATE Start_Date;
    BACNET_DATE End_Date;
    /* Properties concerning Present Value */
    BACNET_OBJ_DAILY_SCHEDULE Weekly_Schedule[7];
    BACNET_APPLICATION_DATA_VALUE Schedule_Default;
    /*
        * Caution: This is a converted to BACNET_PRIMITIVE_APPLICATION_DATA_VALUE.
        * Only some data types may be used!
        */
    BACNET_APPLICATION_DATA_VALUE Present_Value;   /* must be set to a valid value
                                                        * default is Schedule_Default */
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE
        Object_Property_References[BACNET_SCHEDULE_OBJ_PROP_REF_SIZE];
    uint8_t obj_prop_ref_cnt;       /* actual number of obj_prop references */
    uint8_t Priority_For_Writing;   /* (1..16) */
    bool Out_Of_Service;
    bool Changed;
    const char *Object_Name;
    const char *Description;
};

struct object_data_t {
    uint8_t Priority_For_Writing;   /* (1..16) */
    const char *Object_Name;
    const char *Description;
};

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_SCHEDULE;


static const int Schedule_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE,
    PROP_EFFECTIVE_PERIOD, PROP_SCHEDULE_DEFAULT,
    PROP_LIST_OF_OBJECT_PROPERTY_REFERENCES, PROP_PRIORITY_FOR_WRITING,
    PROP_STATUS_FLAGS, PROP_RELIABILITY, PROP_OUT_OF_SERVICE, -1 };

static const int Schedule_Properties_Optional[] = { PROP_WEEKLY_SCHEDULE, -1 };

static const int Schedule_Properties_Proprietary[] = { -1 };

void Schedule_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Schedule_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Schedule_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Schedule_Properties_Proprietary;
    }
}

bool Schedule_Valid_Instance(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return true;
    }

    return false;
}

unsigned Schedule_Count(void)
{
    return Keylist_Count(Object_List);
}

uint32_t Schedule_Index_To_Instance(unsigned index)
{
    return Keylist_Key(Object_List, index);
}

unsigned Schedule_Instance_To_Index(uint32_t object_instance)
{
    return Keylist_Index(Object_List, object_instance);
}

bool Schedule_Object_Name(
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

/* 	BACnet Testing Observed Incident oi00106
        Out of service was not supported by Schedule object
        Revealed by BACnet Test Client v1.8.16 (
   www.bac-test.com/bacnet-test-client-download ) BITS: BIT00032 Any discussions
   can be directed to edward@bac-test.com Please feel free to remove this
   comment when my changes accepted after suitable time for review by all
   interested parties. Say 6 months -> September 2016 */
void Schedule_Out_Of_Service_Set(uint32_t object_instance, bool value)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Out_Of_Service != value) {
            pObject->Out_Of_Service = value;
            pObject->Changed = true;
        }
    }
}

int Schedule_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0;
    struct object_data *pObject;
    uint8_t *apdu = NULL;
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    int i;

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

    switch ((int)rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], Object_Type, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Schedule_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(&apdu[0], Object_Type);
            break;
        case PROP_PRESENT_VALUE:
            apdu_len = bacapp_encode_data(&apdu[0], &pObject->Present_Value);
            break;
        case PROP_EFFECTIVE_PERIOD:
            /* 	BACnet Testing Observed Incident oi00110
                    Effective Period of Schedule object not correctly formatted
                    Revealed by BACnet Test Client v1.8.16 (
               www.bac-test.com/bacnet-test-client-download ) BITS: BIT00031 Any
               discussions can be directed to edward@bac-test.com Please feel
               free to remove this comment when my changes accepted after
               suitable time for
                    review by all interested parties. Say 6 months -> September
               2016 */
            apdu_len =
                encode_application_date(&apdu[0], &pObject->Start_Date);
            apdu_len +=
                encode_application_date(&apdu[apdu_len], &pObject->End_Date);
            break;
        case PROP_WEEKLY_SCHEDULE:
            if (rpdata->array_index == 0) { /* count, always 7 */
                apdu_len = encode_application_unsigned(&apdu[0], 7);
            } else if (rpdata->array_index ==
                BACNET_ARRAY_ALL) { /* full array */
                int day;
                for (day = 0; day < 7; day++) {
                    apdu_len += encode_opening_tag(&apdu[apdu_len], 0);
                    for (i = 0; i < pObject->Weekly_Schedule[day].TV_Count;
                         i++) {
                        apdu_len += bacnet_time_value_encode(&apdu[apdu_len],
                            &pObject->Weekly_Schedule[day].Time_Values[i]);
                    }
                    apdu_len += encode_closing_tag(&apdu[apdu_len], 0);
                }
            } else if (rpdata->array_index <= 7) { /* some array element */
                int day = rpdata->array_index - 1;
                apdu_len += encode_opening_tag(&apdu[apdu_len], 0);
                for (i = 0; i < pObject->Weekly_Schedule[day].TV_Count; i++) {
                    apdu_len += bacnet_time_value_encode(&apdu[apdu_len],
                        &pObject->Weekly_Schedule[day].Time_Values[i]);
                }
                apdu_len += encode_closing_tag(&apdu[apdu_len], 0);
            } else { /* out of bounds */
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                apdu_len = BACNET_STATUS_ERROR;
            }
            break;
        case PROP_SCHEDULE_DEFAULT:
            apdu_len =
                bacapp_encode_data(&apdu[0], &pObject->Schedule_Default);
            break;
        case PROP_LIST_OF_OBJECT_PROPERTY_REFERENCES:
            for (i = 0; i < pObject->obj_prop_ref_cnt; i++) {
                apdu_len += bacapp_encode_device_obj_property_ref(
                    &apdu[apdu_len], &pObject->Object_Property_References[i]);
            }
            break;
        case PROP_PRIORITY_FOR_WRITING:
            apdu_len = encode_application_unsigned(
                &apdu[0], pObject->Priority_For_Writing);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, false);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_RELIABILITY:
            apdu_len = encode_application_enumerated(
                &apdu[0], RELIABILITY_NO_FAULT_DETECTED);
            break;

        case PROP_OUT_OF_SERVICE:
            /* 	BACnet Testing Observed Incident oi00106
                    Out of service was not supported by Schedule object
                    Revealed by BACnet Test Client v1.8.16 (
               www.bac-test.com/bacnet-test-client-download ) BITS: BIT00032 Any
               discussions can be directed to edward@bac-test.com Please feel
               free to remove this comment when my changes accepted after
               suitable time for
                    review by all interested parties. Say 6 months -> September
               2016 */
            apdu_len =
                encode_application_boolean(&apdu[0], pObject->Out_Of_Service);
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }

    if ((apdu_len >= 0) && (rpdata->object_property != PROP_WEEKLY_SCHEDULE) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

bool Schedule_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    /* Ed->Steve, I know that initializing stack values used to be 'safer', but
       warnings in latest compilers indicate when uninitialized values are being
       used, and I think that the warnings are more useful to reveal bad code
       flow than the "safety: of pre-intializing variables. Please give this
       some thought let me know if you agree we should start to remove
       initializations */
    bool status = false; /* return value */
    struct object_data *pObject;
    int len;
    BACNET_APPLICATION_DATA_VALUE value;

    /* 	BACnet Testing Observed Incident oi00106
            Out of service was not supported by Schedule object
            Revealed by BACnet Test Client v1.8.16 (
       www.bac-test.com/bacnet-test-client-download ) BITS: BIT00032 Any
       discussions can be directed to edward@bac-test.com Please feel free to
       remove this comment when my changes accepted after suitable time for
            review by all interested parties. Say 6 months -> September 2016 */
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

    pObject = Keylist_Data(Object_List, wp_data->object_instance);
    if (!pObject)
        return status;

    switch ((int)wp_data->object_property) {
        case PROP_OUT_OF_SERVICE:
            /* 	BACnet Testing Observed Incident oi00106
                    Out of service was not supported by Schedule object
                    Revealed by BACnet Test Client v1.8.16 (
               www.bac-test.com/bacnet-test-client-download ) BITS: BIT00032 Any
               discussions can be directed to edward@bac-test.com Please feel
               free to remove this comment when my changes accepted after
               suitable time for
                    review by all interested parties. Say 6 months -> September
               2016 */
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                Schedule_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;

        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_NAME:
        case PROP_OBJECT_TYPE:
        case PROP_PRESENT_VALUE:
        case PROP_EFFECTIVE_PERIOD:
        case PROP_WEEKLY_SCHEDULE:
        case PROP_SCHEDULE_DEFAULT:
        case PROP_LIST_OF_OBJECT_PROPERTY_REFERENCES:
        case PROP_PRIORITY_FOR_WRITING:
        case PROP_STATUS_FLAGS:
        case PROP_RELIABILITY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }

    return status;
}

static bool Schedule_In_Effective_Period(struct object_data *desc, BACNET_DATE *date)
{
    bool res = false;

    if (desc && date) {
        if (datetime_wildcard_compare_date(&desc->Start_Date, date) <= 0 &&
            datetime_wildcard_compare_date(&desc->End_Date, date) >= 0) {
            res = true;
        }
    }

    return res;
}

/**
 * For a given application value, coerce the encoding, if necessary
 *
 * @param  apdu - buffer to hold the encoding
 * @param  apdu_max - max size of the buffer to hold the encoding
 * @param  value - BACNET_APPLICATION_DATA_VALUE value
 *
 * @return  number of bytes in the APDU, or BACNET_STATUS_ERROR if error.
 */
static int Schedule_Data_Encode(uint8_t *apdu,
    unsigned max_apdu,
    BACNET_APPLICATION_DATA_VALUE *value)
{
    int apdu_len = 0; /* total length of the apdu, return value */
    float float_value = 0.0;
    bool bool_value = false;
    uint32_t enum_value = 1;

    (void)max_apdu;
    if (apdu && value) {
        switch (value->tag) {
            case BACNET_APPLICATION_TAG_BOOLEAN:
                bool_value = value->type.Boolean;
                apdu_len = encode_application_boolean(&apdu[0], bool_value);
                break;
            case BACNET_APPLICATION_TAG_REAL:
                float_value = value->type.Real;
                apdu_len = encode_application_real(&apdu[0], float_value);
                break;
            case BACNET_APPLICATION_TAG_ENUMERATED:
                bool_value = value->type.Enumerated;
                apdu_len = encode_application_enumerated(&apdu[0], enum_value);
                break;
            default:
                apdu_len = BACNET_STATUS_ERROR;
                break;
        }
    }

    return apdu_len;
}


static void Schedule_Recalculate_PV(
    struct object_data *desc, BACNET_DATE_TIME *date)
{
    int i;
    bool active = false;
    bool change = false;
    BACNET_WRITE_PROPERTY_DATA wpdata;
    int apdu_len = 0;
    unsigned m = 0;
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *pMember = NULL;

    /* for future development, here should be the loop for Exception Schedule */

    for (i = 0; i < desc->Weekly_Schedule[date->date.wday - 1].TV_Count; i++) {
        int diff = datetime_wildcard_compare_time(
            &date->time, &desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Time);
        if (diff >= 0) {
            if (desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Value.tag !=
                BACNET_APPLICATION_TAG_NULL) {
                switch (desc->Schedule_Default.tag) {
                case BACNET_APPLICATION_TAG_BOOLEAN:
                    if (desc->Present_Value.type.Boolean != desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Value.type.Boolean) {
                        bacnet_primitive_to_application_data_value(&desc->Present_Value,
                            &desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Value);
                        change = true;
                        desc->Changed = true;
                    }
                    active = true;
                    break;
                case BACNET_APPLICATION_TAG_REAL:
                    if (desc->Present_Value.type.Real < desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Value.type.Real ||
                        desc->Present_Value.type.Real > desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Value.type.Real) {
                        bacnet_primitive_to_application_data_value(&desc->Present_Value,
                            &desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Value);
                        change = true;
                        desc->Changed = true;
                    }
                    active = true;
                    break;
                case BACNET_APPLICATION_TAG_ENUMERATED:
                    if (desc->Present_Value.type.Enumerated != desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Value.type.Enumerated) {
                        bacnet_primitive_to_application_data_value(&desc->Present_Value,
                            &desc->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Value);
                        change = true;
                        desc->Changed = true;
                    }
                    active = true;
                    break;
                default:
                    break;
                }
            } else {
                switch (desc->Schedule_Default.tag) {
                case BACNET_APPLICATION_TAG_BOOLEAN:
                    if (desc->Present_Value.type.Boolean != desc->Schedule_Default.type.Boolean) {
                        memcpy(&desc->Present_Value, &desc->Schedule_Default,
                            sizeof(desc->Present_Value));
                        change = true;
                        desc->Changed = true;
                    }
                    break;
                case BACNET_APPLICATION_TAG_REAL:
                    if (desc->Present_Value.type.Real < desc->Schedule_Default.type.Real ||
                        desc->Present_Value.type.Real > desc->Schedule_Default.type.Real) {
                        memcpy(&desc->Present_Value, &desc->Schedule_Default,
                            sizeof(desc->Present_Value));
                        change = true;
                        desc->Changed = true;
                    }
                    break;
                case BACNET_APPLICATION_TAG_ENUMERATED:
                    if (desc->Present_Value.type.Enumerated != desc->Schedule_Default.type.Enumerated) {
                        memcpy(&desc->Present_Value, &desc->Schedule_Default,
                            sizeof(desc->Present_Value));
                        change = true;
                        desc->Changed = true;
                    }
                    break;
                default:
                    break;
                }
                active = true;
            }
        }
    }

    if (! active ) {
        switch (desc->Schedule_Default.tag) {
        case BACNET_APPLICATION_TAG_BOOLEAN:
            if (desc->Present_Value.type.Boolean != desc->Schedule_Default.type.Boolean) {
                memcpy(&desc->Present_Value, &desc->Schedule_Default,
                    sizeof(desc->Present_Value));
                change = true;
                desc->Changed = true;
            }
            break;
        case BACNET_APPLICATION_TAG_REAL:
            if (desc->Present_Value.type.Real < desc->Schedule_Default.type.Real ||
                desc->Present_Value.type.Real > desc->Schedule_Default.type.Real) {
                memcpy(&desc->Present_Value, &desc->Schedule_Default,
                    sizeof(desc->Present_Value));
                change = true;
                desc->Changed = true;
            }
            break;
        case BACNET_APPLICATION_TAG_ENUMERATED:
            if (desc->Present_Value.type.Enumerated != desc->Schedule_Default.type.Enumerated) {
                memcpy(&desc->Present_Value, &desc->Schedule_Default,
                    sizeof(desc->Present_Value));
                change = true;
                desc->Changed = true;
            }
            break;
        default:
            break;
        }
    }
    if (change) {
        for (m = 0 ; m < desc->obj_prop_ref_cnt; m++) {
            pMember = &desc->Object_Property_References[m];
            wpdata.object_type = pMember->objectIdentifier.type;
            wpdata.object_instance = pMember->objectIdentifier.instance;
            wpdata.object_property = pMember->propertyIdentifier;
            wpdata.array_index = pMember->arrayIndex;
            wpdata.priority = desc->Priority_For_Writing;
            wpdata.application_data_len = sizeof(wpdata.application_data);
            apdu_len = Schedule_Data_Encode(&wpdata.application_data[0],
                &wpdata.application_data_len, &desc->Present_Value);
            if (apdu_len != BACNET_STATUS_ERROR) {
                wpdata.application_data_len = apdu_len;
                Device_Write_Property(&wpdata);
            }
        }
    }
}

void schedule_timer(uint16_t uSeconds)
{
    struct object_data *pObject;
    int iCount = 0;
    bacnet_time_t tNow = 0;
    BACNET_DATE_TIME bdatetime;

    (void)uSeconds;
    /* use OS to get the current time */
    Device_getCurrentDateTime(&bdatetime);
    tNow = datetime_seconds_since_epoch(&bdatetime);
    for (iCount = 0; iCount < Keylist_Count(Object_List);
        iCount++) {
        pObject = Keylist_Data_Index(Object_List, iCount);
        if (Schedule_In_Effective_Period(pObject, &bdatetime.date)) {
            Schedule_Recalculate_PV(pObject, &bdatetime);
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
	int disable,idx,j,k;
    char *uci_list_values[254];
    char *uci_list_name = NULL;
    int uci_list_name_len = 0;

    char *uci_ptr;
	disable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx,"default") == 0)
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
    if (option)
        if (characterstring_init_ansi(&option_str, option))
            pObject->Object_Name = strndup(option,option_str.length);

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "description");
    if (option)
        if (characterstring_init_ansi(&option_str, option))
            pObject->Description = strndup(option,option_str.length);

    if ((pObject->Description == NULL) && (ictx->Object.Description))
        pObject->Description = strdup(ictx->Object.Description);

    pObject->Schedule_Default.context_specific = false;
    pObject->Schedule_Default.tag = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "tag", 0);
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "default");
    if (option){
        switch (pObject->Schedule_Default.tag) {
        case BACNET_APPLICATION_TAG_BOOLEAN:
                pObject->Schedule_Default.type.Boolean = atoi(option);
            break;
        case BACNET_APPLICATION_TAG_REAL:
                pObject->Schedule_Default.type.Real = atof(option);
            break;
        case BACNET_APPLICATION_TAG_ENUMERATED:
                pObject->Schedule_Default.type.Enumerated = atoi(option);
            break;
        default:
            break;
        }
    }

    /* whole year, change as necessary */
    pObject->Start_Date.year = 0;
    pObject->Start_Date.month = 1;
    pObject->Start_Date.day = 1;
    pObject->Start_Date.wday = 1;
    pObject->End_Date.year = 65535;
    pObject->End_Date.month = 12;
    pObject->End_Date.day = 31;
    pObject->End_Date.wday = 7;
    for (j = 0; j < 7; j++) {
        uci_list_name_len = snprintf(NULL, 0, "weekly_%d", j);
        uci_list_name = malloc(uci_list_name_len + 1);
        snprintf(uci_list_name,uci_list_name_len + 1,"weekly_%d",j);
        pObject->Weekly_Schedule[j].TV_Count = ucix_get_list(uci_list_values, ictx->ctx, ictx->section, sec_idx,
            uci_list_name);
        for (k = 0; k < pObject->Weekly_Schedule[j].TV_Count; k++) {
            uci_ptr = strtok(uci_list_values[k], ",");
            pObject->Weekly_Schedule[j].Time_Values[k].Time.hour = atoi(uci_ptr);
            uci_ptr = strtok(NULL, ",");
            pObject->Weekly_Schedule[j].Time_Values[k].Time.min = atoi(uci_ptr);
            uci_ptr = strtok(NULL, ",");
            pObject->Weekly_Schedule[j].Time_Values[k].Time.sec = atoi(uci_ptr);
            pObject->Weekly_Schedule[j].Time_Values[k].Time.hundredths = 0;
            uci_ptr = strtok(NULL, ",");
            if ((strcmp(uci_ptr,"NULL") == 0)) {
                pObject->Weekly_Schedule[j].Time_Values[k].Value.tag = BACNET_APPLICATION_TAG_NULL;
            } else {
                pObject->Weekly_Schedule[j].Time_Values[k].Value.tag = pObject->Schedule_Default.tag;
                switch (pObject->Schedule_Default.tag) {
                case BACNET_APPLICATION_TAG_BOOLEAN:
                        pObject->Weekly_Schedule[j].Time_Values[k].Value.type.Boolean = atoi(uci_ptr);
                    break;
                case BACNET_APPLICATION_TAG_REAL:
                        pObject->Weekly_Schedule[j].Time_Values[k].Value.type.Real = atof(uci_ptr);
                    break;
                case BACNET_APPLICATION_TAG_ENUMERATED:
                        pObject->Weekly_Schedule[j].Time_Values[k].Value.type.Enumerated = atoi(uci_ptr);
                    break;
                default:
                    break;
                }
            }
        }
    }

    pObject->obj_prop_ref_cnt = ucix_get_list(uci_list_values, ictx->ctx, ictx->section, sec_idx,
        "references");
    for (k = 0; k < pObject->obj_prop_ref_cnt; k++) {
        pObject->Object_Property_References[k].deviceIdentifier.instance =
            Device_Object_Instance_Number();
        pObject->Object_Property_References[k].deviceIdentifier.type = OBJECT_DEVICE;
        uci_ptr = strtok(uci_list_values[k], ",");
        pObject->Object_Property_References[k].objectIdentifier.type = atoi(uci_ptr);
        uci_ptr = strtok(NULL, ",");
        pObject->Object_Property_References[k].objectIdentifier.instance = atoi(uci_ptr);
        pObject->Object_Property_References[k].arrayIndex = BACNET_ARRAY_ALL;
        pObject->Object_Property_References[k].propertyIdentifier = PROP_PRESENT_VALUE;
    }
    pObject->Priority_For_Writing = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "prio", ictx->Object.Priority_For_Writing); /* lowest priority */
    pObject->Out_Of_Service = false;
    pObject->Changed = false;
    memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
        sizeof(pObject->Present_Value));

    /* add to list */
    index = Keylist_Data_Add(Object_List, idx, pObject);
    if (index >= 0) {
        Device_Inc_Database_Revision();
    }
    return;
}

void Schedule_Init(void)
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
    if (option)
        if (characterstring_init_ansi(&option_str, option))
            tObject.Description = strndup(option,option_str.length);
    if (!tObject.Description)
        tObject.Description = "Schedule";
    tObject.Priority_For_Writing = ucix_get_option_int(ctx, sec, "default", "prio", 16);
    struct itr_ctx itr_m;
	itr_m.section = sec;
	itr_m.ctx = ctx;
	itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void (*)(const char *, void *))uci_list, &itr_m);
    ucix_cleanup(ctx);
}
