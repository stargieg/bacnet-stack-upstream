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

bool Schedule_In_Effective_Period(SCHEDULE_DESCR *desc, BACNET_DATE *date)
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

void Schedule_Recalculate_PV(
    SCHEDULE_DESCR *desc, BACNET_WEEKDAY wday, BACNET_TIME *time)
{
    int i;
    desc->Present_Value.tag = BACNET_APPLICATION_TAG_NULL;

    /* for future development, here should be the loop for Exception Schedule */

    /* Just a note to developers: We have a paying customer who has asked us to
       fully implement the Schedule Object. In good spirit, they have agreed to
       allow us to release the code we develop back to the Open Source community
       after a 6-12 month waiting period. However, if you are about to work on
       this yourself, please ping us at info@connect-ex.com, we may be able to
       broker an early release on a case-by-case basis. */

    for (i = 0; i < desc->Weekly_Schedule[wday - 1].TV_Count &&
         desc->Present_Value.tag == BACNET_APPLICATION_TAG_NULL;
         i++) {
        int diff = datetime_wildcard_compare_time(
            time, &desc->Weekly_Schedule[wday - 1].Time_Values[i].Time);
        if (diff >= 0 &&
            desc->Weekly_Schedule[wday - 1].Time_Values[i].Value.tag !=
                BACNET_APPLICATION_TAG_NULL) {
            bacnet_primitive_to_application_data_value(&desc->Present_Value,
                &desc->Weekly_Schedule[wday - 1].Time_Values[i].Value);
        }
    }

    if (desc->Present_Value.tag == BACNET_APPLICATION_TAG_NULL) {
        memcpy(&desc->Present_Value, &desc->Schedule_Default,
            sizeof(desc->Present_Value));
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
	int disable,idx,j;
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

    /* whole year, change as necessary */
    pObject->Start_Date.year = 0xFF;
    pObject->Start_Date.month = 1;
    pObject->Start_Date.day = 1;
    pObject->Start_Date.wday = 0xFF;
    pObject->End_Date.year = 0xFF;
    pObject->End_Date.month = 12;
    pObject->End_Date.day = 31;
    pObject->End_Date.wday = 0xFF;
    for (j = 0; j < 7; j++) {
        pObject->Weekly_Schedule[j].TV_Count = 0;
    }
    memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
        sizeof(pObject->Present_Value));
    pObject->Schedule_Default.context_specific = false;
    pObject->Schedule_Default.tag = BACNET_APPLICATION_TAG_REAL;
    pObject->Schedule_Default.type.Real = 21.0f; /* 21 C, room temperature */
    pObject->obj_prop_ref_cnt = 0; /* no references, add as needed */
    pObject->Priority_For_Writing = 16; /* lowest priority */
    pObject->Out_Of_Service = false;
    pObject->Changed = false;

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
        tObject.Description = "Trendlog";
    tObject.Priority_For_Writing = ucix_get_option_int(ctx, sec, "default", "prio", 16);
    struct itr_ctx itr_m;
	itr_m.section = sec;
	itr_m.ctx = ctx;
	itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void (*)(const char *, void *))uci_list, &itr_m);
    ucix_cleanup(ctx);
}
