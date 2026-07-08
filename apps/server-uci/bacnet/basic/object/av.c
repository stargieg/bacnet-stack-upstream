/**
 * @file
 * @brief A basic BACnet Analog Input Object implementation.
 * An analog value object is an I/O object with a present-value that
 * uses an single precision floating point data type.
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @author Krzysztof Malorny <malornykrzysztof@gmail.com>
 * @date 2006, 2011
 * @copyright SPDX-License-Identifier: MIT
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/bacapp.h"
#include "bacnet/bacdcode.h"
#include "bacnet/datetime.h"
#include "bacnet/proplist.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/sys/debug.h"
#include "bacnet/basic/ucix/ucix.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#include "bacnet/alarm_ack.h"
#include "bacnet/getevent.h"
#include "bacnet/get_alarm_sum.h"
#endif
#include "bacnet/basic/object/analog.h"
/* me! */
#include "bacnet/basic/object/av.h"

static const char *sec = "bacnet_av";
static const char *type = "av";

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_Lists[MAX_NUM_DEVICES];
#ifdef BAC_ROUTING
#define Object_List (Object_Lists[Routed_Device_Object_Index()])
#else
#define Object_List (Object_Lists[0])
#endif
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_ANALOG_VALUE;
/* callback for present value writes */
static analog_value_write_present_value_callback
    Analog_Value_Write_Present_Value_Callback;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int32_t Properties_Required[] = {
    /* unordered list of required properties */
    PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE,
    PROP_STATUS_FLAGS,
    PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE,
    PROP_UNITS,
    PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
#if (BACNET_PROTOCOL_REVISION >= 17)
    PROP_CURRENT_COMMAND_PRIORITY,
#endif
    -1
};

static const int32_t Properties_Optional[] = {
    /* unordered list of optional properties */
    PROP_DESCRIPTION,
    PROP_RELIABILITY,
    PROP_COV_INCREMENT,
    PROP_MIN_PRES_VALUE,
    PROP_MAX_PRES_VALUE,
    PROP_RESOLUTION,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY,
    PROP_NOTIFICATION_CLASS,
    PROP_HIGH_LIMIT,
    PROP_LOW_LIMIT,
    PROP_DEADBAND,
    PROP_LIMIT_ENABLE,
    PROP_EVENT_ENABLE,
    PROP_ACKED_TRANSITIONS,
    PROP_NOTIFY_TYPE,
    PROP_EVENT_TIME_STAMPS,
    PROP_EVENT_DETECTION_ENABLE,
    PROP_EVENT_MESSAGE_TEXTS,
#endif
    -1
};

static const int32_t Properties_Proprietary[] = { -1 };

/* Every object shall have a Writable Property_List property
   which is a BACnetARRAY of property identifiers,
   one property identifier for each property within this object
   that is always writable.  */
static const int32_t Writable_Properties[] = {
    /* unordered list of always writable properties */
    PROP_PRESENT_VALUE,
    PROP_OUT_OF_SERVICE,
    PROP_UNITS,
    PROP_COV_INCREMENT,
    PROP_OBJECT_NAME,
    PROP_RELIABILITY,
    PROP_RELINQUISH_DEFAULT,
    PROP_MAX_PRES_VALUE,
    PROP_MIN_PRES_VALUE,
    PROP_RESOLUTION,
    PROP_DESCRIPTION,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY,
    PROP_NOTIFICATION_CLASS,
    PROP_HIGH_LIMIT,
    PROP_LOW_LIMIT,
    PROP_DEADBAND,
    PROP_LIMIT_ENABLE,
    PROP_EVENT_ENABLE,
    PROP_NOTIFY_TYPE,
    PROP_EVENT_DETECTION_ENABLE,
#endif
    -1
};

/**
 * Initialize the pointers for the required, the optional and the properitary
 * value properties.
 *
 * @param pRequired - Pointer to the pointer of required values.
 * @param pOptional - Pointer to the pointer of optional values.
 * @param pProprietary - Pointer to the pointer of properitary values.
 * export
 */
void Analog_Value_Property_Lists(
    const int32_t **pRequired,
    const int32_t **pOptional,
    const int32_t **pProprietary)
{
    if (pRequired) {
        *pRequired = Properties_Required;
    }
    if (pOptional) {
        *pOptional = Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Properties_Proprietary;
    }

    return;
}

/**
 * @brief Get the list of writable properties for an Analog Input object
 * @param  object_instance - object-instance number of the object
 * @param  properties - Pointer to the pointer of writable properties.
 */
void Analog_Value_Writable_Property_List(
    uint32_t object_instance, const int32_t **properties)
{
    (void)object_instance;
    if (properties) {
        *properties = Writable_Properties;
    }
}

/**
* Analog_Value_Object() replaced by
* Keylist_Data(Object_List, object_instance)
*
* Analog_Value_Object_Index() replaced by
* Keylist_Data(Object_List, Analog_Value_Index_To_Instance(index)
*
*/
/**
 * @brief Gets an object from the list using an instance number as the key
 * @param  object_instance - object-instance number of the object
 * @return object found in the list, or NULL if not found
 */
static struct object_data *Analog_Value_Object(uint32_t object_instance)
{
    return Keylist_Data(Object_List, object_instance);
}

#if 0
#if defined(INTRINSIC_REPORTING)
/**
 * @brief Gets an object from the list using its index in the list
 * @param index - index of the object in the list
 * @return object found in the list, or NULL if not found
 */
static struct object_data *Analog_Value_Object_Index(int index)
{
    return Keylist_Data_Index(Object_List, index);
}
#endif
#endif

/**
 * @brief Determines if a given object instance is valid
 * @param  object_instance - object-instance number of the object
 * @return  true if the instance is valid, and false if not
 * export
 */
bool Analog_Value_Valid_Instance(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return true;
    }

    return false;
}

/**
 * @brief Determines the number of objects
 * @return  Number of objects
 * export
 */
unsigned Analog_Value_Count(void)
{
    return Keylist_Count(Object_List);
}

/**
 * @brief Determines the object instance-number for a given 0..(N-1) index
 * of objects where N is object count.
 * @param  index - 0..(N-1) where N is object count.
 * @return  object instance-number for the given index
 * export
 */
uint32_t Analog_Value_Index_To_Instance(unsigned index)
{
    KEY key = UINT32_MAX;

    Keylist_Index_Key(Object_List, index, &key);

    return key;
}

/**
 * For a given object instance-number, return the name.
 *
 * Note: the object name must be unique within this device
 *
 * @param  object_instance - object-instance number of the object
 * @param  object_name - object name/string pointer
 *
 * @return  true/false
 * export
 */
bool Analog_Value_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    char text_string[32] = "";
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Object_Name) {
            status =
                characterstring_init_ansi(object_name, pObject->Object_Name);
        } else {
            snprintf(
                text_string, sizeof(text_string), "ANALOG VALUE %lu",
                (unsigned long)object_instance);
            status = characterstring_init_ansi(object_name, text_string);
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, determines the COV status
 * @param  object_instance - object-instance number of the object
 * @return  true if the COV flag is set
 * export
 */
bool Analog_Value_Change_Of_Value(uint32_t object_instance)
{
    bool changed = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        changed = pObject->Changed;
    }

    return changed;
}

/**
 * @brief For a given object instance-number, clears the COV flag
 * @param  object_instance - object-instance number of the object
 * export
 */
void Analog_Value_Change_Of_Value_Clear(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Changed = false;
    }
}

/**
 * For a given object instance-number, loads the value_list with the COV data.
 *
 * @param  object_instance - object-instance number of the object
 * @param  value_list - list of COV data
 *
 * @return  true if the value list is encoded
 * export
 */
bool Analog_Value_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    status = Analog_Encode_Value_List(pObject, value_list);

    return status;
}

/**
 * @brief For a given object instance-number, handles the ReadProperty service
 * @param  rpdata Property requested, see for BACNET_READ_PROPERTY_DATA details.
 * @return apdu len, or BACNET_STATUS_ERROR on error
 * export
 */
int Analog_Value_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    uint8_t *apdu = NULL;
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    BACNET_ENGINEERING_UNITS units = 0;
    float real_value = 0.0;
    unsigned i = 0;
    bool state = false;
#if defined(INTRINSIC_REPORTING)
    int apdu_size = 0;
    ACKED_INFO *ack_info[MAX_BACNET_EVENT_TRANSITION] = { 0 };
#else
    // for PROP_PRIORITY_ARRAY
    int apdu_size = 0;
#endif
    struct object_data *pObject;

    /* Valid data? */
    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    pObject = Keylist_Data(Object_List, rpdata->object_instance);
    if (!pObject) {
        return BACNET_STATUS_ERROR;
    }
    if (!property_lists_member(
            Properties_Required,
            Properties_Optional,
            Properties_Proprietary,
            rpdata->object_property)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
        return BACNET_STATUS_ERROR;
    }
    apdu = rpdata->application_data;
#if defined(INTRINSIC_REPORTING)
    apdu_size = rpdata->application_data_len;
#else
    // for PROP_PRIORITY_ARRAY
    apdu_size = rpdata->application_data_len;
#endif
    switch ((int)rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], Object_Type, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Analog_Object_Name(pObject, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(&apdu[0], Object_Type);
            break;
        case PROP_PRESENT_VALUE:
            real_value = Analog_Present_Value(pObject);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_MIN_PRES_VALUE:
            real_value = pObject->Min_Pres_Value;
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_MAX_PRES_VALUE:
            real_value = pObject->Max_Pres_Value;
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
#if defined(INTRINSIC_REPORTING)
            bitstring_set_bit(
                &bit_string, STATUS_FLAG_IN_ALARM,
                pObject->Event_State != EVENT_STATE_NORMAL);
#endif
            state = Analog_Object_Fault(pObject);
            bitstring_set_bit(
                &bit_string, STATUS_FLAG_FAULT, state);
            state = pObject->Overridden;
            bitstring_set_bit(
                &bit_string, STATUS_FLAG_OVERRIDDEN, state);
            state = pObject->Out_Of_Service;
            bitstring_set_bit(
                &bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_STATE:
#if defined(INTRINSIC_REPORTING)
            apdu_len = encode_application_enumerated(
                &apdu[0], pObject->Event_State);
#else
            apdu_len = encode_application_enumerated(
                &apdu[0], EVENT_STATE_NORMAL);
#endif
            break;
        case PROP_RELIABILITY:
            apdu_len =
                encode_application_enumerated(&apdu[0], pObject->Reliability);
            break;
        case PROP_OUT_OF_SERVICE:
            state = pObject->Out_Of_Service;
            apdu_len =
                encode_application_boolean(&apdu[0], state);
            break;
        case PROP_UNITS:
            units = pObject->Units;
            apdu_len = encode_application_enumerated(&apdu[0], units);
            break;
        case PROP_PRIORITY_ARRAY:
            apdu_len = bacnet_array_encode_analog(
                Analog_Value_Object,
                rpdata->object_instance, rpdata->array_index,
                Analog_Priority_Array_Encode,
                BACNET_MAX_PRIORITY, apdu,
                apdu_size);
            if (apdu_len == BACNET_STATUS_ABORT) {
                rpdata->error_code =
                    ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
            } else if (apdu_len == BACNET_STATUS_ERROR) {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
            }
            break;
        case PROP_RELINQUISH_DEFAULT:
            real_value = pObject->Relinquish_Default;
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_DESCRIPTION:
            characterstring_init_ansi(
                &char_string,
                pObject->Description);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_COV_INCREMENT:
            apdu_len =
                encode_application_real(&apdu[0], pObject->COV_Increment);
            break;
        case PROP_RESOLUTION:
            apdu_len = encode_application_real(
                &apdu[0], pObject->Resolution);
            break;
#if (BACNET_PROTOCOL_REVISION >= 17)
        case PROP_CURRENT_COMMAND_PRIORITY:
            i = Analog_Present_Value_Priority(pObject);
            if ((i >= BACNET_MIN_PRIORITY) && (i <= BACNET_MAX_PRIORITY)) {
                apdu_len = encode_application_unsigned(&apdu[0], i);
            } else {
                apdu_len = encode_application_null(&apdu[0]);
            }
            break;
#endif
#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            i = pObject->Time_Delay;
            apdu_len =
                encode_application_unsigned(&apdu[0], i);
            break;
        case PROP_NOTIFICATION_CLASS:
            i = pObject->Notification_Class;
            apdu_len = encode_application_unsigned(
                &apdu[0], i);
            break;
        case PROP_HIGH_LIMIT:
            real_value = pObject->High_Limit;
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_LOW_LIMIT:
            real_value = pObject->Low_Limit;
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_DEADBAND:
            real_value = pObject->Deadband;
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_LIMIT_ENABLE:
            i = pObject->Limit_Enable;
            bitstring_init(&bit_string);
            bitstring_set_bit(
                &bit_string, 0,
                (i & EVENT_LOW_LIMIT_ENABLE) ? true
                                             : false);
            bitstring_set_bit(
                &bit_string, 1,
                (i & EVENT_HIGH_LIMIT_ENABLE) ? true
                                              : false);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_ENABLE:
            i = pObject->Event_Enable;
            bitstring_init(&bit_string);
            bitstring_set_bit(
                &bit_string, TRANSITION_TO_OFFNORMAL,
                (i & EVENT_ENABLE_TO_OFFNORMAL) ? true
                                                : false);
            bitstring_set_bit(
                &bit_string, TRANSITION_TO_FAULT,
                (i & EVENT_ENABLE_TO_FAULT) ? true
                                            : false);
            bitstring_set_bit(
                &bit_string, TRANSITION_TO_NORMAL,
                (i & EVENT_ENABLE_TO_NORMAL) ? true
                                             : false);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_DETECTION_ENABLE:
            state = pObject->Event_Detection_Enable;
            apdu_len = encode_application_boolean(
                &apdu[0], state );
            break;
        case PROP_ACKED_TRANSITIONS:
            state = Analog_Acked_Transitions(pObject, ack_info);
            bitstring_init(&bit_string);
            bitstring_set_bit(
                &bit_string, TRANSITION_TO_OFFNORMAL,
                ack_info[TRANSITION_TO_OFFNORMAL]->bIsAcked);
            bitstring_set_bit(
                &bit_string, TRANSITION_TO_FAULT,
                ack_info[TRANSITION_TO_FAULT]->bIsAcked);
            bitstring_set_bit(
                &bit_string, TRANSITION_TO_NORMAL,
                ack_info[TRANSITION_TO_NORMAL]->bIsAcked);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_NOTIFY_TYPE:
            i = pObject->Notify_Type;
            apdu_len = encode_application_enumerated(
                &apdu[0], i ? NOTIFY_EVENT : NOTIFY_ALARM);
            break;
        case PROP_EVENT_TIME_STAMPS:
            apdu_len = bacnet_array_encode_analog(
                Analog_Value_Object,
                rpdata->object_instance, rpdata->array_index,
                Analog_Event_Time_Stamps_Encode,
                MAX_BACNET_EVENT_TRANSITION, apdu, apdu_size);
            if (apdu_len == BACNET_STATUS_ABORT) {
                rpdata->error_code =
                    ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
            } else if (apdu_len == BACNET_STATUS_ERROR) {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
            }
            break;
        case PROP_EVENT_MESSAGE_TEXTS:
            apdu_len = bacnet_array_encode_analog(
                Analog_Value_Object,
                rpdata->object_instance, rpdata->array_index,
                Analog_Event_Message_Texts_Encode,
                MAX_BACNET_EVENT_TRANSITION, apdu, apdu_size);
            if (apdu_len == BACNET_STATUS_ABORT) {
                rpdata->error_code =
                    ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
            } else if (apdu_len == BACNET_STATUS_ERROR) {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
            }
            break;
#endif
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }

    return apdu_len;
}

/**
 * @brief WriteProperty handler for this object.  For the given WriteProperty
 * data, the application_data is loaded or the error flags are set.
 * @param  wp_data - BACNET_WRITE_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 * @return false if an error is loaded, true if no errors
 * export
 */
bool Analog_Value_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value = { 0 };
    struct uci_context *ctxw = NULL;
    char *idx_c = NULL;
    int idx_c_len = 0;
    float value_f = 0.0f;
    float resolution = 0.1f;

    struct object_data *pObject;
    /* Valid data? */
    if (wp_data == NULL) {
        return false;
    }
    if (wp_data->application_data_len == 0) {
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
    if (!pObject) {
        return false;
    }
    ctxw = ucix_init(sec);
    if (!ctxw) {
        debug_log_fprintf(
            DEBUG_LOG_INFO, stderr,
            "Failed to load config file %s\n",sec);
        return false;
    }
    idx_c_len = snprintf(NULL, 0, "%d", wp_data->object_instance);
    idx_c = malloc(idx_c_len + 1);
    snprintf(idx_c,idx_c_len + 1,"%d",wp_data->object_instance);
    resolution = pObject->Resolution;
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Present_Value_Write(pObject,
                    value.type.Real, wp_data->priority,
                    &wp_data->error_class, &wp_data->error_code)) {
                    value_f = Analog_Present_Value(pObject);
                    ucix_add_option_float(ctxw, sec, idx_c, "value",
                        value_f, resolution);
                    ucix_commit(ctxw,sec);
                }
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_NULL);
                if (status) {
                    if (Analog_Present_Value_Relinquish_Write(
                        pObject, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code)) {
                        value_f = Analog_Present_Value(pObject);
                        ucix_add_option_float(ctxw, sec, idx_c, "value",
                            value_f, resolution);
                        ucix_commit(ctxw,sec);
                    }
                }
            }
            break;
        case PROP_OUT_OF_SERVICE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                Analog_Out_Of_Service_Set(
                    pObject, value.type.Boolean);
            }
            break;
        case PROP_UNITS:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                pObject->Units = value.type.Enumerated;
                ucix_add_option_int(ctxw, sec, idx_c, "units",
                    pObject->Units);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_COV_INCREMENT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                Analog_COV_Increment_Set(pObject,
                value.type.Real);
                ucix_add_option_float(ctxw, sec, idx_c, "cov_increment",
                    pObject->COV_Increment, resolution);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_TYPE:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_OBJECT_NAME:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Analog_Name_Set(
                    pObject, value.type.Character_String.value, Object_Type, wp_data->object_instance)) {
                    ucix_add_option(ctxw, sec, idx_c, "name",
                        characterstring_value_const(&value.type.Character_String));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_RELIABILITY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status)
                Analog_Reliability_Set(pObject,
                    value.type.Enumerated);
            break;
        case PROP_PRIORITY_ARRAY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_RELINQUISH_DEFAULT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status)
                Analog_Relinquish_Default_Set(pObject,
                    value.type.Real);
            break;
        case PROP_MAX_PRES_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Max_Pres_Value_Set(pObject,
                    value.type.Real)) {
                    ucix_add_option_float(ctxw, sec, idx_c, "max_value",
                        pObject->Max_Pres_Value, resolution);
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_MIN_PRES_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Min_Pres_Value_Set(pObject,
                    value.type.Real)) {
                    ucix_add_option_float(ctxw, sec, idx_c, "min_value",
                        pObject->Min_Pres_Value, resolution);
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_RESOLUTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                pObject->Resolution = value.type.Real;
                ucix_add_option_float(ctxw, sec, idx_c, "resolution",
                    pObject->Resolution, 0);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_DESCRIPTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                pObject->Description = bacnet_strndup(
                    value.type.Character_String.value,
                    value.type.Character_String.length);
                ucix_add_option(ctxw, sec, idx_c, "description",
                    Analog_Description(pObject));
                ucix_commit(ctxw,sec);
            }
            break;
#if (BACNET_PROTOCOL_REVISION >= 17)
        case PROP_CURRENT_COMMAND_PRIORITY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
#endif
#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                pObject->Time_Delay = value.type.Unsigned_Int;
                ucix_add_option_int(ctxw, sec, idx_c, "time_delay",
                    pObject->Time_Delay);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_NOTIFICATION_CLASS:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                pObject->Notification_Class = value.type.Unsigned_Int;
                ucix_add_option_int(ctxw, sec, idx_c, "nc",
                    pObject->Notification_Class);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_HIGH_LIMIT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_High_Limit_Set(pObject,
                    value.type.Real)) {
                    ucix_add_option_float(ctxw, sec, idx_c, "high_limit",
                        pObject->High_Limit, resolution);
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_LOW_LIMIT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Low_Limit_Set(pObject,
                    value.type.Real)) {
                    ucix_add_option_float(ctxw, sec, idx_c, "low_limit",
                        pObject->Low_Limit, resolution);
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_DEADBAND:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Deadband_Set(pObject,
                    value.type.Real)) {
                    ucix_add_option_float(ctxw, sec, idx_c, "dead_limit",
                        pObject->Deadband, resolution);
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_LIMIT_ENABLE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BIT_STRING);
            if (status) {
                pObject->Limit_Enable = value.type.Bit_String.value[0];
                ucix_add_option_int(ctxw, sec, idx_c, "limit",
                    pObject->Limit_Enable);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_EVENT_ENABLE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BIT_STRING);
            if (status) {
                pObject->Event_Enable = value.type.Bit_String.value[0];
                ucix_add_option_int(ctxw, sec, idx_c, "event",
                    pObject->Event_Enable);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_NOTIFY_TYPE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                pObject->Notify_Type = value.type.Enumerated;
                ucix_add_option_int(ctxw, sec, idx_c, "notify_type",
                    pObject->Notify_Type);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_ACKED_TRANSITIONS:
        case PROP_EVENT_TIME_STAMPS:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_EVENT_DETECTION_ENABLE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                if (Analog_Event_Detection_Enable_Set(
                    pObject, value.type.Boolean)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "event_detection",
                        pObject->Event_Detection_Enable);
                    ucix_commit(ctxw,sec);
                }
            }
            break;
#endif
        default:
            if (property_lists_member(
                    Properties_Required,
                    Properties_Optional,
                    Properties_Proprietary,
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
 * @brief Handles the Intrinsic Reporting Service for the Analog Input Object
 * @param  object_instance - object-instance number of the object
 * export
 */
void Analog_Value_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    struct object_data *pObject = NULL;
    pObject = Keylist_Data(Object_List, object_instance);
    Analog_Intrinsic_Reporting(
        pObject,
        Object_Type,
        object_instance);
#else
    (void)object_instance;
#endif /* defined(INTRINSIC_REPORTING) */
}

#if defined(INTRINSIC_REPORTING)
/**
 * @brief Handles getting the Event Information for this object.
 * @param  index - index number of the object 0..count
 * @param  getevent_data - data for the Event Information
 * @return 1 if an active event is found, 0 if no active event, -1 if
 * end of list
 */
int Analog_Value_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    int i = 0;
    struct object_data *pObject;
    uint32_t instance;

    pObject = Keylist_Data(Object_List, Analog_Value_Index_To_Instance(index));
    instance = Analog_Value_Index_To_Instance(index);
    i = Analog_Event_Information(pObject, Object_Type, instance, getevent_data);
    return i;
}

/**
 * @brief Acknowledges the Event Information for this object.
 * @param alarmack_data - data for the Event Acknowledgement
 * @param error_code - error code for the Event Acknowledgement
 * @return 1 if successful, -1 if error, -2 if request is out-of-range
 */
int Analog_Value_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code)
{
    struct object_data *pObject;

    if (!alarmack_data) {
        return -1;
    }
    pObject =
        Keylist_Data(Object_List, alarmack_data->eventObjectIdentifier.instance);
    return Analog_Alarm_Ack(pObject,alarmack_data,error_code);
}

/**
 * @brief Handles getting the Alarm Summary for this object.
 * @param  index - index number of the object 0..count
 * @param  getalarm_data - data for the Alarm Summary
 * @return 1 if an active alarm is found, 0 if no active alarm, -1 if
 * end of list
 */
int Analog_Value_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    struct object_data *pObject;
    uint32_t instance;
    instance = Analog_Value_Index_To_Instance(index);
    pObject = Keylist_Data(Object_List, instance);
    return Analog_Alarm_Summary(pObject, Object_Type, instance, getalarm_data);
}
#endif /* defined(INTRINSIC_REPORTING) */

/**
 * @brief Sets a callback used when present-value is written from BACnet
 * @param cb - callback used to provide indications
 */
void Analog_Value_Write_Present_Value_Callback_Set(
    analog_value_write_present_value_callback cb)
{
    Analog_Value_Write_Present_Value_Callback = cb;
}

/**
 * @brief Set the context used with a specific object instance
 * @param object_instance [in] BACnet object instance number
 * @param context [in] pointer to the context
 */
void *Analog_Value_Context_Get(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return pObject->Context;
    }

    return NULL;
}

/**
 * @brief Set the context used with a specific object instance
 * @param object_instance [in] BACnet object instance number
 * @param context [in] pointer to the context
 */
void Analog_Value_Context_Set(uint32_t object_instance, void *context)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Context = context;
    }
}

/**
 * @brief Creates a Analog Value object
 * @param object_instance - object-instance number of the object
 * @return the object-instance that was created, or BACNET_MAX_INSTANCE
 */
uint32_t Analog_Value_Create(uint32_t object_instance)
{
    struct object_data *pObject = NULL;
    int index = 0;
    unsigned priority = 0;

    if (!Object_List) {
        Object_List = Keylist_Create();
    }
    if (object_instance > BACNET_MAX_INSTANCE) {
        return BACNET_MAX_INSTANCE;
    } else if (object_instance == BACNET_MAX_INSTANCE) {
        /* wildcard instance */
        /* the Object_Identifier property of the newly created object
            shall be initialized to a value that is unique within the
            responding BACnet-user device. The method used to generate
            the object identifier is a local matter.*/
        object_instance = Keylist_Next_Empty_Key(Object_List, 1);
    }
    pObject = Keylist_Data(Object_List, object_instance);
    if (!pObject) {
        pObject = calloc(1, sizeof(struct object_data));
        if (pObject) {
            pObject->Object_Name = NULL;
            pObject->Description = NULL;
            pObject->Reliability = RELIABILITY_NO_FAULT_DETECTED;
            pObject->Overridden = false;
            for (priority = 0; priority < BACNET_MAX_PRIORITY; priority++) {
                pObject->Relinquished[priority] = true;
                pObject->Priority_Array[priority] = 0.0;
            }
            pObject->Relinquish_Default = 0.0;
            pObject->COV_Increment = 1.0;
            pObject->Prior_Value = 0.0;
            pObject->Units = UNITS_NO_UNITS;
            pObject->Out_Of_Service = false;
            pObject->Changed = false;
            pObject->Min_Pres_Value = 0;
            pObject->Max_Pres_Value = 100;
#if defined(INTRINSIC_REPORTING)
            pObject->Event_State = EVENT_STATE_NORMAL;
            pObject->Event_Detection_Enable = true;
            pObject->Time_Delay = 0;
            /* notification class not connected */
            pObject->Notification_Class = BACNET_MAX_INSTANCE;
#endif
            /* add to list */
            index = Keylist_Data_Add(Object_List, object_instance, pObject);
            if (index < 0) {
                free(pObject);
                return BACNET_MAX_INSTANCE;
            }
#if defined(INTRINSIC_REPORTING)
            Analog_Reset_Event_Properties(pObject);
#endif
        } else {
            return BACNET_MAX_INSTANCE;
        }
    }

    Device_Inc_Database_Revision();
    return object_instance;
}

/**
 * @brief Deletes an Analog Value object
 * @param object_instance - object-instance number of the object
 * @return true if the object-instance was deleted
 */
bool Analog_Value_Delete(uint32_t object_instance)
{
    bool status = false;
    struct object_data *pObject = NULL;

    pObject = Keylist_Data_Delete(Object_List, object_instance);
    if (pObject) {
        free(pObject);
        status = true;
        Device_Inc_Database_Revision();
    }

    return status;
}

/**
 * @brief Deletes all the Analog Values and their data
 */
void Analog_Value_Cleanup(void)
{
    struct object_data *pObject;
    uint16_t dev_id;
#ifdef BAC_ROUTING
    uint16_t current_dev_id = Routed_Device_Object_Index();
#endif

    for (dev_id = 0; dev_id < MAX_NUM_DEVICES; dev_id++) {
#ifdef BAC_ROUTING
        Set_Routed_Device_Object_Index(dev_id);
#endif
        if (Object_List) {
            do {
                pObject = Keylist_Data_Pop(Object_List);
                if (pObject) {
                    free(pObject);
                }
            } while (pObject);
            Keylist_Delete(Object_List);
            Object_List = NULL;
        }
    }

#ifdef BAC_ROUTING
    Set_Routed_Device_Object_Index(current_dev_id);
#endif
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
#if defined(INTRINSIC_REPORTING)
    unsigned j;
#endif
    struct object_data *pObject = NULL;
    int index = 0;
    unsigned priority = 0;
    char options[64];
    float value_f = 0.0;
	disable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx, "default") == 0)
		return;
	if (disable)
		return;
    idx = atoi(sec_idx);

    pObject = Keylist_Data(Object_List, idx);
    if (!pObject) {
        pObject = calloc(1, sizeof(struct object_data));
    }
    pObject->Object_Name = ucix_get_option_char(ictx->ctx, ictx->section, sec_idx, "name");
    if (!pObject->Object_Name) {
        snprintf(options, sizeof(options), "Analog Value %i", idx);
        pObject->Object_Name = strndup(options, sizeof(options));
    }

    pObject->Description = ucix_get_option_char(ictx->ctx, ictx->section, sec_idx, "description");
    pObject->Reliability = RELIABILITY_NO_FAULT_DETECTED;
    pObject->Overridden = false;
    for (priority = 0; priority < BACNET_MAX_PRIORITY; priority++) {
        pObject->Relinquished[priority] = true;
        pObject->Priority_Array[priority] = 0.0;
    }
    pObject->Relinquish_Default = 0.0;
    pObject->COV_Increment = ucix_get_option_float(ictx->ctx, ictx->section, sec_idx,
        "cov_increment", ictx->Object.COV_Increment);
    pObject->Resolution = ucix_get_option_float(ictx->ctx, ictx->section, sec_idx,
        "resolution", ictx->Object.Resolution);
    pObject->Units = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
        "si_unit", ictx->Object.Units);
    pObject->Out_Of_Service = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
        "Out_Of_Service", false);
    pObject->Changed = false;
    pObject->Min_Pres_Value = ucix_get_option_float(ictx->ctx, ictx->section, sec_idx,
        "min_value", ictx->Object.Min_Pres_Value);
    pObject->Max_Pres_Value = ucix_get_option_float(ictx->ctx, ictx->section, sec_idx,
        "max_value", ictx->Object.Max_Pres_Value);
    value_f = ucix_get_option_float(ictx->ctx, ictx->section, sec_idx,
        "value", 0);
    pObject->Priority_Array[BACNET_MAX_PRIORITY-1] = value_f;
    pObject->Relinquished[BACNET_MAX_PRIORITY-1] = false;
    pObject->Prior_Value = value_f;
#if defined(INTRINSIC_REPORTING)
    pObject->Event_State = EVENT_STATE_NORMAL;
    /* notification class not connected */
    pObject->Notification_Class = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
        "nc", ictx->Object.Notification_Class);
    pObject->Event_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
        "event", ictx->Object.Event_Enable);
    pObject->Event_Detection_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
        "event_detection", ictx->Object.Event_Detection_Enable);
    pObject->Time_Delay = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
        "time_delay", ictx->Object.Time_Delay);
    pObject->Limit_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
        "limit", ictx->Object.Limit_Enable);
    pObject->High_Limit = ucix_get_option_float(ictx->ctx, ictx->section, sec_idx,
        "high_limit", ictx->Object.High_Limit);
    pObject->Low_Limit = ucix_get_option_float(ictx->ctx, ictx->section, sec_idx,
        "low_limit", ictx->Object.Low_Limit);
    pObject->Deadband = ucix_get_option_float(ictx->ctx, ictx->section, sec_idx,
        "dead_limit", ictx->Object.Deadband);
    pObject->Notify_Type = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
        "notify_type", ictx->Object.Notify_Type);
    pObject->Event_Message_Texts[TRANSITION_TO_OFFNORMAL] =
        ucix_get_option_char(ictx->ctx, ictx->section, sec_idx, "evt_msg_offnormal");
    pObject->Event_Message_Texts[TRANSITION_TO_FAULT] =
        ucix_get_option_char(ictx->ctx, ictx->section, sec_idx, "evt_msg_fault");
    pObject->Event_Message_Texts[TRANSITION_TO_NORMAL] =
        ucix_get_option_char(ictx->ctx, ictx->section, sec_idx, "evt_msg_normal");
    for (j = 0; priority < MAX_BACNET_EVENT_TRANSITION; j++) {
        if (!pObject->Event_Message_Texts[j] &&
            ictx->Object.Event_Message_Texts[j])
            pObject->Event_Message_Texts[j] =
            ictx->Object.Event_Message_Texts[j];
    }

    /* initialize Event time stamps using wildcards
        and set Acked_transitions */
    for (j = 0; j < MAX_BACNET_EVENT_TRANSITION; j++) {
        datetime_wildcard_set(&pObject->Event_Time_Stamps[j]);
        pObject->Acked_Transitions[j].bIsAcked = true;
    }
#endif

    /* add to list */
    index = Keylist_Data_Add(Object_List, idx, pObject);
    if (index >= 0) {
        Device_Inc_Database_Revision();
    }

    return;
}

/**
 * @brief Initializes the Analog Value object data
 * export
 */
void Analog_Value_Init(void)
{
    struct uci_context *ctx;
    struct object_data_t tObject = { 0 };

    struct itr_ctx itr_m;
    uint16_t dev_id;
#ifdef BAC_ROUTING
    uint16_t current_dev_id = Routed_Device_Object_Index();
#endif

    for (dev_id = 0; dev_id < MAX_NUM_DEVICES; dev_id++) {
#ifdef BAC_ROUTING
        Set_Routed_Device_Object_Index(dev_id);
#endif
        if (!Object_List) {
            Object_List = Keylist_Create();
        }
    }

#ifdef BAC_ROUTING
    Set_Routed_Device_Object_Index(current_dev_id);
#endif 

    ctx = ucix_init(sec);
    if (!ctx) {
        debug_log_fprintf(
            DEBUG_LOG_ERROR, stderr,
            "Failed to load config file %s\n",sec);
    } else {

        tObject.COV_Increment = ucix_get_option_float(ctx, sec, "default",
            "cov_increment", 0);
        tObject.Resolution = ucix_get_option_float(ctx, sec, "default",
            "resolution", 0);
        tObject.Units = ucix_get_option_int(ctx, sec, "default",
            "si_unit", 0);
        tObject.Min_Pres_Value = ucix_get_option_float(ctx, sec, "default",
            "min_value", 0);
        tObject.Max_Pres_Value = ucix_get_option_float(ctx, sec, "default",
            "max_value", 0);
#if defined(INTRINSIC_REPORTING)
        tObject.Notification_Class = ucix_get_option_int(ctx, sec, "default",
            "nc", BACNET_MAX_INSTANCE);
        tObject.Event_Enable = ucix_get_option_int(ctx, sec, "default",
            "event", 0);
        tObject.Event_Detection_Enable = ucix_get_option_int(ctx, sec, "default",
            "event_detection", 0);
        tObject.Time_Delay = ucix_get_option_int(ctx, sec, "default",
            "time_delay", 0);
        tObject.Limit_Enable = ucix_get_option_int(ctx, sec, "default",
            "limit", 0);
        tObject.High_Limit = ucix_get_option_float(ctx, sec, "default",
            "high_limit", 100);
        tObject.Low_Limit = ucix_get_option_float(ctx, sec, "default",
            "low_limit", 0);
        tObject.Deadband = ucix_get_option_float(ctx, sec, "default",
            "dead_limit", 0);
        tObject.Notify_Type = ucix_get_option_int(ctx, sec, "default",
            "notify_type", 0);
        tObject.Event_Message_Texts[TRANSITION_TO_OFFNORMAL] =
            ucix_get_option_char(ctx, sec, "default", "evt_msg_offnormal");
        tObject.Event_Message_Texts[TRANSITION_TO_FAULT] =
            ucix_get_option_char(ctx, sec, "default", "evt_msg_fault");
        tObject.Event_Message_Texts[TRANSITION_TO_NORMAL] =
            ucix_get_option_char(ctx, sec, "default", "evt_msg_normal");
#endif
        itr_m.section = sec;
        itr_m.ctx = ctx;
        itr_m.Object = tObject;
        ucix_for_each_section_type(ctx, sec, type,
            (void (*)(const char *, void *))uci_list, &itr_m);
        ucix_cleanup(ctx);
    }
#if defined(INTRINSIC_REPORTING)
    /* Set handler for GetEventInformation function */
    handler_get_event_information_set(Object_Type,
        Analog_Value_Event_Information);
    /* Set handler for AcknowledgeAlarm function */
    handler_alarm_ack_set(Object_Type, Analog_Value_Alarm_Ack);
    /* Set handler for GetAlarmSummary Service */
    handler_get_alarm_summary_set(Object_Type,
        Analog_Value_Alarm_Summary);
#endif
}
