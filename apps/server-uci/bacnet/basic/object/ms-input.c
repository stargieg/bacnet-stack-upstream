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
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/ucix/ucix.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#include "bacnet/alarm_ack.h"
#include "bacnet/getevent.h"
#include "bacnet/get_alarm_sum.h"
#endif
/* me! */
#include "bacnet/basic/object/ms-input.h"

static const char *sec = "bacnet_mi";
static const char *type = "mi";

struct object_data {
    bool Out_Of_Service : 1;
    bool Overridden : 1;
    bool Changed : 1;
    bool Write_Enabled : 1;
    uint8_t Prior_Value;
    bool Relinquished[BACNET_MAX_PRIORITY];
    uint8_t Priority_Array[BACNET_MAX_PRIORITY];
    uint8_t Relinquish_Default;
    uint8_t Reliability;
    const char *Object_Name;
    /* The state text functions expect a list of C strings separated by '\0' */
    const char *State_Text[254];
    uint32_t State_Count;
    const char *Description;
    void *Context;
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State:3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    bool Alarm_State[254];
    unsigned Event_Enable:3;
    unsigned Event_Detection_Enable : 1;
    unsigned Notify_Type:1;
    ACKED_INFO Acked_Transitions[MAX_BACNET_EVENT_TRANSITION];
    BACNET_DATE_TIME Event_Time_Stamps[MAX_BACNET_EVENT_TRANSITION];
    /* time to generate event notification */
    uint32_t Remaining_Time_Delay;
    /* AckNotification informations */
    ACK_NOTIFICATION Ack_notify_data;
#endif /* INTRINSIC_REPORTING */
};

struct object_data_t {
    bool Out_Of_Service : 1;
    const char *Prior_Value;
    const char *Relinquish_Default;
    uint8_t Reliability;
    const char *Object_Name;
    const char *State_Text[254];
    uint32_t State_Count;
    const char *Description;
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State:3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    bool Alarm_State[254];
    unsigned Limit_Enable:2;
    unsigned Event_Enable:3;
    unsigned Event_Detection_Enable:1;
    unsigned Notify_Type:1;
#endif /* INTRINSIC_REPORTING */
};

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_MULTI_STATE_INPUT;
/* callback for present value writes */
static multistate_input_write_present_value_callback
    Multistate_Input_Write_Present_Value_Callback;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int32_t Properties_Required[] = { 
    PROP_OBJECT_IDENTIFIER, PROP_OBJECT_NAME,      PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE,     PROP_STATUS_FLAGS,     PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE,    PROP_NUMBER_OF_STATES, PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
#if (BACNET_PROTOCOL_REVISION >= 17)
    PROP_CURRENT_COMMAND_PRIORITY,
#endif
    -1
};

static const int32_t Properties_Optional[] = {
    /* unordered list of optional properties */
    PROP_DESCRIPTION, PROP_RELIABILITY, PROP_STATE_TEXT,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY, PROP_NOTIFICATION_CLASS, PROP_ALARM_VALUES, PROP_EVENT_ENABLE,
    PROP_ACKED_TRANSITIONS, PROP_NOTIFY_TYPE, PROP_EVENT_TIME_STAMPS,
#endif
    -1 };

static const int32_t Properties_Proprietary[] = { -1 };

/**
 * Initialize the pointers for the required, the optional and the properitary
 * value properties.
 *
 * @param pRequired - Pointer to the pointer of required values.
 * @param pOptional - Pointer to the pointer of optional values.
 * @param pProprietary - Pointer to the pointer of properitary values.
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
 * @brief Gets an object from the list using an instance number as the key
 * @param  object_instance - object-instance number of the object
 * @return object found in the list, or NULL if not found
 */
static struct object_data *Multistate_Input_Object(uint32_t object_instance)
{
    return Keylist_Data(Object_List, object_instance);
}

/**
 * @brief For a given object instance-number, determines a 0..N index
 * of Multistate objects where N is count.
 * @param  object_instance - object-instance number of the object
 * @return  index for the given instance-number, or count (object not found)
 */
unsigned Multistate_Input_Instance_To_Index(uint32_t object_instance)
{
    return Keylist_Index(Object_List, object_instance);
}

/**
 * @brief Determines the object instance-number for a given 0..N index
 * of objects where N is the count.
 * @param  index - 0..N value
 * @return  object instance-number for a valid given index, or UINT32_MAX
 */
uint32_t Multistate_Input_Index_To_Instance(unsigned index)
{
    uint32_t instance = UINT32_MAX;

    (void)Keylist_Index_Key(Object_List, index, &instance);

    return instance;
}

/**
 * @brief Determines the number of Multistate Input objects
 * @return  Number of Multistate Input objects
 */
unsigned Multistate_Input_Count(void)
{
    return Keylist_Count(Object_List)-1;
}

/**
 * @brief Determines if a given Multistate Input instance is valid
 * @param  object_instance - object-instance number of the object
 * @return  true if the instance is valid, and false if not
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
 * @brief Get the specific state name at index 0..N
 * @param struct object_data
 * @param state_index - state index number 1..N of the state names
 * @return state name, or NULL
 */
static char *state_name_by_index(const struct object_data *pObject, unsigned index)
{
    char *pName = NULL; /* return value */

    if (pObject) {
        if (index > 0) {
            index--;
            pName = (char *)pObject->State_Text[index];
        }
    }
    return pName;
}

/**
 * @brief For a given object instance-number, determines number of states
 * @param  object_instance - object-instance number of the object
 * @return  number of states 1..N
 */
uint32_t Multistate_Input_Max_States(uint32_t object_instance)
{
    uint32_t count = 0;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        count = pObject->State_Count;
    }

    return count;
}

/**
 * @brief For a given object instance-number, set number of states
 * @param  object_instance - object-instance number of the object
 * @param  count - number of states
 * @return  status
 */
bool Multistate_Input_Max_States_Set(uint32_t object_instance,
    uint32_t count)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->State_Count = count;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the state-text in
 *  a C string.
 * @param  object_instance - object-instance number of the object
 * @param  state_index - state index number 1..N of the text requested
 * @return  C string retrieved
 */
const char *
Multistate_Input_State_Text(uint32_t object_instance, uint32_t state_index)
{
    const char *pName = NULL; /* return value */
    const struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        if (state_index > 0) {
            pName = state_name_by_index(pObject, state_index);
        }
    }

    return pName;
}

/**
 * @brief For a given object instance-number, sets the state-text from
 * a C string.
 *
 * @param  object_instance - object-instance number of the object
 * @param  state_index - state index
 * @param  state_text - state text
 * @return true if the state text was set
 */
bool Multistate_Input_State_Text_Set(
    uint32_t object_instance,
    uint32_t state_index,
    BACNET_CHARACTER_STRING *char_string)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject && state_index > 0 && char_string) {
        if (state_index > pObject->State_Count) pObject->State_Count = state_index;
        state_index--;
        pObject->State_Text[state_index] = strdup(char_string->value);
        status = true;
    }

    return status;
}

#if 0
/**
 * @brief Encode a BACnetARRAY property element
 * @param object_instance [in] BACnet network port object instance number
 * @param index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Multistate_Input_State_Text_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX index, uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    const char *pName = NULL; /* return value */
    BACNET_CHARACTER_STRING char_string = { 0 };
    uint32_t state_index = 1;

    state_index += index;
    pName = Multistate_Input_State_Text(object_instance, state_index);
    if (pName) {
        characterstring_init_ansi(&char_string, pName);
        apdu_len = encode_application_character_string(apdu, &char_string);
    }

    return apdu_len;
}
#endif

/**
 * @brief For a given object instance-number, determines the present-value
 * @param  object_instance - object-instance number of the object
 * @return  present-value of the object
 */
static uint32_t Object_Present_Value(struct object_data *pObject)
{
    uint32_t value = 1;
    uint8_t priority = 0; /* loop counter */

    if (pObject) {
        value = pObject->Relinquish_Default;
        for (priority = 0; priority < BACNET_MAX_PRIORITY; priority++) {
            if (!pObject->Relinquished[priority]) {
                value = pObject->Priority_Array[priority];
                break;
            }
        }
    }

    return value;
}

/**
 * @brief For a given object instance-number, determines the present-value
 * @param  object_instance - object-instance number of the object
 * @return  present-value 1..N of the object
 */
uint32_t Multistate_Input_Present_Value(uint32_t object_instance)
{
    uint32_t value = 1;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = Object_Present_Value(pObject);
    }

    return value;
}

/**
 * @brief For a given object instance-number, checks the present-value for COV
 * @param  pObject - specific object with valid data
 * @param  value - multistate value
 */
static void Multistate_Input_Present_Value_COV_Detect(
    struct object_data *pObject, uint32_t value)
{
    uint32_t prior_value = 1;

    if (pObject) {
        prior_value = pObject->Prior_Value;
        if (prior_value != value) {
            pObject->Changed = true;
            pObject->Prior_Value = value;
        }
    }
}

/**
 * @brief For a given object instance-number, sets the present-value
 * @param  object_instance - object-instance number of the object
 * @param  value - integer multi-state value 1..N
 * @param  priority - priority-array index value 1..16
 * @return  true if values are within range and present-value is set.
 */
bool Multistate_Input_Present_Value_Set(
    uint32_t object_instance, uint32_t value, uint8_t priority)
{
    bool status = false;
    struct object_data *pObject;
    unsigned max_states = 0;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        max_states = pObject->State_Count;
        if ((value >= 1) && (value <= max_states) &&
            (priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = false;
            pObject->Priority_Array[priority - 1] = value;
            Multistate_Input_Present_Value_COV_Detect(
                pObject, Multistate_Input_Present_Value(object_instance));
            status = true;
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, relinquishes the present-value
 * @param  object_instance - object-instance number of the object
 * @param  priority - priority-array index value 1..16
 * @return  true if values are within range and present-value is relinquished.
 */
bool Multistate_Input_Present_Value_Relinquish(
    uint32_t object_instance, uint8_t priority)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = true;
            pObject->Priority_Array[priority - 1] = 0;
            Multistate_Input_Present_Value_COV_Detect(
                pObject, Multistate_Input_Present_Value(object_instance));
            status = true;
        }
    }

    return status;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point analog value
 * @param  priority - priority-array index value 1..16
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 *
 * @return  true if values are within range and present-value is set.
 */
static bool Multistate_Input_Present_Value_Write(
    uint32_t object_instance,
    uint32_t value,
    uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;
    struct object_data *pObject;
    uint32_t old_value = 0;
    uint32_t new_value = 0;
    unsigned max_states = 0;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        max_states = pObject->State_Count;
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY) &&
            (value >= 1) && (value <= max_states)) {
            if (priority != 6) {
                old_value = Object_Present_Value(pObject);
                Multistate_Input_Present_Value_Set(object_instance, value,
                    priority);
                if (pObject->Out_Of_Service) {
                    /* The physical point that the object represents
                        is not in service. This means that changes to the
                        Present_Value property are decoupled from the
                        physical point when the value of Out_Of_Service
                        is true. */
                } else if (Multistate_Input_Write_Present_Value_Callback) {
                    new_value = Object_Present_Value(pObject);
                    Multistate_Input_Write_Present_Value_Callback(
                        object_instance, old_value, new_value);
                }
                status = true;
            } else {
                *error_class = ERROR_CLASS_PROPERTY;
                *error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            }
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        }
    } else {
        *error_class = ERROR_CLASS_OBJECT;
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
    }

    return status;
}

/**
 * @brief For a given object instance-number, writes the present-value to the
 *  remote node
 * @param  object_instance - object-instance number of the object
 * @param  priority - priority-array index value 1..16
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 * @return  true if values are within range and write is requested
 */
static bool Multistate_Input_Present_Value_Relinquish_Write(
    uint32_t object_instance, uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;
    struct object_data *pObject;
    uint32_t old_value = 0;
    uint32_t new_value = 0;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            if (priority != 6) {
                old_value = Object_Present_Value(pObject);
                Multistate_Input_Present_Value_Relinquish(object_instance,
                    priority);
                if (pObject->Out_Of_Service) {
                    /* The physical point that the object represents
                        is not in service. This means that changes to the
                        Present_Value property are decoupled from the
                        physical input when the value of Out_Of_Service
                        is true. */
                } else if (Multistate_Input_Write_Present_Value_Callback) {
                    new_value = Object_Present_Value(pObject);
                    Multistate_Input_Write_Present_Value_Callback(
                        object_instance, old_value, new_value);
                }
                status = true;
            } else {
                *error_class = ERROR_CLASS_PROPERTY;
                *error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            }
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        }
    } else {
        *error_class = ERROR_CLASS_OBJECT;
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
    }

    return status;
}

/**
 * @brief For a given object instance-number, determines the
 *  out-of-service state
 * @param  object_instance - object-instance number of the object
 * @return  out-of-service state of the object
 */
bool Multistate_Input_Out_Of_Service(uint32_t object_instance)
{
    bool value = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = pObject->Out_Of_Service;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the out-of-service state
 * @param  object_instance - object-instance number of the object
 * @param  value - out-of-service state
 */
void Multistate_Input_Out_Of_Service_Set(uint32_t object_instance, bool value)
{
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        if (pObject->Out_Of_Service != value) {
            pObject->Out_Of_Service = value;
            pObject->Changed = true;
        }
    }

    return;
}

/**
 * For a given object instance-number, sets the out-of-service state
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - out-of-service state
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 *
 * @return  true if value is set, false if error occurred
 */
static bool Multistate_Input_Out_Of_Service_Write(
    uint32_t object_instance,
    bool value,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        if (pObject->Write_Enabled) {
            Multistate_Input_Out_Of_Service_Set(object_instance, value);
            status = true;
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
        }
    } else {
        *error_class = ERROR_CLASS_OBJECT;
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
    }

    return status;
}

/**
 * @brief For a given object instance-number, loads the object-name into
 *  a characterstring. Note that the object name must be unique
 *  within this device.
 * @param  object_instance - object-instance number of the object
 * @param  object_name - holds the object-name retrieved
 *
 * @return  true if object-name was retrieved
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
 * @brief For a given object instance-number, sets the object-name
 *  Note that the object name must be unique within this device.
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the object-name to be set
 * @return  true if object-name was set
 */
bool Multistate_Input_Name_Set(uint32_t object_instance, const char *new_name)
{
    bool status = false; /* return value */
    BACNET_CHARACTER_STRING object_name;
    BACNET_OBJECT_TYPE found_type = OBJECT_NONE;
    uint32_t found_instance = 0;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
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
 * @brief Return the object name C string
 * @param object_instance [in] BACnet object instance number
 * @return object name or NULL if not found
 */
const char *Multistate_Input_Name_ASCII(uint32_t object_instance)
{
    const char *name = NULL;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        name = pObject->Object_Name;
    }

    return name;
}

/**
 * @brief For a given object instance-number, gets the reliability.
 * @param  object_instance - object-instance number of the object
 * @return reliability value
 */
BACNET_RELIABILITY Multistate_Input_Reliability(uint32_t object_instance)
{
    BACNET_RELIABILITY reliability = RELIABILITY_NO_FAULT_DETECTED;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        reliability = (BACNET_RELIABILITY)pObject->Reliability;
    }

    return reliability;
}

/**
 * @brief For a given object instance-number, gets the Fault status flag
 * @param  object_instance - object-instance number of the object
 * @return  true the status flag is in Fault
 */
static bool Multistate_Input_Object_Fault(const struct object_data *pObject)
{
    bool fault = false;

    if (pObject) {
        if (pObject->Reliability != RELIABILITY_NO_FAULT_DETECTED) {
            fault = true;
        }
    }

    return fault;
}

/**
 * @brief For a given object instance-number, sets the reliability
 * @param  object_instance - object-instance number of the object
 * @param  value - reliability enumerated value
 * @return  true if values are within range and property is set.
 */
bool Multistate_Input_Reliability_Set(
    uint32_t object_instance, BACNET_RELIABILITY value)
{
    struct object_data *pObject;
    bool status = false;
    bool fault = false;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        if (value <= RELIABILITY_PROPRIETARY_MAX) {
            fault = Multistate_Input_Object_Fault(pObject);
            pObject->Reliability = value;
            if (fault != Multistate_Input_Object_Fault(pObject)) {
                pObject->Changed = true;
            }
            status = true;
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, gets the Fault status flag
 * @param  object_instance - object-instance number of the object
 * @return  true the status flag is in Fault
 */
static bool Multistate_Input_Fault(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);

    return Multistate_Input_Object_Fault(pObject);
}

/**
 * @brief For a given object instance-number, returns the description
 * @param  object_instance - object-instance number of the object
 * @return description text or NULL if not found
 */
const char *Multistate_Input_Description(uint32_t object_instance)
{
    const char *name = NULL;
    const struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        name = pObject->Description;
    }

    return name;
}

/**
 * @brief For a given object instance-number, sets the description
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the description to be set
 * @return  true if object-name was set
 */
bool Multistate_Input_Description_Set(
    uint32_t object_instance, const char *new_name)
{
    bool status = false; /* return value */
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        status = true;
        pObject->Description = new_name;
    }

    return status;
}

/**
 * @brief Get the COV change flag status
 * @param object_instance - object-instance number of the object
 * @return the COV change flag status
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
        fault = Multistate_Input_Object_Fault(pObject);
        present_value = Object_Present_Value(pObject);
        status =
            cov_value_list_encode_unsigned(value_list, present_value,
                in_alarm, fault, overridden, pObject->Out_Of_Service);
    }
    return status;
}


/**
 * @brief For a given object instance-number, determines the active priority
 * @param  object_instance - object-instance number of the object
 * @return  active priority 1..16, or 0 if no priority is active
 */
unsigned Multistate_Input_Present_Value_Priority(
    uint32_t object_instance)
{
    unsigned p = 0; /* loop counter */
    uint8_t priority = 0; /* return value */
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        for (p = 0; p < BACNET_MAX_PRIORITY; p++) {
            if (!pObject->Relinquished[p]) {
                priority = p + 1;
                break;
            }
        }
    }

    return priority;
}

/**
 * @brief Encode a BACnetARRAY property element
 * @param object_instance [in] BACnet network port object instance number
 * @param priority [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Multistate_Input_Priority_Array_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX priority, uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    uint32_t value = 1;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && (priority < BACNET_MAX_PRIORITY)) {
        if (pObject->Relinquished[priority]) {
            apdu_len = encode_application_null(apdu);
        } else {
            value = pObject->Priority_Array[priority];
            apdu_len = encode_application_enumerated(apdu, value);
        }
    }

    return apdu_len;
}

/**
 * @brief For a given object instance-number, determines the
 *  relinquish-default value
 * @param object_instance - object-instance number
 * @return relinquish-default value of the object
 */
uint32_t Multistate_Input_Relinquish_Default(uint32_t object_instance)
{
    uint32_t value = 1;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = pObject->Relinquish_Default;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the relinquish-default value
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point analog input relinquish-default value
 * @return  true if values are within range and relinquish-default value is set.
 */
bool Multistate_Input_Relinquish_Default_Set(uint32_t object_instance,
    uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Relinquish_Default = value;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the overridden
 * status flag value
 * @param  object_instance - object-instance number of the object
 * @return  out-of-service property value
 */
bool Multistate_Input_Overridden(uint32_t object_instance)
{
    bool value = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = pObject->Overridden;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the overridden status flag
 * @param object_instance - object-instance number of the object
 * @param value - boolean out-of-service value
 * @return true if the overridden status flag was set
 */
void Multistate_Input_Overridden_Set(uint32_t object_instance, bool value)
{
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        if (pObject->Overridden != value) {
            pObject->Overridden = value;
            pObject->Changed = true;
        }
    }
}

/**
 * For a given object instance-number, gets the event-state property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  event-state property value
 */
unsigned Multistate_Input_Event_State(uint32_t object_instance)
{
    unsigned state = EVENT_STATE_NORMAL;
#if defined(INTRINSIC_REPORTING)
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        state = pObject->Event_State;
    }
#endif

    return state;
}

#if defined(INTRINSIC_REPORTING)
/**
 * For a given object instance-number, returns the units property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  time delay property value
 */
uint32_t Multistate_Input_Time_Delay(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = pObject->Time_Delay;
    }

    return value;
}

/**
 * For a given object instance-number, sets the units property value
 *
 * @param object_instance - object-instance number of the object
 * @param value - Time Delay property value
 *
 * @return true if the Time Delay property value was set
 */
bool Multistate_Input_Time_Delay_Set(uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Time_Delay = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, returns the Notification Class
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  Notification Class property value
 */
uint32_t Multistate_Input_Notification_Class(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = pObject->Notification_Class;
    }

    return value;
}

/**
 * For a given object instance-number, sets the Notification Class
 *
 * @param object_instance - object-instance number of the object
 * @param value - Notification Class
 *
 * @return true if the Notification Class value was set
 */
bool Multistate_Input_Notification_Class_Set(uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Notification_Class = value;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the High Limit
 * @param  object_instance - object-instance number of the object
 * @return value or 100.0 if not found
 */
bool Multistate_Input_Alarm_Value(uint32_t object_instance,
    uint32_t state)
{
    bool value = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject && pObject->Alarm_State[state-1]) {
        value = true;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the High Limit
 * @param  object_instance - object-instance number of the object
 * @param  value - value to be set
 * @return true if valid object-instance and value within range
 */
bool Multistate_Input_Alarm_Value_Set(uint32_t object_instance, 
uint32_t state, bool value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Alarm_State[state-1] = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, returns the Event Enable value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  Event Enable value
 */
uint8_t Multistate_Input_Event_Enable(uint32_t object_instance)
{
    uint8_t value = 0;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = pObject->Event_Enable;
    }

    return value;
}

/**
 * For a given object instance-number, sets the Event Enable value
 *
 * @param object_instance - object-instance number of the object
 * @param value - Event Enable value
 *
 * @return true if the Event Enable value was set
 */
bool Multistate_Input_Event_Enable_Set(uint32_t object_instance, uint8_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Event_Enable = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, returns the Acked Transitions
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - acked_info struct
 *
 * @return true
 */
bool Multistate_Input_Acked_Transitions(uint32_t object_instance, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION])
{
    struct object_data *pObject;
    uint8_t b = 0;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        for (b = 0; b < MAX_BACNET_EVENT_TRANSITION; b++) {
            value[b] = &pObject->Acked_Transitions[b];
        }
        return true;
    } else
        return false;
}

/**
 * For a given object instance-number, returns the Notify Type
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  Notify Type value
 */
uint8_t Multistate_Input_Notify_Type(uint32_t object_instance)
{
    uint8_t value = 0;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = pObject->Notify_Type;
    }

    return value;
}

/**
 * For a given object instance-number, sets the Notify_Type value
 *
 * @param object_instance - object-instance number of the object
 * @param value - Notify Type value
 *
 * @return true if the Notify Type value was set
 */
bool Multistate_Input_Notify_Type_Set(uint32_t object_instance, uint8_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Notify_Type = value;
        status = true;
    }

    return status;
}


/**
 * For a given object instance-number, returns the Acked Transitions
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - acked_info struct
 *
 * @return true
 */
bool Multistate_Input_Event_Time_Stamps(uint32_t object_instance, BACNET_DATE_TIME *value[MAX_BACNET_EVENT_TRANSITION])
{
    struct object_data *pObject;
    uint8_t b = 0;
    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        for (b = 0; b < MAX_BACNET_EVENT_TRANSITION; b++) {
            value[b] = &pObject->Event_Time_Stamps[b];
        }
        return true;
    } else
        return false;
}

#endif

/**
 * @brief ReadProperty handler for this object.  For the given ReadProperty
 *  data, the application_data is loaded or the error flags are set.
 * @param  rpdata - BACNET_READ_PROPERTY_DATA data, including
 *  requested data and space for the reply, or error response.
 * @return number of APDU bytes in the response, or
 *  BACNET_STATUS_ERROR on error.
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
    BACNET_DATE_TIME *timestamp[MAX_BACNET_EVENT_TRANSITION] = { 0 };
#endif

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
    apdu_size = rpdata->application_data_len;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], Object_Type, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Multistate_Input_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(&apdu[0], Object_Type);
            break;
        case PROP_PRESENT_VALUE:
            present_value =
                Multistate_Input_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], present_value);
            break;
        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
            state = Multistate_Input_Fault(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, state);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            state = Multistate_Input_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_RELIABILITY:
            apdu_len = encode_application_enumerated(
                &apdu[0],
                Multistate_Input_Reliability(rpdata->object_instance));
            break;
        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0], 
                    Multistate_Input_Event_State(rpdata->object_instance));
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;
        case PROP_OUT_OF_SERVICE:
            state = Multistate_Input_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_NUMBER_OF_STATES:
            apdu_len = encode_application_unsigned(
                &apdu[apdu_len],
                Multistate_Input_Max_States(rpdata->object_instance));
            break;
        case PROP_STATE_TEXT:
            max_states = Multistate_Input_Max_States(rpdata->object_instance);
            if (rpdata->array_index == 0) {
                /* Array element zero is the number of elements in the array */
                apdu_len = encode_application_unsigned(&apdu[0], max_states);
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                /* if no index was specified, then try to encode the entire list
                 */
                /* into one packet. */
                for (i = 1; i <= max_states; i++) {
                    characterstring_init_ansi(&char_string,
                        Multistate_Input_State_Text(
                            rpdata->object_instance, i));
                    /* FIXME: this might go beyond MAX_APDU length! */
                    len = encode_application_character_string(
                        &apdu[apdu_len], &char_string);
                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU) {
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
                        Multistate_Input_State_Text(
                            rpdata->object_instance, rpdata->array_index));
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
            apdu_len = bacnet_array_encode(
                rpdata->object_instance, rpdata->array_index,
                Multistate_Input_Priority_Array_Encode, BACNET_MAX_PRIORITY,
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
            present_value =
                Multistate_Input_Relinquish_Default(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], present_value);
            break;
#if (BACNET_PROTOCOL_REVISION >= 17)
        case PROP_CURRENT_COMMAND_PRIORITY:
            i = Multistate_Input_Present_Value_Priority(
                rpdata->object_instance);
            if ((i >= BACNET_MIN_PRIORITY) && (i <= BACNET_MAX_PRIORITY)) {
                apdu_len = encode_application_unsigned(&apdu[0], i);
            } else {
                apdu_len = encode_application_null(&apdu[0]);
            }
            break;
#endif
#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            i = Multistate_Input_Time_Delay(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], i);
            break;

        case PROP_NOTIFICATION_CLASS:
            i = Multistate_Input_Notification_Class(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], i);
            break;

        case PROP_ALARM_VALUES:
            max_states = Multistate_Input_Max_States(rpdata->object_instance);
            if (rpdata->array_index == 0) {
                /* Array element zero is the number of elements in the array */
                apdu_len = encode_application_unsigned(&apdu[0], max_states);
            } else {
                /* if no index was specified, then try to encode the entire list
                 */
                /* into one packet. */
                for (i = 1; i <= max_states; i++) {
                    if (Multistate_Input_Alarm_Value(rpdata->object_instance, i)) {
                        len = encode_application_unsigned(
                            &apdu[apdu_len], i);
                        /* add it if we have room */
                        if ((apdu_len + len) < MAX_APDU) {
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
            i = Multistate_Input_Event_Enable(rpdata->object_instance);
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

        case PROP_ACKED_TRANSITIONS:
            state = Multistate_Input_Acked_Transitions(rpdata->object_instance, ack_info);
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
            i = Multistate_Input_Notify_Type(rpdata->object_instance);
            apdu_len = encode_application_enumerated(
                &apdu[0], i ? NOTIFY_EVENT : NOTIFY_ALARM);
            break;

        case PROP_EVENT_TIME_STAMPS:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0)
                apdu_len = encode_application_unsigned(
                    &apdu[0], MAX_BACNET_EVENT_TRANSITION);
            /* if no index was specified, then try to encode the entire list */
            /* into one packet. */
            else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                state = Multistate_Input_Event_Time_Stamps(rpdata->object_instance, timestamp);
                for (i = 0; i < MAX_BACNET_EVENT_TRANSITION; i++) {
                    len = encode_opening_tag(
                        &apdu[apdu_len], TIME_STAMP_DATETIME);
                    len += encode_application_date(&apdu[apdu_len + len],
                        &timestamp[i]->date);
                    len += encode_application_time(&apdu[apdu_len + len],
                        &timestamp[i]->time);
                    len += encode_closing_tag(
                        &apdu[apdu_len + len], TIME_STAMP_DATETIME);

                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU)
                        apdu_len += len;
                    else {
                        rpdata->error_code =
                            ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
                        apdu_len = BACNET_STATUS_ABORT;
                        break;
                    }
                }
            } else if (rpdata->array_index <= MAX_BACNET_EVENT_TRANSITION) {
                state = Multistate_Input_Event_Time_Stamps(rpdata->object_instance, timestamp);
                apdu_len =
                    encode_opening_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
                apdu_len += encode_application_date(&apdu[apdu_len],
                    &timestamp[rpdata->array_index-1]->date);
                apdu_len += encode_application_time(&apdu[apdu_len],
                    &timestamp[rpdata->array_index-1]->time);
                apdu_len +=
                    encode_closing_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
            } else {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                apdu_len = BACNET_STATUS_ERROR;
            }
            break;
#endif
        case PROP_DESCRIPTION:
            characterstring_init_ansi(
                &char_string,
                Multistate_Input_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->object_property != PROP_STATE_TEXT) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/**
 * @brief WriteProperty handler for this object.  For the given WriteProperty
 *  data, the application_data is loaded or the error flags are set.
 * @param  wp_data - BACNET_WRITE_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 * @return false if an error is loaded, true if no errors
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
    if ((wp_data->object_property != PROP_STATE_TEXT) &&
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
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                status = false;
                if (value.type.Unsigned_Int <= UINT32_MAX) {
                    if (Multistate_Input_Present_Value_Write(
                        wp_data->object_instance,
                        value.type.Unsigned_Int, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code)) {
                        value_i = Multistate_Input_Present_Value(wp_data->object_instance);
                        ucix_add_option_int(ctxw, sec, idx_c, "value", value_i);
                        ucix_commit(ctxw,sec);
                        free(value_c);
                    };
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_NULL);
                if (status) {
                    if (Multistate_Input_Present_Value_Relinquish_Write(
                        wp_data->object_instance, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code)) {
                        value_i = Multistate_Input_Present_Value(wp_data->object_instance);
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
                status = Multistate_Input_Out_Of_Service_Write(
                    wp_data->object_instance, value.type.Boolean,
                    &wp_data->error_class, &wp_data->error_code);
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
                if (Multistate_Input_Name_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
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
                        status = Multistate_Input_State_Text_Set(
                            wp_data->object_instance, idx,
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
                if (idx != Multistate_Input_Max_States(wp_data->object_instance)) {
                    Multistate_Input_Max_States_Set(wp_data->object_instance, idx);
                }
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_CHARACTER_STRING);
                if (status) {
                    status = Multistate_Input_State_Text_Set(
                        wp_data->object_instance, wp_data->array_index,
                        &value.type.Character_String);
                }
            }
            if (status) {
                stats_n = Multistate_Input_Max_States(wp_data->object_instance);
                for (k = 0 ; k < stats_n; k++) {
                    pName = Multistate_Input_State_Text(wp_data->object_instance, k+1);
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
                Multistate_Input_Max_States_Set(wp_data->object_instance,
                    value.type.Enumerated);
            break;
        case PROP_RELIABILITY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status)
                Multistate_Input_Reliability_Set(wp_data->object_instance,
                    value.type.Enumerated);
            break;
        case PROP_PRIORITY_ARRAY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_RELINQUISH_DEFAULT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BOOLEAN);
            if (status)
                Multistate_Input_Relinquish_Default_Set(wp_data->object_instance,
                    value.type.Boolean);
            break;
        case PROP_DESCRIPTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Multistate_Input_Description_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "description",
                        Multistate_Input_Description(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
#if (BACNET_PROTOCOL_REVISION >= 17)
        case PROP_CURRENT_COMMAND_PRIORITY:
#endif
#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                if (Multistate_Input_Time_Delay_Set(
                    wp_data->object_instance, value.type.Unsigned_Int)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "time_delay",
                        Multistate_Input_Time_Delay(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_NOTIFICATION_CLASS:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                if (Multistate_Input_Notification_Class_Set(
                    wp_data->object_instance, value.type.Unsigned_Int)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "nc",
                        Multistate_Input_Notification_Class(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
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
                    Multistate_Input_Alarm_Value_Set(
                        wp_data->object_instance,
                        idx, false);
                }
                for (idx = 0; idx < 255; idx++ ) {
                    status = write_property_type_valid(wp_data, &value,
                        BACNET_APPLICATION_TAG_UNSIGNED_INT);
                    if (!status) {
                        break;
                    }
                    if (element_len) {
                        status = Multistate_Input_Alarm_Value_Set(
                            wp_data->object_instance,
                            value.type.Unsigned_Int, true);
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
                    status = Multistate_Input_Alarm_Value_Set(
                        wp_data->object_instance, wp_data->array_index,
                        value.type.Unsigned_Int);
                }
            }
            if (status) {
                stats_n = Multistate_Input_Max_States(wp_data->object_instance);
                k = 0;
                for (idx = 1 ; idx <= stats_n; idx++) {
                    if (Multistate_Input_Alarm_Value(wp_data->object_instance, idx)) {
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
                if (Multistate_Input_Event_Enable_Set(
                    wp_data->object_instance, value.type.Bit_String.value[0])) {
                    ucix_add_option_int(ctxw, sec, idx_c, "event",
                        Multistate_Input_Event_Enable(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_NOTIFY_TYPE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                if (Multistate_Input_Notify_Type_Set(
                    wp_data->object_instance, value.type.Enumerated)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "notify_type",
                        Multistate_Input_Notify_Type(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                    }
            }
            break;
        case PROP_ACKED_TRANSITIONS:
        case PROP_EVENT_TIME_STAMPS:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
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
 * @brief Determines a object write-enabled flag state
 * @param object_instance - object-instance number of the object
 * @return  write-enabled status flag
 */
bool Multistate_Input_Write_Enabled(uint32_t object_instance)
{
    bool value = false;
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        value = pObject->Write_Enabled;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the write-enabled flag
 * @param object_instance - object-instance number of the object
 */
void Multistate_Input_Write_Enable(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Write_Enabled = true;
    }
}

/**
 * @brief For a given object instance-number, clears the write-enabled flag
 * @param object_instance - object-instance number of the object
 */
void Multistate_Input_Write_Disable(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Multistate_Input_Object(object_instance);
    if (pObject) {
        pObject->Write_Enabled = false;
    }
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
            pObject->Write_Enabled = false;
            /* add to list */
            index = Keylist_Data_Add(Object_List, object_instance, pObject);
            if (index < 0) {
                free(pObject);
                return BACNET_MAX_INSTANCE;
            }
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
    stats_n = ucix_get_list(stats, ictx->ctx, ictx->section, sec_idx,
        "alarm");
    if (stats_n) {
        for (k = 0 ; k < stats_n; k++) {
            l = atoi(stats[k]);
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
 */
void Multistate_Input_Init(void)
{
    struct uci_context *ctx;
    struct object_data_t tObject;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
    char *stats[254];
    uint32_t stats_n = 0;
    uint32_t k = 0;
    uint32_t l = 0;

    struct object_data *pObject = NULL;
    struct itr_ctx itr_m;
    if (!Object_List) {
        Object_List = Keylist_Create();
    }
    ctx = ucix_init(sec);
    if (!ctx)
        fprintf(stderr, "Failed to load config file %s\n",sec);
    /* add to list */
    Keylist_Data_Add(Object_List, BACNET_MAX_INSTANCE, pObject);

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
    stats_n = ucix_get_list(stats, ctx, sec, "default",
        "alarm");
    if (stats_n) {
        for (k = 0 ; k < stats_n; k++) {
            l = atoi(stats[k]);
            tObject.Alarm_State[l] = true;
        }
    }
#endif
    itr_m.section = sec;
    itr_m.ctx = ctx;
    itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void (*)(const char *, void *))uci_list, &itr_m);
    ucix_cleanup(ctx);
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

void Multistate_Input_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    struct object_data *pObject;
    BACNET_EVENT_NOTIFICATION_DATA event_data;
    BACNET_CHARACTER_STRING msgText;
    uint8_t FromState = 0;
    uint8_t ToState;
    uint32_t PresentVal = 1;
    bool SendNotify = false;

    pObject = Keylist_Data(Object_List, object_instance);

    if (!pObject)
        return;

    if (pObject->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        pObject->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = pObject->Ack_notify_data.EventState;

#if PRINT_ENABLED
        fprintf(stderr, "Send Acknotification for (%s,%d).\n",
            bactext_object_type_name(Object_Type), object_instance);
#endif /* PRINT_ENABLED */

        characterstring_init_ansi(&msgText, "AckNotification");

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        PresentVal = Multistate_Input_Present_Value(object_instance);
        FromState = pObject->Event_State;
        switch (pObject->Event_State) {
            case EVENT_STATE_NORMAL:
                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the High_Limit for a minimum
                   period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-OFFNORMAL flag must be set in the Event_Enable property. */
                if (pObject->Alarm_State[PresentVal] &&
                    ((pObject->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                        EVENT_ENABLE_TO_OFFNORMAL)) {
                    if (!pObject->Remaining_Time_Delay)
                        pObject->Event_State = EVENT_STATE_OFFNORMAL;
                    else
                        pObject->Remaining_Time_Delay--;
                    break;
                }

                /* value of the object is still in the same event state */
                pObject->Remaining_Time_Delay = pObject->Time_Delay;
                break;

            case EVENT_STATE_OFFNORMAL:
                /* Once exceeded, the Present_Value must fall below the High_Limit minus
                   the Deadband before a TO-NORMAL event is generated under these conditions:
                   (a) the Present_Value must fall below the High_Limit minus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-NORMAL flag must be set in the Event_Enable property. */
                if (!pObject->Alarm_State[PresentVal]
                    && ((pObject->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                        EVENT_ENABLE_TO_NORMAL)) {
                    if (!pObject->Remaining_Time_Delay)
                        pObject->Event_State = EVENT_STATE_NORMAL;
                    else
                        pObject->Remaining_Time_Delay--;
                    break;
                }
                /* value of the object is still in the same event state */
                pObject->Remaining_Time_Delay = pObject->Time_Delay;
                break;

            default:
                return; /* shouldn't happen */
        }       /* switch (FromState) */

        ToState = pObject->Event_State;

        if (FromState != ToState) {
            /* Event_State has changed.
               Need to fill only the basic parameters of this type of event.
               Other parameters will be filled in common function. */

            switch (ToState) {
                case EVENT_STATE_OFFNORMAL:
                    characterstring_init_ansi(&msgText, "Goes to Alarm Value");
                    break;

                case EVENT_STATE_FAULT:
                    characterstring_init_ansi(&msgText, "Goes to Feedback fault");
                    break;

                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_OFFNORMAL) {
                        characterstring_init_ansi(&msgText,
                            "Back to normal state from Alarm Value");
                    } else {
                        characterstring_init_ansi(&msgText,
                            "Back to normal state from Feedback Fault");
                    }
                    break;

                default:
                    break;
            }   /* switch (ToState) */

#if PRINT_ENABLED
            fprintf(stderr, "Event_State for (%s,%d) goes from %s to %s.\n",
                bactext_object_type_name(Object_Type), object_instance,
                bactext_event_state_name(FromState),
                bactext_event_state_name(ToState));
#endif /* PRINT_ENABLED */

            /* Notify Type */
            event_data.notifyType = pObject->Notify_Type;

            /* Send EventNotification. */
            SendNotify = true;
        }
    }


    if (SendNotify) {
        /* Event Object Identifier */
        event_data.eventObjectIdentifier.type = Object_Type;
        event_data.eventObjectIdentifier.instance = object_instance;

        /* Time Stamp */
        event_data.timeStamp.tag = TIME_STAMP_DATETIME;
        Device_getCurrentDateTime(&event_data.timeStamp.value.dateTime);

        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* fill Event_Time_Stamps */
            switch (ToState) {
                case EVENT_STATE_OFFNORMAL:
                    pObject->Event_Time_Stamps[TRANSITION_TO_OFFNORMAL] =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    pObject->Event_Time_Stamps[TRANSITION_TO_FAULT] =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    pObject->Event_Time_Stamps[TRANSITION_TO_NORMAL] =
                        event_data.timeStamp.value.dateTime;
                    break;
                default:
                    break;
            }
        }

        /* Notification Class */
        event_data.notificationClass = pObject->Notification_Class;

        /* Event Type */
        event_data.eventType = EVENT_OUT_OF_RANGE;

        /* Message Text */
        event_data.messageText = &msgText;

        /* Notify Type */
        /* filled before */

        /* From State */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION)
            event_data.fromState = FromState;

        /* To State */
        event_data.toState = pObject->Event_State;

        /* Event Values */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* Status_Flags of the referenced object. */
            bitstring_init(&event_data.notificationParams.outOfRange.
                statusFlags);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.
                statusFlags, STATUS_FLAG_IN_ALARM,
                pObject->Event_State ? true : false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.
                statusFlags, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.
                statusFlags, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&event_data.notificationParams.outOfRange.
                statusFlags, STATUS_FLAG_OUT_OF_SERVICE,
                pObject->Out_Of_Service);
        }

        /* add data from notification class */
        Notification_Class_common_reporting_function(&event_data);

        /* Ack required */
        if ((event_data.notifyType != NOTIFY_ACK_NOTIFICATION) &&
            (event_data.ackRequired == true)) {
            switch (event_data.toState) {
                case EVENT_STATE_OFFNORMAL:
                    pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                        bIsAcked = false;
                    pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                        Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    pObject->Acked_Transitions[TRANSITION_TO_FAULT].
                        bIsAcked = false;
                    pObject->Acked_Transitions[TRANSITION_TO_FAULT].
                        Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    pObject->Acked_Transitions[TRANSITION_TO_NORMAL].
                        bIsAcked = false;
                    pObject->Acked_Transitions[TRANSITION_TO_NORMAL].
                        Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;
                case EVENT_STATE_LOW_LIMIT:
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_MAX:
                    break;
                default:
                    break;
            }
        }
    }
#endif /* defined(INTRINSIC_REPORTING) */
}

#if defined(INTRINSIC_REPORTING)
int Multistate_Input_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    struct object_data *pObject;
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;

    pObject = Keylist_Data(Object_List, Multistate_Input_Index_To_Instance(index));

    /* check index */
    if (pObject) {
        /* Event_State not equal to NORMAL */
        IsActiveEvent = (pObject->Event_State != EVENT_STATE_NORMAL);

        /* Acked_Transitions property, which has at least one of the bits
           (TO-OFFNORMAL, TO-FAULT, TONORMAL) set to FALSE. */
        IsNotAckedTransitions =
            (pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
            bIsAcked ==
            false) | (pObject->Acked_Transitions[TRANSITION_TO_FAULT].
            bIsAcked ==
            false) | (pObject->Acked_Transitions[TRANSITION_TO_NORMAL].
            bIsAcked == false);
    } else
        return -1;      /* end of list  */

    if ((IsActiveEvent) || (IsNotAckedTransitions)) {
        /* Object Identifier */
        getevent_data->objectIdentifier.type = Object_Type;
        getevent_data->objectIdentifier.instance =
            Multistate_Input_Index_To_Instance(index);
        /* Event State */
        getevent_data->eventState = pObject->Event_State;
        /* Acknowledged Transitions */
        bitstring_init(&getevent_data->acknowledgedTransitions);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_OFFNORMAL,
            pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
            bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_FAULT,
            pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_NORMAL,
            pObject->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);
        /* Event Time Stamps */
        for (i = 0; i < 3; i++) {
            getevent_data->eventTimeStamps[i].tag = TIME_STAMP_DATETIME;
            getevent_data->eventTimeStamps[i].value.dateTime =
                pObject->Event_Time_Stamps[i];
        }
        /* Notify Type */
        getevent_data->notifyType = pObject->Notify_Type;
        /* Event Enable */
        bitstring_init(&getevent_data->eventEnable);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_OFFNORMAL,
            (pObject->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true : false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_FAULT,
            (pObject->Event_Enable & EVENT_ENABLE_TO_FAULT) ? true : false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_NORMAL,
            (pObject->Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true : false);
        /* Event Priorities */
        Notification_Class_Get_Priorities(pObject->Notification_Class,
            getevent_data->eventPriorities);

        return 1;       /* active event */
    } else
        return 0;       /* no active event at this index */
}

int Multistate_Input_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code)
{
    struct object_data *pObject;
    pObject = Keylist_Data(Object_List, alarmack_data->eventObjectIdentifier.instance);

    /* check index */
    if (!pObject) {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -1;
    }

    switch (alarmack_data->eventStateAcked) {
        case EVENT_STATE_OFFNORMAL:
            if (pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                bIsAcked == false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&pObject->
                        Acked_Transitions[TRANSITION_TO_OFFNORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                    bIsAcked = true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_FAULT:
            if (pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&pObject->
                        Acked_Transitions[TRANSITION_TO_FAULT].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                    true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_NORMAL:
            if (pObject->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&pObject->
                        Acked_Transitions[TRANSITION_TO_NORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                pObject->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
                    true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        default:
            return -2;
    }

    /* Need to send AckNotification. */
    pObject->Ack_notify_data.bSendAckNotify = true;
    pObject->Ack_notify_data.EventState = alarmack_data->eventStateAcked;

    /* Return OK */
    return 1;
}

int Multistate_Input_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, Multistate_Input_Index_To_Instance(index));

    /* check index */
    if (pObject) {
        /* Event_State is not equal to NORMAL  and
           Notify_Type property value is ALARM */
        if ((pObject->Event_State != EVENT_STATE_NORMAL) &&
            (pObject->Notify_Type == NOTIFY_ALARM)) {
            /* Object Identifier */
            getalarm_data->objectIdentifier.type = Object_Type;
            getalarm_data->objectIdentifier.instance =
                Multistate_Input_Index_To_Instance(index);
            /* Alarm State */
            getalarm_data->alarmState = pObject->Event_State;
            /* Acknowledged Transitions */
            bitstring_init(&getalarm_data->acknowledgedTransitions);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_OFFNORMAL,
                pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].
                bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_FAULT,
                pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_NORMAL,
                pObject->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);

            return 1;   /* active alarm */
        } else
            return 0;   /* no active alarm at this index */
    } else
        return -1;      /* end of list  */
}
#endif /* defined(INTRINSIC_REPORTING) */
