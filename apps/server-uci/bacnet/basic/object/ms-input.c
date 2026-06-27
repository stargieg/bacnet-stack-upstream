/**
 * @file
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2009
 * @brief Multi-State object is an input object with a present-value that
 * uses an integer data type with a sequence of 1 to N values.
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
#include "bacnet/bactext.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacapp.h"
#include "bacnet/rp.h"
#include "bacnet/wp.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/sys/debug.h"
#include "bacnet/basic/ucix/ucix.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#include "bacnet/alarm_ack.h"
#include "bacnet/getevent.h"
#include "bacnet/get_alarm_sum.h"
#endif
#include "bacnet/basic/object/ms.h"
/* me! */
#include "bacnet/basic/object/ms-input.h"

static const char *sec = "bacnet_mi";
static const char *type = "mi";

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_Lists[MAX_NUM_DEVICES];
#ifdef BAC_ROUTING
#define Object_List (Object_Lists[Routed_Device_Object_Index()])
#else
#define Object_List (Object_Lists[0])
#endif
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_MULTI_STATE_INPUT;
/* callback for present value writes */
static multistate_input_write_present_value_callback
    Multistate_Input_Write_Present_Value_Callback;

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
    PROP_NUMBER_OF_STATES,
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
    PROP_STATE_TEXT,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY,
    PROP_NOTIFICATION_CLASS,
    PROP_ALARM_VALUES,
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
    PROP_OBJECT_NAME,
    PROP_STATE_TEXT,
    PROP_NUMBER_OF_STATES,
    PROP_RELIABILITY,
    PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
    PROP_DESCRIPTION,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY,
    PROP_NOTIFICATION_CLASS,
    PROP_ALARM_VALUES,
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
void Multistate_Input_Property_Lists(
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
 * @brief Get the list of writable properties for an object
 * @param  object_instance - object-instance number of the object
 * @param  properties - Pointer to the pointer of writable properties.
 * export
 */
void Multistate_Input_Writable_Property_List(
    uint32_t object_instance, const int32_t **properties)
{
    (void)object_instance;
    if (properties) {
        *properties = Writable_Properties;
    }
}

/**
* Multistate_Input_Object() replaced by
* Keylist_Data(Object_List, object_instance)
* 
* Multistate_Input_Object_Index() replaced by
* Keylist_Data(Object_List, Multistate_Input_Index_To_Instance(index)
* 
*/

/**
 * @brief Gets an object from the list using an instance number as the key
 * @param  object_instance - object-instance number of the object
 * @return object found in the list, or NULL if not found
 */
static struct object_data *Multistate_Input_Object(uint32_t object_instance)
{
    return Keylist_Data(Object_List, object_instance);
}

/**
 * @brief Determines the number of Multistate Input objects
 * @return  Number of Multistate Input objects
 * export
 */
unsigned Multistate_Input_Count(void)
{
    return Keylist_Count(Object_List);
}

/**
 * @brief Determines if a given Multistate Input instance is valid
 * @param  object_instance - object-instance number of the object
 * @return  true if the instance is valid, and false if not
 * export
 */
bool Multistate_Input_Valid_Instance(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        return true;
    }

    return false;
}

/**
 * @brief Determines the object instance-number for a given 0..(N-1) index
 * of objects where N is object count.
 * @param  index - 0..(N-1) where N is object count.
 * @return  object instance-number for the given index
 * export
 */
uint32_t Multistate_Input_Index_To_Instance(unsigned index)
{
    KEY key = UINT32_MAX;

    Keylist_Index_Key(Object_List, index, &key);

    return key;
}

/**
 * @brief For a given object instance-number, loads the object-name into
 *  a characterstring. Note that the object name must be unique
 *  within this device.
 * @param  object_instance - object-instance number of the object
 * @param  object_name - holds the object-name retrieved
 *
 * @return  true if object-name was retrieved
 * export
 */
bool Multistate_Input_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    bool status = false;
    struct object_data *pObject;
    char name_text[32];

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        if (pObject->Object_Name) {
            status =
                characterstring_init_ansi(object_name, pObject->Object_Name);
        } else {
            snprintf(
                name_text, sizeof(name_text), "MULTI-STATE INPUT %lu",
                (unsigned long)object_instance);
            status = characterstring_init_ansi(object_name, name_text);
        }
    }

    return status;
}

/**
 * @brief Get the COV change flag status
 * @param object_instance - object-instance number of the object
 * @return the COV change flag status
 * export
 */
bool Multistate_Input_Change_Of_Value(uint32_t object_instance)
{
    bool changed = false;

    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        changed = pObject->Changed;
    }

    return changed;
}

/**
 * @brief Clear the COV change flag
 * @param object_instance - object-instance number of the object
 * export
 */
void Multistate_Input_Change_Of_Value_Clear(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Changed = false;
    }
}

/**
 * @brief Encode the Value List for Present-Value and Status-Flags
 * @param object_instance - object-instance number of the object
 * @param  value_list - #BACNET_PROPERTY_VALUE with at least 2 entries
 * @return true if values were encoded
 * export
 */
bool Multistate_Input_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list)
{
    bool status = false;
    struct object_data *pObject;
    const bool in_alarm = false;
    bool fault = false;
    const bool overridden = false;
    uint32_t present_value = 1;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        fault = Multistate_Object_Fault(pObject);
        present_value = Multistate_Present_Value(pObject);
        status =
            cov_value_list_encode_unsigned(value_list, present_value,
                in_alarm, fault, overridden, pObject->Out_Of_Service);
    }
    return status;
}

/**
 * @brief ReadProperty handler for this object.  For the given ReadProperty
 *  data, the application_data is loaded or the error flags are set.
 * @param  rpdata - BACNET_READ_PROPERTY_DATA data, including
 *  requested data and space for the reply, or error response.
 * @return number of APDU bytes in the response, or
 *  BACNET_STATUS_ERROR on error.
 * export
 */
int Multistate_Input_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int len = 0;
    int apdu_len = 0; /* return value */
    int apdu_size = 0;
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint32_t present_value = 0;
    unsigned i = 0;
    uint32_t max_states = 0;
    bool state = false;
    uint8_t *apdu = NULL;
#if defined(INTRINSIC_REPORTING)
    ACKED_INFO *ack_info[MAX_BACNET_EVENT_TRANSITION] = { 0 };
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
    apdu_size = rpdata->application_data_len;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], Object_Type, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Multistate_Object_Name(pObject, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(&apdu[0], Object_Type);
            break;
        case PROP_PRESENT_VALUE:
            present_value =
                Multistate_Present_Value(pObject);
            apdu_len = encode_application_unsigned(&apdu[0], present_value);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
#if defined(INTRINSIC_REPORTING)
            bitstring_set_bit(
                &bit_string, STATUS_FLAG_IN_ALARM,
                pObject->Event_State != EVENT_STATE_NORMAL);
#endif
            state = Multistate_Object_Fault(pObject);
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
        case PROP_RELIABILITY:
            apdu_len = encode_application_enumerated(
                &apdu[0], pObject->Reliability);
            break;
        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0], pObject->Event_State);
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;
        case PROP_OUT_OF_SERVICE:
            state = pObject->Out_Of_Service;
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_NUMBER_OF_STATES:
            apdu_len = encode_application_unsigned(
                &apdu[apdu_len], pObject->State_Count);
            break;
        case PROP_STATE_TEXT:
            max_states = pObject->State_Count;
            if (rpdata->array_index == 0) {
                /* Array element zero is the number of elements in the array */
                apdu_len = encode_application_unsigned(&apdu[0], max_states);
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                /* if no index was specified, then try to encode the entire list
                 */
                /* into one packet. */
                for (i = 1; i <= max_states; i++) {
                    characterstring_init_ansi(&char_string,
                        Multistate_State_Text(pObject, i));
                    /* FIXME: this might go beyond MAX_APDU length! */
                    len = encode_application_character_string(
                        &apdu[apdu_len], &char_string);
                    /* add it if we have room */
                    if ((apdu_len + len) < apdu_size) {
                        apdu_len += len;
                    } else {
                        rpdata->error_class = ERROR_CLASS_SERVICES;
                        rpdata->error_code = ERROR_CODE_NO_SPACE_FOR_OBJECT;
                        apdu_len = BACNET_STATUS_ERROR;
                        break;
                    }
                }
            } else {
                if (rpdata->array_index <= max_states) {
                    characterstring_init_ansi(&char_string,
                        Multistate_State_Text(pObject, rpdata->array_index));
                    apdu_len = encode_application_character_string(
                        &apdu[0], &char_string);
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;
        case PROP_PRIORITY_ARRAY:
            apdu_len = bacnet_array_encode_multistate(
                Multistate_Input_Object,
                rpdata->object_instance, rpdata->array_index,
                Multistate_Priority_Array_Encode, BACNET_MAX_PRIORITY,
                apdu, apdu_size);
            if (apdu_len == BACNET_STATUS_ABORT) {
                        rpdata->error_code =
                            ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
            } else if (apdu_len == BACNET_STATUS_ERROR) {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
            }
            break;
        case PROP_RELINQUISH_DEFAULT:
            present_value = pObject->Relinquish_Default;
            apdu_len = encode_application_unsigned(&apdu[0], present_value);
            break;
#if (BACNET_PROTOCOL_REVISION >= 17)
        case PROP_CURRENT_COMMAND_PRIORITY:
            i = Multistate_Present_Value_Priority(pObject);
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
            apdu_len = encode_application_unsigned(&apdu[0], i);
            break;

        case PROP_NOTIFICATION_CLASS:
            i = pObject->Notification_Class;
            apdu_len = encode_application_unsigned(&apdu[0], i);
            break;

        case PROP_ALARM_VALUES:
            max_states = pObject->State_Count;
            if (rpdata->array_index == 0) {
                /* Array element zero is the number of elements in the array */
                apdu_len = encode_application_unsigned(&apdu[0], max_states);
            } else {
                /* if no index was specified, then try to encode the entire list
                 */
                /* into one packet. */
                for (i = 1; i <= max_states; i++) {
                    if (pObject->Alarm_State[i-1]) {
                        len = encode_application_unsigned(
                            &apdu[apdu_len], i);
                        /* add it if we have room */
                        if ((apdu_len + len) < apdu_size) {
                            apdu_len += len;
                        } else {
                            rpdata->error_class = ERROR_CLASS_SERVICES;
                            rpdata->error_code = ERROR_CODE_NO_SPACE_FOR_OBJECT;
                            apdu_len = BACNET_STATUS_ERROR;
                            break;
                        }
                    }
                }
            }
            break;

        case PROP_EVENT_ENABLE:
            i = pObject->Event_Enable;
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                (i & EVENT_ENABLE_TO_OFFNORMAL) ? true
                                                : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                (i & EVENT_ENABLE_TO_FAULT) ? true
                                            : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                (i & EVENT_ENABLE_TO_NORMAL) ? true
                                             : false);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_EVENT_DETECTION_ENABLE:
            apdu_len = encode_application_boolean(
                &apdu[0], pObject->Event_Detection_Enable);
            break;

        case PROP_ACKED_TRANSITIONS:
            state = Multistate_Acked_Transitions(pObject, ack_info);
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                ack_info[TRANSITION_TO_OFFNORMAL]->bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                ack_info[TRANSITION_TO_FAULT]->bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                ack_info[TRANSITION_TO_NORMAL]->bIsAcked);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_NOTIFY_TYPE:
            i = pObject->Notify_Type;
            apdu_len = encode_application_enumerated(
                &apdu[0], i ? NOTIFY_EVENT : NOTIFY_ALARM);
            break;

        case PROP_EVENT_TIME_STAMPS:
            apdu_len = bacnet_array_encode_multistate(
                Multistate_Input_Object,
                rpdata->object_instance, rpdata->array_index,
                Multistate_Event_Time_Stamps_Encode,
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
            apdu_len = bacnet_array_encode_multistate(
                Multistate_Input_Object,
                rpdata->object_instance, rpdata->array_index,
                Multistate_Event_Message_Texts_Encode,
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
        case PROP_DESCRIPTION:
            characterstring_init_ansi(
                &char_string,
                pObject->Description);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
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
 *  data, the application_data is loaded or the error flags are set.
 * @param  wp_data - BACNET_WRITE_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 * @return false if an error is loaded, true if no errors
 * export
 */
bool Multistate_Input_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    int element_len = 0;
    BACNET_APPLICATION_DATA_VALUE value = { 0 };
    uint32_t idx = 0;
    struct uci_context *ctxw = NULL;
    char *idx_c = NULL;
    int idx_c_len = 0;
    uint32_t value_i = false;
    char *value_c = NULL;
    const char *pName = NULL;
    BACNET_CHARACTER_STRING char_string = { 0 };
    char stats[254][64];
    uint32_t stats_n = 0;
    uint32_t k = 0;

    struct object_data *pObject;
    /* Valid data? */
    if (wp_data == NULL) {
        return false;
    }
    if (wp_data->application_data_len == 0) {
        return false;
    }
    /* decode the first chunk of the request */
    len = bacapp_decode_application_data(
        wp_data->application_data, wp_data->application_data_len, &value);
    /* len < application_data_len: extra data for arrays only */
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
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                status = false;
                if (value.type.Unsigned_Int <= UINT32_MAX) {
                    if (Multistate_Present_Value_Write(pObject,
                        value.type.Unsigned_Int, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code)) {
                        value_i = Multistate_Present_Value(pObject);
                        ucix_add_option_int(ctxw, sec, idx_c, "value", value_i);
                        ucix_commit(ctxw,sec);
                        free(value_c);
                        status = true;
                    };
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_NULL);
                if (status) {
                    if (Multistate_Present_Value_Relinquish_Write(
                        pObject, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code)) {
                        value_i = Multistate_Present_Value(pObject);
                        ucix_add_option_int(ctxw, sec, idx_c, "value", value_i);
                        ucix_commit(ctxw,sec);
                        free(value_c);
                    };
                }
            }
            break;
        case PROP_OUT_OF_SERVICE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                Multistate_Out_Of_Service_Set(
                    pObject, value.type.Boolean);
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
                if (Multistate_Name_Set(
                    pObject, value.type.Character_String.value,
                    Object_Type,wp_data->object_instance)) {
                    ucix_add_option(ctxw, sec, idx_c, "name",
                        strndup(value.type.Character_String.value,value.type.Character_String.length));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_STATE_TEXT:
            if (wp_data->array_index == 0) {
                /* Array element zero is the number of
                   elements in the array.  We have a fixed
                   size array, so we are read-only. */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            } else if (wp_data->array_index == BACNET_ARRAY_ALL) {
                element_len = len;
                for (idx = 1; idx <= 255; idx++ ) {
                    status = write_property_type_valid(wp_data, &value,
                        BACNET_APPLICATION_TAG_CHARACTER_STRING);
                    if (!status) {
                        break;
                    }
                    if (element_len) {
                        status = Multistate_State_Text_Set(
                            pObject, idx,
                            &value.type.Character_String);
                    }
                    element_len = bacapp_decode_application_data(
                        &wp_data->application_data[len],
                        wp_data->application_data_len - len, &value);
                    if (element_len <= 0) {
                        break;
                    }
                    len += element_len;
                }
                if (idx != pObject->State_Count) {
                    pObject->State_Count = idx;
                }
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_CHARACTER_STRING);
                if (status) {
                    status = Multistate_State_Text_Set(
                        pObject, wp_data->array_index,
                        &value.type.Character_String);
                }
            }
            if (status) {
                stats_n = pObject->State_Count;
                for (k = 0 ; k < stats_n; k++) {
                    pName = Multistate_State_Text(pObject, k+1);
                    if (pName) {
                        characterstring_init_ansi(&char_string, pName);
                        sprintf(stats[k], "%s", char_string.value);
                    }
                }
                ucix_set_list(ctxw, sec, idx_c, "state",
                stats, stats_n);
                ucix_commit(ctxw,sec);
            }
            break;
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_NUMBER_OF_STATES:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status)
                pObject->State_Count = value.type.Enumerated;
            break;
        case PROP_RELIABILITY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status)
                Multistate_Reliability_Set(pObject,
                    value.type.Enumerated);
            break;
        case PROP_PRIORITY_ARRAY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_RELINQUISH_DEFAULT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status)
                pObject->Relinquish_Default = value.type.Unsigned_Int;
            break;
        case PROP_DESCRIPTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                pObject->Description = value.type.Character_String.value;
                ucix_add_option(ctxw, sec, idx_c, "description",
                    Multistate_Description(pObject));
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
        case PROP_ALARM_VALUES:
            if (wp_data->array_index == 0) {
                /* Array element zero is the number of
                   elements in the array.  We have a fixed
                   size array, so we are read-only. */
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            } else if (wp_data->array_index == BACNET_ARRAY_ALL) {
                element_len = len;
                for (idx = 1; idx <= 255; idx++ ) {
                    pObject->Alarm_State[idx-1] = false;
                }
                for (idx = 0; idx < 255; idx++ ) {
                    status = write_property_type_valid(wp_data, &value,
                        BACNET_APPLICATION_TAG_UNSIGNED_INT);
                    if (!status) {
                        break;
                    }
                    if (element_len) {
                        pObject->Alarm_State[value.type.Unsigned_Int-1] = true;
                    }
                    element_len = bacapp_decode_application_data(
                        &wp_data->application_data[len],
                        wp_data->application_data_len - len, &value);
                    if (element_len <= 0) {
                        break;
                    }
                    len += element_len;
                }
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_UNSIGNED_INT);
                if (status) {
                    pObject->Alarm_State[wp_data->array_index-1] = value.type.Unsigned_Int;
                }
            }
            if (status) {
                stats_n = pObject->State_Count;
                k = 0;
                for (idx = 1 ; idx <= stats_n; idx++) {
                    if (pObject->Alarm_State[idx-1]) {
                        sprintf(stats[k], "%i", idx);
                        k++;
                    }
                }
                ucix_set_list(ctxw, sec, idx_c, "alarm", stats, k);
                ucix_commit(ctxw, sec);
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
                if (Multistate_Event_Detection_Enable_Set(
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
                    Properties_Required, Properties_Optional,
                    Properties_Proprietary, wp_data->object_property)) {
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
 * @brief Sets a callback used when present-value is written from BACnet
 * @param cb - callback used to provide indications
 */
void Multistate_Input_Write_Present_Value_Callback_Set(
    multistate_input_write_present_value_callback cb)
{
    Multistate_Input_Write_Present_Value_Callback = cb;
}

/**
 * @brief Set the context used with a specific object instance
 * @param object_instance [in] BACnet object instance number
 * @param context [in] pointer to the context
 */
void *Multistate_Input_Context_Get(uint32_t object_instance)
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
void Multistate_Input_Context_Set(uint32_t object_instance, void *context)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Context = context;
    }
}

/**
 * @brief Creates a new object and adds it to the object list
 * @param  object_instance - object-instance number of the object
 * @return the object-instance that was created, or BACNET_MAX_INSTANCE
 * export
 */
uint32_t Multistate_Input_Create(uint32_t object_instance)
{
    struct object_data *pObject = NULL;
    int index = 0;
    uint8_t priority = 0;

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
            pObject->Out_Of_Service = false;
            pObject->Reliability = RELIABILITY_NO_FAULT_DETECTED;
            pObject->Changed = false;
            for (priority = 0; priority < BACNET_MAX_PRIORITY; priority++) {
                pObject->Relinquished[priority] = true;
                pObject->Priority_Array[priority] = 0;
            }
            pObject->Relinquish_Default = 1;
            /* add to list */
            index = Keylist_Data_Add(Object_List, object_instance, pObject);
            if (index < 0) {
                free(pObject);
                return BACNET_MAX_INSTANCE;
            }
            Device_Inc_Database_Revision();
        } else {
            return BACNET_MAX_INSTANCE;
        }
    }

    return object_instance;
}

/**
 * @brief Delete an object and its data from the object list
 * @param  object_instance - object-instance number of the object
 * @return true if the object is deleted
 * export
 */
bool Multistate_Input_Delete(uint32_t object_instance)
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
 * @brief Cleans up the object list and its data
 */
void Multistate_Input_Cleanup(void)
{
    struct object_data *pObject;

    if (Object_List) {
        do {
            pObject = Keylist_Data_Pop(Object_List);
            if (pObject) {
                free(pObject);
                Device_Inc_Database_Revision();
            }
        } while (pObject);
        Keylist_Delete(Object_List);
        Object_List = NULL;
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
    char *stats[254];
    uint32_t stats_n = 0;
    uint32_t k = 0;
#if defined(INTRINSIC_REPORTING)
    uint32_t l = 0;
    unsigned j;
#endif
    struct object_data *pObject = NULL;
    int index = 0;
    uint8_t priority = 0;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
    uint32_t value_i = 1;
	disable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx, "default") == 0)
		return;
	if (disable)
		return;
    idx = atoi(sec_idx);
    pObject = calloc(1, sizeof(struct object_data));

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "name");
    if (option && characterstring_init_ansi(&option_str, option))
        pObject->Object_Name = strndup(option,option_str.length);

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "description");
    if (option && characterstring_init_ansi(&option_str, option))
        pObject->Description = strndup(option,option_str.length);
    else
        pObject->Description = strdup(ictx->Object.Description);

    pObject->Reliability = RELIABILITY_NO_FAULT_DETECTED;
    pObject->Overridden = false;
    for (priority = 0; priority < BACNET_MAX_PRIORITY; priority++) {
        pObject->Relinquished[priority] = true;
        pObject->Priority_Array[priority] = false;
    }
    pObject->Relinquish_Default = false;
    pObject->Out_Of_Service = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "Out_Of_Service", false);
    pObject->Changed = false;
    value_i = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "value", 0);
    pObject->Priority_Array[BACNET_MAX_PRIORITY-1] = value_i;
    pObject->Relinquished[BACNET_MAX_PRIORITY-1] = false;
    pObject->Prior_Value = value_i;
    stats_n = ucix_get_list(stats, ictx->ctx, ictx->section, sec_idx,
        "state");
    if (stats_n) {
        for (k = 0 ; k < stats_n; k++) {
            pObject->State_Text[k] = strdup(stats[k]);
        }
        pObject->State_Count = stats_n;
    } else {
        for (k = 0 ; k < ictx->Object.State_Count; k++) {
            pObject->State_Text[k] = strdup(ictx->Object.State_Text[k]);
        }
        pObject->State_Count = ictx->Object.State_Count;
    }
#if defined(INTRINSIC_REPORTING)
    pObject->Event_State = EVENT_STATE_NORMAL;
    /* notification class not connected */
    pObject->Notification_Class = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "nc", ictx->Object.Notification_Class);
    pObject->Event_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "event", ictx->Object.Event_Enable);
    pObject->Event_Detection_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "event_detection", ictx->Object.Event_Detection_Enable);
    pObject->Time_Delay = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "time_delay", ictx->Object.Time_Delay);
    stats_n = ucix_get_list(stats, ictx->ctx, ictx->section, sec_idx, "alarm");
    if (stats_n) {
        for (l = 0 ; l < pObject->State_Count; l++) {
            pObject->Alarm_State[l] = false;
        }
        for (k = 0 ; k < stats_n; k++) {
            l = atoi(stats[k]);
            l--;
            pObject->Alarm_State[l] = true;
        }
    } else {
        for (k = 0 ; k < pObject->State_Count; k++) {
            pObject->Alarm_State[k] = ictx->Object.Alarm_State[k];
        }
    }

    pObject->Notify_Type = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "notify_type", ictx->Object.Notify_Type);

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
 * @brief Initializes the object list
 * export
 */
void Multistate_Input_Init(void)
{
    struct uci_context *ctx;
    struct object_data_t tObject = { 0 };
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str = { 0 };
    char *stats[254];
    uint32_t stats_n = 0;
    uint32_t k = 0;
#if defined(INTRINSIC_REPORTING)
    uint32_t l = 0;
#endif

    struct itr_ctx itr_m;
    if (!Object_List) {
        Object_List = Keylist_Create();
    }

    ctx = ucix_init(sec);
    if (!ctx) {
        debug_log_fprintf(
            DEBUG_LOG_ERROR, stderr,
            "Failed to load config file %s\n",sec);
    } else {

        option = ucix_get_option(ctx, sec, "default", "description");
        if (option && characterstring_init_ansi(&option_str, option))
            tObject.Description = strndup(option,option_str.length);
        else
            tObject.Description = "Multistate Ouput";
        stats_n = ucix_get_list(stats, ctx, sec, "default",
            "state");
        if (stats_n) {
            for (k = 0 ; k < stats_n; k++) {
                tObject.State_Text[k] = strdup(stats[k]);
            }
            tObject.State_Count = stats_n;
        }
#if defined(INTRINSIC_REPORTING)
        tObject.Notification_Class = ucix_get_option_int(ctx, sec, "default", "nc", BACNET_MAX_INSTANCE);
        tObject.Event_Enable = ucix_get_option_int(ctx, sec, "default", "event", 0);
        tObject.Event_Detection_Enable = ucix_get_option_int(ctx, sec, "default", "event_detection", 0);
        tObject.Time_Delay = ucix_get_option_int(ctx, sec, "default", "time_delay", 0);
        stats_n = ucix_get_list(stats, ctx, sec, "default", "alarm");
        if (stats_n) {
            for (k = 0 ; k < stats_n; k++) {
                l = atoi(stats[k]);
                l--;
                if (l < tObject.State_Count) {
                    tObject.Alarm_State[l] = true;
                }
            }
        }
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
        Multistate_Input_Event_Information);
    /* Set handler for AcknowledgeAlarm function */
    handler_alarm_ack_set(Object_Type, Multistate_Input_Alarm_Ack);
    /* Set handler for GetAlarmSummary Service */
    handler_get_alarm_summary_set(Object_Type,
        Multistate_Input_Alarm_Summary);
#endif
}

/**
 * @brief Handles the Intrinsic Reporting Service for the Object
 * @param  object_instance - object-instance number of the object
 * export
 */
void Multistate_Input_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    struct object_data *pObject = NULL;
    pObject = Keylist_Data(Object_List, object_instance);
    Multistate_Intrinsic_Reporting(
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
 * handler
 */
int Multistate_Input_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    int i = 0;
    struct object_data *pObject;
    uint32_t instance;

    instance = Multistate_Input_Index_To_Instance(index);
    pObject = Keylist_Data(Object_List, instance);
    i = Multistate_Event_Information(pObject, Object_Type, instance, getevent_data);
    return i;
}

/**
 * @brief Acknowledges the Event Information for this object.
 * @param alarmack_data - data for the Event Acknowledgement
 * @param error_code - error code for the Event Acknowledgement
 * @return 1 if successful, -1 if error, -2 if request is out-of-range
 * handler
 */
int Multistate_Input_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code)
{
    struct object_data *pObject;

    if (!alarmack_data) {
        return -1;
    }
    pObject =
        Keylist_Data(Object_List, alarmack_data->eventObjectIdentifier.instance);
    return Multistate_Alarm_Ack(pObject, alarmack_data, error_code);
}

/**
 * @brief Handles getting the Alarm Summary for this object.
 * @param  index - index number of the object 0..count
 * @param  getalarm_data - data for the Alarm Summary
 * @return 1 if an active alarm is found, 0 if no active alarm, -1 if
 * end of list
 * handler
 */
int Multistate_Input_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    struct object_data *pObject;
    uint32_t instance;
    instance = Multistate_Input_Index_To_Instance(index);
    pObject = Keylist_Data(Object_List, instance);
    return Multistate_Alarm_Summary(pObject, Object_Type, instance, getalarm_data);
}
#endif /* defined(INTRINSIC_REPORTING) */
