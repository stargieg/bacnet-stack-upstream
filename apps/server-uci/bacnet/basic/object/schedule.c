/**
 * @file
 * @author Nikola Jelic <nikola.jelic@euroicc.com>
 * @date 2015
 * @brief A basic BACnet Schedule object implementation.
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/bacdcode.h"
#include "bacnet/bactext.h"
#include "bacnet/proplist.h"
#include "bacnet/timestamp.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/ucix/ucix.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/object/schedule.h"

static const char *sec = "bacnet_sc";
static const char *type = "sc";

struct object_data_t {
    uint8_t Priority_For_Writing;   /* (1..16) */
    const char *Object_Name;
    const char *Description;
};

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_SCHEDULE;


static const int Schedule_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE,
    PROP_EFFECTIVE_PERIOD,
    PROP_SCHEDULE_DEFAULT,
    PROP_LIST_OF_OBJECT_PROPERTY_REFERENCES,
    PROP_PRIORITY_FOR_WRITING,
    PROP_STATUS_FLAGS,
    PROP_RELIABILITY,
    PROP_OUT_OF_SERVICE,
    -1
};

static const int Schedule_Properties_Optional[] = { PROP_WEEKLY_SCHEDULE,
#if BACNET_EXCEPTION_SCHEDULE_SIZE
                                                    PROP_EXCEPTION_SCHEDULE,
#endif
                                                    -1 };

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

/**
 * @brief Gets an object from the list using an instance number
 * @param  object_instance - object-instance number of the object
 * @return object found in the list, or NULL if not found
 */
static struct object_data_schedule *Schedule_Object(uint32_t object_instance)
{
    return Keylist_Data(Object_List, object_instance);
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
	int disable,idx,j,k,e;
    char *uci_list_values[254];
    uint32_t instance;
    char *uci_list_name = NULL;
    int uci_list_name_len = 0;
    int obj_prop_ref_cnt = 0;

    char *uci_ptr;
    struct object_data_schedule *pObject = NULL;
    int index = 0;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
    BACNET_DATE start_date = { 0 }, end_date = { 0 };
    BACNET_SPECIAL_EVENT *event;
	disable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx, "default") == 0)
		return;
	if (disable)
		return;
    idx = atoi(sec_idx);
    pObject = calloc(1, sizeof(struct object_data_schedule));

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "name");
    if (option && characterstring_init_ansi(&option_str, option))
        pObject->Object_Name = strndup(option,option_str.length);

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "description");
    if (option && characterstring_init_ansi(&option_str, option))
        pObject->Description = strndup(option,option_str.length);
    else
        pObject->Description = strdup(ictx->Object.Description);

    pObject->Schedule_Default.context_specific = false;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "default");
    if (option){
        uci_ptr = strtok(strdup(option), ",");
        pObject->Schedule_Default.tag = atoi(uci_ptr);
        switch (pObject->Schedule_Default.tag) {
        case BACNET_APPLICATION_TAG_BOOLEAN:
            uci_ptr = strtok(NULL, ",");
            pObject->Schedule_Default.type.Boolean = atoi(uci_ptr);
            break;
        case BACNET_APPLICATION_TAG_REAL:
            uci_ptr = strtok(NULL, ",");
            pObject->Schedule_Default.type.Real = atof(uci_ptr);
            break;
        case BACNET_APPLICATION_TAG_ENUMERATED:
            uci_ptr = strtok(NULL, ",");
            pObject->Schedule_Default.type.Enumerated = atoi(uci_ptr);
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
        snprintf(uci_list_name, uci_list_name_len + 1, "weekly_%d", j);
        pObject->Weekly_Schedule[j].TV_Count = ucix_get_list(uci_list_values, ictx->ctx, ictx->section, sec_idx,
            uci_list_name);
        for (k = 0; k < pObject->Weekly_Schedule[j].TV_Count; k++) {
            if (*uci_list_values[k] == '\0') break;
            uci_ptr = strtok(uci_list_values[k], ",");
            pObject->Weekly_Schedule[j].Time_Values[k].Time.hour = atoi(uci_ptr);
            uci_ptr = strtok(NULL, ",");
            pObject->Weekly_Schedule[j].Time_Values[k].Time.min = atoi(uci_ptr);
            uci_ptr = strtok(NULL, ",");
            pObject->Weekly_Schedule[j].Time_Values[k].Time.sec = atoi(uci_ptr);
            pObject->Weekly_Schedule[j].Time_Values[k].Time.hundredths = 0;
            uci_ptr = strtok(NULL, ",");
            pObject->Weekly_Schedule[j].Time_Values[k].Value.tag = atoi(uci_ptr);
            switch (pObject->Weekly_Schedule[j].Time_Values[k].Value.tag) {
            case BACNET_APPLICATION_TAG_BOOLEAN:
                uci_ptr = strtok(NULL, ",");
                pObject->Weekly_Schedule[j].Time_Values[k].Value.type.Boolean = atoi(uci_ptr);
                break;
            case BACNET_APPLICATION_TAG_REAL:
                uci_ptr = strtok(NULL, ",");
                pObject->Weekly_Schedule[j].Time_Values[k].Value.type.Real = atof(uci_ptr);
                break;
            case BACNET_APPLICATION_TAG_ENUMERATED:
                uci_ptr = strtok(NULL, ",");
                pObject->Weekly_Schedule[j].Time_Values[k].Value.type.Enumerated = atoi(uci_ptr);
                break;
            default:
                break;
            }
        }
    }

    j = ucix_get_list(uci_list_values, ictx->ctx, ictx->section, sec_idx,
        "references");
    for (k = 0; k < j; k++) {
        if (*uci_list_values[obj_prop_ref_cnt] == '\0') break;
        if (k >= BACNET_SCHEDULE_OBJ_PROP_REF_SIZE ) break;
        uci_ptr = strtok(uci_list_values[obj_prop_ref_cnt], ",");
        instance = atoi(uci_ptr);
        if (instance >= BACNET_MAX_INSTANCE) {
            pObject->Object_Property_References[obj_prop_ref_cnt].deviceIdentifier.type = 65535;
        } else {
            pObject->Object_Property_References[obj_prop_ref_cnt].deviceIdentifier.instance =
                instance;
            pObject->Object_Property_References[obj_prop_ref_cnt].deviceIdentifier.type = OBJECT_DEVICE;
        }
        uci_ptr = strtok(NULL, ",");
        pObject->Object_Property_References[obj_prop_ref_cnt].objectIdentifier.type = atoi(uci_ptr);
        uci_ptr = strtok(NULL, ",");
        pObject->Object_Property_References[obj_prop_ref_cnt].objectIdentifier.instance = atoi(uci_ptr);
        pObject->Object_Property_References[obj_prop_ref_cnt].arrayIndex = BACNET_ARRAY_ALL;
        pObject->Object_Property_References[obj_prop_ref_cnt].propertyIdentifier = PROP_PRESENT_VALUE;
        obj_prop_ref_cnt++;
    }
    pObject->obj_prop_ref_cnt = obj_prop_ref_cnt;
    pObject->Priority_For_Writing = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "prio", ictx->Object.Priority_For_Writing); /* lowest priority */
    pObject->Out_Of_Service = false;
    pObject->Changed = false;
#if BACNET_EXCEPTION_SCHEDULE_SIZE
    for (e = 0; e < BACNET_EXCEPTION_SCHEDULE_SIZE; e++) {
        event = &pObject->Exception_Schedule[e];
        event->periodTag = BACNET_SPECIAL_EVENT_PERIOD_CALENDAR_ENTRY;
        event->period.calendarEntry.tag = BACNET_CALENDAR_DATE_RANGE;
        datetime_copy_date(
            &event->period.calendarEntry.type.DateRange.startdate,
            &start_date);
        datetime_copy_date(
            &event->period.calendarEntry.type.DateRange.enddate, &end_date);
        event->period.calendarEntry.next = NULL;
        event->timeValues.TV_Count = 0;
        event->priority = 16;
    }
#endif

    memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
        sizeof(pObject->Present_Value));

    /* add to list */
    index = Keylist_Data_Add(Object_List, idx, pObject);
    if (index >= 0) {
        Device_Inc_Database_Revision();
    }
    return;
}

/**
 * @brief Initialize the Schedule object data
 */
void Schedule_Init(void)
{
    struct uci_context *ctx;
    struct object_data_t tObject;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
    struct itr_ctx itr_m;

    if (!Object_List) {
        Object_List = Keylist_Create();
    }
    ctx = ucix_init(sec);
    if (!ctx)
        fprintf(stderr, "Failed to load config file %s\n", sec);

    option = ucix_get_option(ctx, sec, "default", "description");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Description = strndup(option, option_str.length);
    else
        tObject.Description = "Schedule";
    tObject.Priority_For_Writing = ucix_get_option_int(ctx, sec, "default", "prio", 16);
	itr_m.section = sec;
	itr_m.ctx = ctx;
	itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void (*)(const char *, void *))uci_list, &itr_m);
    ucix_cleanup(ctx);
}

/**
 * @brief Determines if a given instance is valid
 * @param  object_instance - object-instance number of the object
 * @return true if the instance is valid, and false if not
 */
bool Schedule_Valid_Instance(uint32_t object_instance)
{
    struct object_data_schedule *pObject;
    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return true;
    } else {
        return false;
    }
}

/**
 * @brief Determines the number of Schedule objects
 * @return Number of Schedule objects
 */
unsigned Schedule_Count(void)
{
    return Keylist_Count(Object_List);
}

/**
 * @brief Determines the object instance number for a given index
 * @param  index - index number of the object
 * @return object instance number for the given index, or MAX_SCHEDULES if the
 * index is not valid
 */
uint32_t Schedule_Index_To_Instance(unsigned index)
{
    uint32_t instance = UINT32_MAX;

    (void)Keylist_Index_Key(Object_List, index, &instance);

    return instance;
}

/**
 * @brief Determines the index for a given object instance number
 * @param  instance - object-instance number of the object
 * @return index number for the given object instance number, or MAX_SCHEDULES
 * if the instance is not valid
 */
unsigned Schedule_Instance_To_Index(uint32_t instance)
{
    return Keylist_Index(Object_List, instance);
}

/**
 * @brief Determines the object name for a given object instance number
 * @param  object_instance - object-instance number of the object
 * @param  object_name - object name of the object
 * @return true if the object name is valid, and false if not
 */
bool Schedule_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    char name_text[32] = "";
    bool status = false;
    struct object_data_schedule *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Object_Name) {
            status = characterstring_init_ansi(object_name,
                pObject->Object_Name);
        } else {
            snprintf(name_text, sizeof(name_text), "SC %u",
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
bool Schedule_Name_Set(uint32_t object_instance, char *new_name)
{
    bool status = false; /* return value */
    BACNET_CHARACTER_STRING object_name;
    BACNET_OBJECT_TYPE found_type = 0;
    uint32_t found_instance = 0;
    struct object_data_schedule *pObject;

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
 * For a given object instance-number, returns the out-of-service
 * status flag
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  out-of-service status flag
 */
bool Schedule_Out_Of_Service(uint32_t object_instance)
{
    bool value = false;
    struct object_data_schedule *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Out_Of_Service;
    }

    return value;
}

/**
 * @brief Sets a specificSchedule object out of service
 * @param object_instance - object-instance number of the object
 * @param value - true if out of service, and false if not
 */
void Schedule_Out_Of_Service_Set(uint32_t object_instance, bool value)
{
    struct object_data_schedule *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Out_Of_Service != value) {
            pObject->Out_Of_Service = value;
            pObject->Changed = true;
        }
    }
}

/**
 * @brief For a given object instance-number, returns the description
 * @param  object_instance - object-instance number of the object
 * @return description text or NULL if not found
 */
char *Schedule_Description(uint32_t object_instance)
{
    char *name = NULL;
    struct object_data_schedule *pObject;

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
bool Schedule_Description_Set(uint32_t object_instance, char *new_name)
{
    bool status = false; /* return value */
    struct object_data_schedule *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && new_name) {
        status = true;
        pObject->Description = new_name;
    }

    return status;
}


#if 1
/**
 * @brief Encode a BACnetARRAY property element
 * @param object_instance [in] BACnet network port object instance number
 * @param array_index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Schedule_Weekly_Schedule_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX array_index, uint8_t *apdu)
{
    int apdu_len = 0, len = 0;
    struct object_data_schedule *pObject;
    int day, i;

    if (array_index >= 7) {
        return BACNET_STATUS_ERROR;
    }
    pObject = Schedule_Object(object_instance);
    if (!pObject) {
        return BACNET_STATUS_ERROR;
    }
    day = array_index;
    len = encode_opening_tag(apdu, 0);
    apdu_len += len;
    if (apdu) {
        apdu += len;
    }
    for (i = 0; i < pObject->Weekly_Schedule[day].TV_Count; i++) {
        len = bacnet_time_value_encode(
            apdu, &pObject->Weekly_Schedule[day].Time_Values[i]);
        apdu_len += len;
        if (apdu) {
            apdu += len;
        }
    }
    len = encode_closing_tag(apdu, 0);
    apdu_len += len;

    return apdu_len;
}
#endif

#if BACNET_EXCEPTION_SCHEDULE_SIZE
/**
 * @brief Encode a BACnetARRAY property element
 * @param object_instance [in] BACnet network port object instance number
 * @param array_index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Schedule_Exception_Schedule_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX array_index, uint8_t *apdu)
{
    int apdu_len;
    struct object_data_schedule *pObject;

    if (array_index >= BACNET_EXCEPTION_SCHEDULE_SIZE) {
        return BACNET_STATUS_ERROR;
    }
    pObject = Schedule_Object(object_instance);
    if (!pObject) {
        return BACNET_STATUS_ERROR;
    }
    apdu_len = bacnet_special_event_encode(
        apdu, &pObject->Exception_Schedule[array_index]);

    return apdu_len;
}
#endif

int Schedule_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0;
    struct object_data_schedule *pObject;
    uint8_t *apdu = NULL;
    uint16_t apdu_max = 0;
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    int i;
    bool state = false;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    pObject = Keylist_Data(Object_List, rpdata->object_instance);
    if (!pObject) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        apdu_len = BACNET_STATUS_ERROR;
        return apdu_len;
    }
    apdu = rpdata->application_data;
    apdu_max = rpdata->application_data_len;
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
            apdu_len =
                encode_application_date(&apdu[0], &pObject->Start_Date);
            apdu_len +=
                encode_application_date(&apdu[apdu_len], &pObject->End_Date);
            break;
        case PROP_WEEKLY_SCHEDULE:
            apdu_len = bacnet_array_encode(
                rpdata->object_instance, rpdata->array_index,
                Schedule_Weekly_Schedule_Encode, 7, apdu, apdu_max);
            if (apdu_len == BACNET_STATUS_ABORT) {
                rpdata->error_code =
                    ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
            } else if (apdu_len == BACNET_STATUS_ERROR) {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
            }
            break;
#if BACNET_EXCEPTION_SCHEDULE_SIZE
        case PROP_EXCEPTION_SCHEDULE:
            apdu_len = bacnet_array_encode(
                rpdata->object_instance, rpdata->array_index,
                Schedule_Exception_Schedule_Encode,
                BACNET_EXCEPTION_SCHEDULE_SIZE, apdu, apdu_max);
            if (apdu_len == BACNET_STATUS_ABORT) {
                rpdata->error_code =
                    ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
            } else if (apdu_len == BACNET_STATUS_ERROR) {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
            }
            break;
#endif
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
            state = Schedule_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Schedule_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
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
    bool status = false; /* return value */
    int iOffset;
    uint8_t idx;
    struct object_data_schedule *pObject;
    int len;
    BACNET_APPLICATION_DATA_VALUE value = { 0 };
    bool is_array;
    BACNET_TIME_VALUE time_value;
    BACNET_UNSIGNED_INTEGER unsigned_value = 0;
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE obj_prop_ref_value;
    uint32_t instance = 0;
    int k;
    struct uci_context *ctxw = NULL;
    char *idx_c = NULL;
    int idx_c_len = 0;
    char *uci_list_name = NULL;
    int uci_list_name_len = 0;
    char uci_list_values[254][64];
    int TV_Count = 0;
    char *value_c = NULL;
    int value_c_len = 0;


    /*  only array properties can have array options */
    is_array = property_list_bacnet_array_member(
        wp_data->object_type, wp_data->object_property);
    if (!is_array && (wp_data->array_index != BACNET_ARRAY_ALL)) {
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        return false;
    }
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

    ctxw = ucix_init(sec);
    if (!ctxw)
        fprintf(stderr, "Failed to load config file %s\n",sec);
    idx_c_len = snprintf(NULL, 0, "%d", wp_data->object_instance);
    idx_c = malloc(idx_c_len + 1);
    snprintf(idx_c,idx_c_len + 1,"%d",wp_data->object_instance);
    switch ((int)wp_data->object_property) {
        case PROP_OUT_OF_SERVICE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                Schedule_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_OBJECT_NAME:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Schedule_Name_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "name",
                        strndup(value.type.Character_String.value, value.type.Character_String.length));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_OBJECT_TYPE:
        case PROP_PRESENT_VALUE:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_EFFECTIVE_PERIOD:
            iOffset = 0;
            /* Decode From Date */
            len = bacapp_decode_application_data(
                &wp_data->application_data[iOffset],
                wp_data->application_data_len, &value);

            if ((len == 0) || (value.tag != BACNET_APPLICATION_TAG_DATE)) {
                /* Bad decode, wrong tag or following required parameter
                    * missing */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                return false;
            }
            /* store value object */
            pObject->Start_Date = value.type.Date;

            iOffset += len;
            /* Decode To Time */
            len = bacapp_decode_application_data(
                &wp_data->application_data[iOffset],
                wp_data->application_data_len, &value);

            if ((len == 0) || (value.tag != BACNET_APPLICATION_TAG_DATE)) {
                /* Bad decode, wrong tag or following required parameter
                    * missing */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                return false;
            }
            /* store value object */
            pObject->End_Date = value.type.Date;
            status = true;
            break;
        case PROP_WEEKLY_SCHEDULE:
            idx = 0;
            iOffset = 0;
            /* decode MO - SA */
            for (idx = 0 ; idx < 7 ; idx++) {
                if (bacnet_is_opening_tag_number(
                    wp_data->application_data,iOffset, 0, NULL)) {
                    iOffset++;
                    TV_Count = 0;
                    /* Decode Valid Days */
                    for (k = 0 ; k < BACNET_WEEKLY_SCHEDULE_SIZE;k++) {
                        len = bacnet_time_value_decode(
                            &wp_data->application_data[iOffset],
                            wp_data->application_data_len,
                            &time_value);
                        if (len > 0) {
                            iOffset += len;
                            /* store value object */
                            switch (time_value.Value.tag) {
                            case BACNET_APPLICATION_TAG_BOOLEAN:
                                pObject->Weekly_Schedule[idx].Time_Values[TV_Count] = time_value;
                                value_c_len = snprintf(NULL, 0, "%i,%i,%i,%i,%i",
                                time_value.Time.hour, time_value.Time.min, time_value.Time.sec,
                                BACNET_APPLICATION_TAG_BOOLEAN, time_value.Value.type.Boolean);
                                snprintf(uci_list_values[TV_Count],value_c_len + 1, "%i,%i,%i,%i,%i",
                                time_value.Time.hour, time_value.Time.min, time_value.Time.sec,
                                BACNET_APPLICATION_TAG_BOOLEAN, time_value.Value.type.Boolean);
                                TV_Count++;
                                break;
                            case BACNET_APPLICATION_TAG_REAL:
                                pObject->Weekly_Schedule[idx].Time_Values[TV_Count] = time_value;
                                value_c_len = snprintf(NULL, 0, "%i,%i,%i,%i,%f",
                                time_value.Time.hour, time_value.Time.min, time_value.Time.sec,
                                BACNET_APPLICATION_TAG_REAL, time_value.Value.type.Real);
                                snprintf(uci_list_values[TV_Count],value_c_len + 1, "%i,%i,%i,%i,%f",
                                time_value.Time.hour, time_value.Time.min, time_value.Time.sec,
                                BACNET_APPLICATION_TAG_REAL, time_value.Value.type.Real);
                                TV_Count++;
                                break;
                            case BACNET_APPLICATION_TAG_ENUMERATED:
                                pObject->Weekly_Schedule[idx].Time_Values[TV_Count] = time_value;
                                value_c_len = snprintf(NULL, 0, "%i,%i,%i,%i,%i",
                                time_value.Time.hour, time_value.Time.min, time_value.Time.sec,
                                BACNET_APPLICATION_TAG_ENUMERATED, time_value.Value.type.Enumerated);
                                snprintf(uci_list_values[TV_Count],value_c_len + 1, "%i,%i,%i,%i,%i",
                                time_value.Time.hour, time_value.Time.min, time_value.Time.sec,
                                BACNET_APPLICATION_TAG_ENUMERATED, time_value.Value.type.Enumerated);
                                TV_Count++;
                                break;
                            case BACNET_APPLICATION_TAG_NULL:
                                pObject->Weekly_Schedule[idx].Time_Values[TV_Count] = time_value;
                                value_c_len = snprintf(NULL, 0, "%i,%i,%i,%i,0",
                                time_value.Time.hour, time_value.Time.min, time_value.Time.sec,
                                BACNET_APPLICATION_TAG_NULL);
                                snprintf(uci_list_values[TV_Count],value_c_len + 1, "%i,%i,%i,%i,0",
                                time_value.Time.hour, time_value.Time.min, time_value.Time.sec,
                                BACNET_APPLICATION_TAG_NULL);
                                TV_Count++;
                                break;
                            default:
                                break;
                            }
                        }
                        if (bacnet_is_closing_tag_number(
                                wp_data->application_data,iOffset, 0, NULL)) {
                            iOffset++;
                            /* store time value count */
                            pObject->Weekly_Schedule[idx].TV_Count = TV_Count;
                            uci_list_name_len = snprintf(NULL, 0, "weekly_%d", idx);
                            uci_list_name = malloc(uci_list_name_len + 1);
                            snprintf(uci_list_name,uci_list_name_len + 1, "weekly_%d", idx);
                            if (TV_Count > 0) {
                                ucix_set_list(ctxw, sec, idx_c, uci_list_name,
                                uci_list_values, TV_Count);
                                status = true;
                            } else {
                                ucix_del(ctxw, sec, idx_c, uci_list_name);
                                status = true;
                            }
                            free(uci_list_name);
                        }
                    }
                } else {
                    /* store time value count */
                    pObject->Weekly_Schedule[idx].TV_Count = 0;
                    uci_list_name_len = snprintf(NULL, 0, "weekly_%d", idx);
                    uci_list_name = malloc(uci_list_name_len + 1);
                    snprintf(uci_list_name,uci_list_name_len + 1, "weekly_%d", idx);
                    ucix_del(ctxw, sec, idx_c, uci_list_name);
                    free(uci_list_name);
                    status = true;
                }
            }
            if (status) {
                ucix_commit(ctxw, sec);
            }
            break;
        case PROP_SCHEDULE_DEFAULT:
            iOffset = 0;
            len = bacapp_decode_application_data(
                &wp_data->application_data[iOffset],
                wp_data->application_data_len, &value);

            if (len == 0) {
                /* Bad decode, wrong tag or following required parameter
                    * missing */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                return false;
            }
            /* store value object */
            pObject->Schedule_Default = value;
            /* store value uci */
            switch (value.tag) {
            case BACNET_APPLICATION_TAG_BOOLEAN:
                value_c_len = snprintf(NULL, 0, "%i,%i", BACNET_APPLICATION_TAG_BOOLEAN, value.type.Boolean);
                value_c = malloc(value_c_len + 1);
                snprintf(value_c,value_c_len + 1,"%i,%i", BACNET_APPLICATION_TAG_BOOLEAN, value.type.Boolean);
                status = true;
                break;
            case BACNET_APPLICATION_TAG_REAL:
                value_c_len = snprintf(NULL, 0, "%i,%f", BACNET_APPLICATION_TAG_REAL, value.type.Real);
                value_c = malloc(value_c_len + 1);
                snprintf(value_c,value_c_len + 1,"%i,%f", BACNET_APPLICATION_TAG_REAL, value.type.Real);
                status = true;
                break;
            case BACNET_APPLICATION_TAG_ENUMERATED:
                value_c_len = snprintf(NULL, 0, "%i,%i", BACNET_APPLICATION_TAG_ENUMERATED, value.type.Enumerated);
                value_c = malloc(value_c_len + 1);
                snprintf(value_c,value_c_len + 1,"%i,%i", BACNET_APPLICATION_TAG_ENUMERATED, value.type.Enumerated);
                status = true;
                break;
            case BACNET_APPLICATION_TAG_NULL:
                value_c_len = snprintf(NULL, 0, "%i,0", BACNET_APPLICATION_TAG_NULL);
                value_c = malloc(value_c_len + 1);
                snprintf(value_c,value_c_len + 1, "%i,0", BACNET_APPLICATION_TAG_NULL);
                status = true;
                break;
            default:
                break;
            }
            if (status) {
                ucix_add_option(ctxw, sec, idx_c, "default", value_c);
                ucix_commit(ctxw, sec);
                free(value_c);
            }
            break;
        case PROP_LIST_OF_OBJECT_PROPERTY_REFERENCES:
            idx = 0;
            iOffset = 0;
            TV_Count = 0;
            for (idx = 0 ; idx < BACNET_SCHEDULE_OBJ_PROP_REF_SIZE ; idx++) {
                len = bacnet_device_object_property_reference_decode(
                    wp_data->application_data,iOffset,
                    &obj_prop_ref_value);
                if (len <= 0) {
                    idx = BACNET_SCHEDULE_OBJ_PROP_REF_SIZE;
                } else {
                    /* store value object */
                    pObject->Object_Property_References[TV_Count] = obj_prop_ref_value;
                    iOffset += len;
                    if (obj_prop_ref_value.deviceIdentifier.type == 65535) {
                        instance = -1;
                    } else if (obj_prop_ref_value.deviceIdentifier.type == OBJECT_DEVICE) {
                        instance = obj_prop_ref_value.deviceIdentifier.instance;
                    }
                    value_c_len = snprintf(NULL, 0, "%i,%i,%i",
                    instance,
                    obj_prop_ref_value.objectIdentifier.type,
                    obj_prop_ref_value.objectIdentifier.instance);
                    snprintf(uci_list_values[TV_Count],value_c_len + 1, "%i,%i,%i",
                    instance,
                    obj_prop_ref_value.objectIdentifier.type,
                    obj_prop_ref_value.objectIdentifier.instance);
                    status = true;
                    TV_Count++;
                }
            }
            pObject->obj_prop_ref_cnt = TV_Count; 
            if (TV_Count > 0) {
                ucix_set_list(
                    ctxw,
                    sec,
                    idx_c,
                    "references",
                    uci_list_values,
                    TV_Count);
            } else {
                ucix_del(ctxw, sec, idx_c, "references");
            }
            ucix_commit(ctxw, sec);
            status = true;
            break;
        case PROP_PRIORITY_FOR_WRITING:
            iOffset = 0;
            len = bacnet_unsigned_decode(
                wp_data->application_data,iOffset,
                wp_data->application_data_len, &unsigned_value);

            if (len == 0) {
                /* Bad decode, wrong tag or following required parameter
                    * missing */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                return false;
            }
            /* store value object */
            pObject->Priority_For_Writing = unsigned_value;
            ucix_add_option_int(ctxw, sec, idx_c, "prio", pObject->Priority_For_Writing);
            ucix_commit(ctxw, sec);
            status = true;
            break;
        case PROP_EXCEPTION_SCHEDULE:
        case PROP_STATUS_FLAGS:
        case PROP_RELIABILITY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_DESCRIPTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Schedule_Description_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "description",
                        Schedule_Description(wp_data->object_instance));
                    ucix_commit(ctxw, sec);
                }
            }
            break;
        default:
            if (property_lists_member(
                    Schedule_Properties_Required, Schedule_Properties_Optional,
                    Schedule_Properties_Proprietary,
                    wp_data->object_property)) {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            }
            break;
    }
    if (ctxw)
        ucix_cleanup(ctxw);
    free(idx_c);

    return status;
}

/**
 * @brief Determine if the given calendar entry is within the effective period
 * @param desc - schedule descriptor
 * @param date - date to check
 * @return true if the calendar entry is within the effective period
 */
bool Schedule_In_Effective_Period(
    const struct object_data_schedule *pObject, const BACNET_DATE *date)
{
    bool res = false;

    if (pObject && date) {
        if (datetime_wildcard_compare_date(&pObject->Start_Date, date) <= 0 &&
            datetime_wildcard_compare_date(&pObject->End_Date, date) >= 0) {
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


/**
 * @brief Recalculate the Present Value of the Schedule object
 * @param pObject - schedule descriptor
 * @param wday - day of the week
 * @param time - time of the day
 */
void Schedule_Recalculate_PV(
    struct object_data_schedule *pObject, BACNET_DATE_TIME *date)
{
    int i,j;
    bool active = false;
    bool change = false;
    BACNET_WRITE_PROPERTY_DATA wpdata;
    int apdu_len = 0;
    unsigned m = 0;
    BACNET_DEVICE_OBJECT_PROPERTY_REFERENCE *pMember = NULL;
    int diff = 0;
    bool found = false;
    /* needed to filter incoming messages */
    static uint8_t Request_Invoke_ID = 0;
    static BACNET_ADDRESS Target_Address;
    unsigned max_apdu = 0;

    /* for future development, here should be the loop for Exception Schedule */

    /*  Note to developers: please ping Edward at info@connect-ex.com
        for a more complete schedule object implementation. */
    j = -1;
    for (i = 0; i < pObject->Weekly_Schedule[date->date.wday - 1].TV_Count; i++) {
        diff = datetime_wildcard_compare_time(
            &date->time, &pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Time);
        if (diff >= 0) {
            if (j >= 0) {
                diff = datetime_wildcard_compare_time(
                    &pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[i].Time,
                    &pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[j].Time);
                if (diff >= 0) j = i;
            } else {
                j = i;
            }
        }
    }

    if (j >= 0) {
        if (pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[j].Value.tag !=
            BACNET_APPLICATION_TAG_NULL) {
            switch (pObject->Schedule_Default.tag) {
            case BACNET_APPLICATION_TAG_BOOLEAN:
                if (pObject->Present_Value.type.Boolean != pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[j].Value.type.Boolean) {
                    change = true;
                    pObject->Changed = true;
                }
                active = true;
                break;
            case BACNET_APPLICATION_TAG_REAL:
                if (pObject->Present_Value.type.Real < pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[j].Value.type.Real ||
                    pObject->Present_Value.type.Real > pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[j].Value.type.Real) {
                    change = true;
                    pObject->Changed = true;
                }
                active = true;
                break;
            case BACNET_APPLICATION_TAG_ENUMERATED:
                if (pObject->Present_Value.type.Enumerated != pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[j].Value.type.Enumerated) {
                    change = true;
                    pObject->Changed = true;
                }
                active = true;
                break;
            default:
                break;
            }
            if (change) {
                bacnet_primitive_to_application_data_value(&pObject->Present_Value,
                    &pObject->Weekly_Schedule[date->date.wday - 1].Time_Values[j].Value);
            }
        } else {
            switch (pObject->Schedule_Default.tag) {
            case BACNET_APPLICATION_TAG_BOOLEAN:
                if (pObject->Present_Value.type.Boolean != pObject->Schedule_Default.type.Boolean) {
                    memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
                        sizeof(pObject->Present_Value));
                    change = true;
                    pObject->Changed = true;
                }
                break;
            case BACNET_APPLICATION_TAG_REAL:
                if (pObject->Present_Value.type.Real < pObject->Schedule_Default.type.Real ||
                    pObject->Present_Value.type.Real > pObject->Schedule_Default.type.Real) {
                    memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
                        sizeof(pObject->Present_Value));
                    change = true;
                    pObject->Changed = true;
                }
                break;
            case BACNET_APPLICATION_TAG_ENUMERATED:
                if (pObject->Present_Value.type.Enumerated != pObject->Schedule_Default.type.Enumerated) {
                    memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
                        sizeof(pObject->Present_Value));
                    change = true;
                    pObject->Changed = true;
                }
                break;
            default:
                break;
            }
            active = true;
        }
    }
    if (! active ) {
        switch (pObject->Schedule_Default.tag) {
        case BACNET_APPLICATION_TAG_BOOLEAN:
            if (pObject->Present_Value.type.Boolean != pObject->Schedule_Default.type.Boolean) {
                memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
                    sizeof(pObject->Present_Value));
                change = true;
                pObject->Changed = true;
            }
            break;
        case BACNET_APPLICATION_TAG_REAL:
            if (pObject->Present_Value.type.Real < pObject->Schedule_Default.type.Real ||
                pObject->Present_Value.type.Real > pObject->Schedule_Default.type.Real) {
                memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
                    sizeof(pObject->Present_Value));
                change = true;
                pObject->Changed = true;
            }
            break;
        case BACNET_APPLICATION_TAG_ENUMERATED:
            if (pObject->Present_Value.type.Enumerated != pObject->Schedule_Default.type.Enumerated) {
                memcpy(&pObject->Present_Value, &pObject->Schedule_Default,
                    sizeof(pObject->Present_Value));
                change = true;
                pObject->Changed = true;
            }
            break;
        default:
            break;
        }
    }
    if (change) {
        for (m = 0 ; m < pObject->obj_prop_ref_cnt; m++) {
            pMember = &pObject->Object_Property_References[m];
            if (pMember->deviceIdentifier.type == 65535) {
                wpdata.object_type = pMember->objectIdentifier.type;
                wpdata.object_instance = pMember->objectIdentifier.instance;
                wpdata.object_property = pMember->propertyIdentifier;
                wpdata.array_index = pMember->arrayIndex;
                wpdata.priority = pObject->Priority_For_Writing;
                wpdata.application_data_len = sizeof(wpdata.application_data);
                apdu_len = Schedule_Data_Encode(&wpdata.application_data[0],
                    wpdata.application_data_len, &pObject->Present_Value);
                if (apdu_len != BACNET_STATUS_ERROR) {
                    wpdata.application_data_len = apdu_len;
                    Device_Write_Property(&wpdata);
                }
            } else if (pMember->deviceIdentifier.type == OBJECT_DEVICE) {
                /* try to bind with the device */
                found = address_bind_request(
                    pMember->deviceIdentifier.instance, &max_apdu, &Target_Address);
                if (!found) {
                    Send_WhoIs(
                        pMember->deviceIdentifier.instance, pMember->deviceIdentifier.instance);
                    found = address_bind_request(
                        pMember->deviceIdentifier.instance, &max_apdu, &Target_Address);
                }
                if (found) {
                    if (Request_Invoke_ID == 0) {
                        Request_Invoke_ID = Send_Write_Property_Request(
                            pMember->deviceIdentifier.instance,
                            pMember->objectIdentifier.type,
                            pMember->objectIdentifier.instance,
                            pMember->propertyIdentifier,
                            &pObject->Present_Value,
                            pObject->Priority_For_Writing,
                            pMember->arrayIndex);
                    }
                }
            }
        }
    }
}

void schedule_timer(uint16_t uSeconds)
{
    struct object_data_schedule *pObject;
    int iCount = 0;
    BACNET_DATE_TIME bdatetime;

    (void)uSeconds;
    /* use OS to get the current time */
    Device_getCurrentDateTime(&bdatetime);
    for (iCount = 0; iCount < Keylist_Count(Object_List);
        iCount++) {
        pObject = Keylist_Data_Index(Object_List, iCount);
        if (Schedule_In_Effective_Period(pObject, &bdatetime.date)) {
            Schedule_Recalculate_PV(pObject, &bdatetime);
        }
    }
}
