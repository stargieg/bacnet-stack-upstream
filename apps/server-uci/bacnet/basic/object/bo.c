/**
 * @file
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2005
 * @brief A basic BACnet Binary Output object implementation.
 * The Binary Lighting Output object is a commandable object, and the
 * present-value property uses a priority array and an enumerated 2-state
 * data type.
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
#include "bacnet/bacdcode.h"
#include "bacnet/bacerror.h"
#include "bacnet/bacapp.h"
#include "bacnet/bactext.h"
#include "bacnet/cov.h"
#include "bacnet/apdu.h"
#include "bacnet/npdu.h"
#include "bacnet/abort.h"
#include "bacnet/reject.h"
#include "bacnet/rp.h"
#include "bacnet/wp.h"
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
/* me! */
#include "bo.h"

static const char *sec = "bacnet_bo";
static const char *type = "bo";

struct object_data {
    bool Out_Of_Service : 1;
    bool Overridden : 1;
    bool Changed : 1;
    bool Prior_Value : 1;
    bool Relinquish_Default : 1;
    bool Polarity : 1;
    bool Relinquished[BACNET_MAX_PRIORITY];
    bool Priority_Array[BACNET_MAX_PRIORITY];
    uint8_t Reliability;
    const char *Object_Name;
    const char *Active_Text;
    const char *Inactive_Text;
    const char *Description;
    void *Context;
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State:3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    bool Alarm_Value;
    bool Feedback_Value;
    unsigned Event_Enable:3;
    unsigned Event_Detection_Enable : 1;
    unsigned Notify_Type:1;
    ACKED_INFO Acked_Transitions[MAX_BACNET_EVENT_TRANSITION];
    BACNET_DATE_TIME Event_Time_Stamps[MAX_BACNET_EVENT_TRANSITION];
    const char *Event_Message_Texts[MAX_BACNET_EVENT_TRANSITION];
    const char *Event_Message_Texts_Custom[MAX_BACNET_EVENT_TRANSITION];
    /* time to generate event notification */
    uint32_t Remaining_Time_Delay;
    /* AckNotification informations */
    ACK_NOTIFICATION Ack_notify_data;
    BACNET_RELIABILITY Last_ToFault_Event_Reliability;
#endif /* INTRINSIC_REPORTING */
};

struct object_data_t {
    bool Out_Of_Service : 1;
    const char *Prior_Value;
    const char *Relinquish_Default;
    const char *Inactive_Text;
    const char *Active_Text;
    uint8_t Reliability;
    const char *Object_Name;
    const char *Description;
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State:3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    const char *Alarm_Value;
    unsigned Limit_Enable:2;
    unsigned Event_Enable:3;
    unsigned Event_Detection_Enable : 1;
    unsigned Notify_Type:1;
#endif /* INTRINSIC_REPORTING */
};

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_BINARY_OUTPUT;
/* callback for present value writes */
static binary_output_write_present_value_callback
    Binary_Output_Write_Present_Value_Callback;

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
    PROP_POLARITY,
    PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
#if (BACNET_PROTOCOL_REVISION >= 17)
    PROP_CURRENT_COMMAND_PRIORITY,
#endif
    -1
};

static const int32_t Properties_Optional[] = {
    /* unordered list of optional properties */
    PROP_RELIABILITY, PROP_DESCRIPTION, PROP_ACTIVE_TEXT, PROP_INACTIVE_TEXT,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY,        PROP_NOTIFICATION_CLASS,
    PROP_ALARM_VALUE,       PROP_EVENT_ENABLE,
    PROP_ACKED_TRANSITIONS, PROP_NOTIFY_TYPE,
    PROP_EVENT_TIME_STAMPS, PROP_FEEDBACK_VALUE,
    PROP_EVENT_MESSAGE_TEXTS,
#endif
    -1 };

static const int32_t Properties_Proprietary[] = { -1 };

/**
 * Returns the list of required, optional, and proprietary properties.
 * Used by ReadPropertyMultiple service.
 *
 * @param pRequired - pointer to list of int terminated by -1, of
 * BACnet required properties for this object.
 * @param pOptional - pointer to list of int terminated by -1, of
 * BACnet optkional properties for this object.
 * @param pProprietary - pointer to list of int terminated by -1, of
 * BACnet proprietary properties for this object.
 */
void Binary_Output_Property_Lists(
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
 * Determines if a given Binary Output instance is valid
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  true if the instance is valid, and false if not
 */
bool Binary_Output_Valid_Instance(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return true;
    }

    return false;
}

/**
 * Determines the number of Binary Output objects
 *
 * @return  Number of Binary Output objects
 */
unsigned Binary_Output_Count(void)
{
    return Keylist_Count(Object_List)-1;
}

/**
 * Determines the object instance-number for a given 0..N index
 * of Binary Output objects where N is Binary_Output_Count().
 *
 * @param  index - 0..MAX_BINARY_OUTPUTS value
 *
 * @return  object instance-number for the given index
 */
uint32_t Binary_Output_Index_To_Instance(unsigned index)
{
    KEY key = UINT32_MAX;

    Keylist_Index_Key(Object_List, index, &key);

    return key;
}

/**
 * For a given object instance-number, determines a 0..N index
 * of Binary Output objects where N is Binary_Output_Count().
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  index for the given instance-number, or MAX_BINARY_OUTPUTS
 * if not valid.
 */
unsigned Binary_Output_Instance_To_Index(uint32_t object_instance)
{
    return Keylist_Index(Object_List, object_instance);
}

/**
 * @brief Calculated the present-value property from the priority array.
 * @param pObject - pointer to the object data
 * @return The present-value of the object
 */
static BACNET_BINARY_PV Object_Present_Value(struct object_data *pObject)
{
    BACNET_BINARY_PV value = BINARY_INACTIVE;
    uint8_t priority = 0; /* loop counter */

    if (pObject) {
        if (pObject->Relinquish_Default) {
            value = BINARY_ACTIVE;
        }
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
 * For a given object instance-number, determines the present-value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  present-value of the object
 */
BACNET_BINARY_PV Binary_Output_Present_Value(uint32_t object_instance)
{
    BACNET_BINARY_PV value = BINARY_INACTIVE;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = Object_Present_Value(pObject);
    }

    return value;
}

/**
 * For a given object instance-number, gets the event-state property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  event-state property value
 */
static unsigned Binary_Output_Event_State(uint32_t object_instance)
{
    unsigned state = EVENT_STATE_NORMAL;
#if defined(INTRINSIC_REPORTING)
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        state = pObject->Event_State;
    }
#else
    (void)object_instance;
#endif

    return state;
}

/**
 * For a given object instance-number, checks the present-value for COV
 *
 * @param  pObject - specific object with valid data
 * @param  value - floating point analog value
 */
static void Binary_Output_Present_Value_COV_Detect(
    struct object_data *pObject, BACNET_BINARY_PV value)
{
    BACNET_BINARY_PV prior_value = false;

    if (pObject) {
        prior_value = pObject->Prior_Value;
        if (prior_value != value) {
            pObject->Changed = true;
            pObject->Prior_Value = value;
        }
    }
}

#if defined(INTRINSIC_REPORTING)
/**
 * @brief For a given object instance-number, determines the present-value
 * @param  object_instance - object-instance number of the object
 * @return  feedback-value of the object
 */
BACNET_BINARY_PV Binary_Output_Feedback_Value(uint32_t object_instance)
{
    BACNET_BINARY_PV value = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Feedback_Value) {
            value = pObject->Feedback_Value;
        } else {
            value = Binary_Output_Present_Value(object_instance);
        }
    }

    return value;
}

/**
 * For a given object instance-number, returns the units property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  time delay property value
 */
uint32_t Binary_Output_Time_Delay(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
bool Binary_Output_Time_Delay_Set(uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
uint32_t Binary_Output_Notification_Class(uint32_t object_instance)
{
    uint32_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
bool Binary_Output_Notification_Class_Set(uint32_t object_instance, uint32_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
BACNET_BINARY_PV Binary_Output_Alarm_Value(uint32_t object_instance)
{
    BACNET_BINARY_PV value = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Alarm_Value;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the High Limit
 * @param  object_instance - object-instance number of the object
 * @param  value - value to be set
 * @return true if valid object-instance and value within range
 */
bool Binary_Output_Alarm_Value_Set(uint32_t object_instance, BACNET_BINARY_PV value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Alarm_Value = value;
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
uint8_t Binary_Output_Event_Enable(uint32_t object_instance)
{
    uint8_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
bool Binary_Output_Event_Enable_Set(uint32_t object_instance, uint8_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
bool Binary_Output_Acked_Transitions(uint32_t object_instance, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION])
{
    struct object_data *pObject;
    uint8_t b = 0;

    pObject = Keylist_Data(Object_List, object_instance);
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
uint8_t Binary_Output_Notify_Type(uint32_t object_instance)
{
    uint8_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
bool Binary_Output_Notify_Type_Set(uint32_t object_instance, uint8_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Notify_Type = value;
        status = true;
    }

    return status;
}

static void
Binary_Output_Reset_Event_Properties(uint32_t object_instance)
{
    unsigned j;
    struct object_data *pObject;
    /* initialize Event time stamps using wildcards
            and set Acked_transitions */
    pObject = Keylist_Data(Object_List, object_instance);
    for (j = 0; j < MAX_BACNET_EVENT_TRANSITION; j++) {
        datetime_wildcard_set(&pObject->Event_Time_Stamps[j]);
        pObject->Acked_Transitions[j].bIsAcked = true;
        pObject->Event_Message_Texts[j] = NULL;
    }
    pObject->Event_State = EVENT_STATE_NORMAL;
    pObject->Last_ToFault_Event_Reliability = RELIABILITY_NO_FAULT_DETECTED;
}


/**
 * @brief For a given object instance-number and event transition, returns the
 * event message text
 * @param  object_instance - object-instance number of the object
 * @param  transition - transition type
 * @return event message text or NULL if object not found or transition invalid
 */
const char *Binary_Output_Event_Message_Text(
    const uint32_t object_instance,
    const enum BACnetEventTransitionBits transition)
{
    const char *text = NULL;
    const struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && transition < MAX_BACNET_EVENT_TRANSITION) {
        text = pObject->Event_Message_Texts[transition];
        if (!text) {
            text = Notification_Class_Event_Message_Text(
                    pObject->Notification_Class,
                    transition);
        }
        if (!text) {
            text = "";
        }
    }

    return text;
}

/**
 * @brief For a given object instance-number and event transition, sets the
 * custom event message text
 * NOTE: Event_Message_Text will be generated on event if custom_text is null
 * @param  object_instance - object-instance number of the object
 * @param  transition - transition type
 * @param  custom_text - holds the event message text to be set
 * @return  true if custom event message text was set
 */

bool Binary_Output_Event_Message_Text_Custom_Set(
    const uint32_t object_instance,
    const enum BACnetEventTransitionBits transition,
    const char *const custom_text)
{
    bool status = false; /* return value */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && transition < MAX_BACNET_EVENT_TRANSITION) {
        pObject->Event_Message_Texts_Custom[transition] = custom_text;
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
bool Binary_Output_Event_Time_Stamps(uint32_t object_instance, BACNET_DATE_TIME *value[MAX_BACNET_EVENT_TRANSITION])
{
    struct object_data *pObject;
    uint8_t b = 0;
    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        for (b = 0; b < MAX_BACNET_EVENT_TRANSITION; b++) {
            value[b] = &pObject->Event_Time_Stamps[b];
        }
        return true;
    } else
        return false;
}

/**
 * @brief Encode a BACnetARRAY property element
 * @param object_instance [in] object instance number
 * @param index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Binary_Output_Event_Message_Texts_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX index, uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    const char *text = NULL; /* return value */
    BACNET_CHARACTER_STRING char_string = { 0 };

    text = Binary_Output_Event_Message_Text(object_instance, index);
    if (text) {
        characterstring_init_ansi(&char_string, text);
        apdu_len = encode_application_character_string(apdu, &char_string);
    }

    return apdu_len;
}

#endif

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
static int Binary_Output_Priority_Array_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX index, uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    struct object_data *pObject;
    BACNET_BINARY_PV value = BINARY_INACTIVE;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && (index < BACNET_MAX_PRIORITY)) {
        if (!pObject->Relinquished[index]) {
            value = pObject->Priority_Array[index];
            apdu_len = encode_application_enumerated(apdu, value);
        } else {
            apdu_len = encode_application_null(apdu);
        }
    }

    return apdu_len;
}

/**
 * For a given object instance-number, determines the active priority
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  active priority 1..16, or 0 if no priority is active
 */
unsigned Binary_Output_Present_Value_Priority(uint32_t object_instance)
{
    unsigned p = 0; /* loop counter */
    unsigned priority = 0; /* return value */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
 * For a given object instance-number, sets the present-value at a given
 * priority 1..16.
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - enumerated 2-state active or inactive value
 * @param  priority - priority 1..16
 *
 * @return  true if values are within range and present-value is set.
 */
bool Binary_Output_Present_Value_Set(
    uint32_t object_instance, BACNET_BINARY_PV binary_value, unsigned priority)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = false;
            pObject->Priority_Array[priority - 1] = binary_value;
            Binary_Output_Present_Value_COV_Detect(
                pObject, Binary_Output_Present_Value(object_instance));
            status = true;
        }
    }

    return status;
}

/**
 * For a given object instance-number, relinquishes the present-value
 * at a given priority 1..16.
 *
 * @param  object_instance - object-instance number of the object
 * @param  priority - priority 1..16
 *
 * @return  true if values are within range and present-value is set.
 */
bool Binary_Output_Present_Value_Relinquish(
    uint32_t object_instance, unsigned priority)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = true;
            pObject->Priority_Array[priority - 1] = false;
            Binary_Output_Present_Value_COV_Detect(
                pObject, Binary_Output_Present_Value(object_instance));
            status = true;
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, writes the present-value to the
 * remote node
 * @param  object_instance - object-instance number of the object
 * @param  value - present-value
 * @param  priority - priority-array index value 1..16
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 * @return  true if values are within range and present-value is set.
 */
static bool Binary_Output_Present_Value_Write(
    uint32_t object_instance,
    BACNET_BINARY_PV value,
    uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;
    struct object_data *pObject;
    BACNET_BINARY_PV old_value = BINARY_INACTIVE;
    BACNET_BINARY_PV new_value = BINARY_INACTIVE;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY) &&
            (value <= MAX_BINARY_PV)) {
            if (priority != 6) {
                old_value = Binary_Output_Present_Value(object_instance);
                Binary_Output_Present_Value_Set(object_instance, value,
                    priority);
                if (pObject->Out_Of_Service) {
                    /* The physical point that the object represents
                        is not in service. This means that changes to the
                        Present_Value property are decoupled from the
                        physical output when the value of Out_Of_Service
                        is true. */
                } else if (Binary_Output_Write_Present_Value_Callback) {
                    new_value = Binary_Output_Present_Value(object_instance);
                    Binary_Output_Write_Present_Value_Callback(
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
 * remote node
 * @param  object_instance - object-instance number of the object
 * @param  priority - priority-array index value 1..16
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 * @return  true if values are within range and write is requested
 */
static bool Binary_Output_Present_Value_Relinquish_Write(
    uint32_t object_instance,
    uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;
    struct object_data *pObject;
    BACNET_BINARY_PV old_value = BINARY_INACTIVE;
    BACNET_BINARY_PV new_value = BINARY_INACTIVE;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            if (priority != 6) {
                old_value = Binary_Output_Present_Value(object_instance);
                Binary_Output_Present_Value_Relinquish(object_instance, priority);
                if (pObject->Out_Of_Service) {
                    /* The physical point that the object represents
                        is not in service. This means that changes to the
                        Present_Value property are decoupled from the
                        physical output when the value of Out_Of_Service
                        is true. */
                } else if (Binary_Output_Write_Present_Value_Callback) {
                    new_value = Binary_Output_Present_Value(object_instance);
                    Binary_Output_Write_Present_Value_Callback(
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
 * For a given object instance-number, returns the out-of-service
 * property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  out-of-service property value
 */
bool Binary_Output_Out_Of_Service(uint32_t object_instance)
{
    bool value = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Out_Of_Service;
    }

    return value;
}

/**
 * For a given object instance-number, sets the out-of-service property value
 *
 * @param object_instance - object-instance number of the object
 * @param value - boolean out-of-service value
 *
 * @return true if the out-of-service property value was set
 */
void Binary_Output_Out_Of_Service_Set(uint32_t object_instance, bool value)
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

/**
 * For a given object instance-number, loads the object-name into
 * a characterstring. Note that the object name must be unique
 * within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  object_name - holds the object-name retrieved
 *
 * @return  true if object-name was retrieved
 */
bool Binary_Output_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name)
{
    bool status = false;
    struct object_data *pObject;
    char name_text[32];

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Object_Name) {
            status =
                characterstring_init_ansi(object_name, pObject->Object_Name);
        } else {
            snprintf(
                name_text, sizeof(name_text), "BINARY OUTPUT %lu",
                (unsigned long)object_instance);
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
bool Binary_Output_Name_Set(uint32_t object_instance, const char *new_name)
{
    bool status = false; /* return value */
    BACNET_CHARACTER_STRING object_name;
    BACNET_OBJECT_TYPE found_type = OBJECT_NONE;
    uint32_t found_instance = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
const char *Binary_Output_Name_ASCII(uint32_t object_instance)
{
    const char *name = NULL;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        name = pObject->Object_Name;
    }

    return name;
}

/**
 * For a given object instance-number, returns the polarity property.
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  the polarity property of the object.
 */
BACNET_POLARITY Binary_Output_Polarity(uint32_t object_instance)
{
    BACNET_POLARITY polarity = POLARITY_NORMAL;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Polarity) {
            polarity = POLARITY_REVERSE;
        }
    }

    return polarity;
}

/**
 * For a given object instance-number, sets the polarity property.
 *
 * @param object_instance - object-instance number of the object
 * @param polarity - enumerated polarity property value
 *
 * @return true if the polarity property value was set
 */
bool Binary_Output_Polarity_Set(
    uint32_t object_instance, BACNET_POLARITY polarity)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (polarity < MAX_POLARITY) {
            if (polarity == POLARITY_NORMAL) {
                pObject->Polarity = false;
            } else {
                pObject->Polarity = true;
            }
            status = true;
        }
    }

    return status;
}

/**
 * For a given object instance-number, returns the relinquish-default
 * property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  relinquish-default property value
 */
BACNET_BINARY_PV Binary_Output_Relinquish_Default(uint32_t object_instance)
{
    BACNET_BINARY_PV value = BINARY_INACTIVE;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Relinquish_Default) {
            value = BINARY_ACTIVE;
        } else {
            value = BINARY_INACTIVE;
        }
    }

    return value;
}

/**
 * For a given object instance-number, sets the relinquish-default
 * property value
 *
 * @param object_instance - object-instance number of the object
 * @param value - floating point relinquish-default value
 *
 * @return true if the relinquish-default property value was set
 */
bool Binary_Output_Relinquish_Default_Set(
    uint32_t object_instance, BACNET_BINARY_PV value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (value == BINARY_ACTIVE) {
            pObject->Relinquish_Default = true;
            status = true;
        } else if (value == BINARY_INACTIVE) {
            pObject->Relinquish_Default = false;
            status = true;
        }
    }

    return status;
}

/**
 * For a given object instance-number, gets the reliability.
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return reliability value
 */
BACNET_RELIABILITY Binary_Output_Reliability(uint32_t object_instance)
{
    BACNET_RELIABILITY reliability = RELIABILITY_NO_FAULT_DETECTED;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        reliability = (BACNET_RELIABILITY)pObject->Reliability;
    }

    return reliability;
}

/**
 * For a given object, gets the Fault status flag
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  true the status flag is in Fault
 */
static bool Binary_Output_Object_Fault(const struct object_data *pObject)
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
 * For a given object instance-number, sets the reliability
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - reliability enumerated value
 *
 * @return  true if values are within range and property is set.
 */
bool Binary_Output_Reliability_Set(
    uint32_t object_instance, BACNET_RELIABILITY value)
{
    struct object_data *pObject;
    bool status = false;
    bool fault = false;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (value <= 255) {
            fault = Binary_Output_Object_Fault(pObject);
            pObject->Reliability = value;
            if (fault != Binary_Output_Object_Fault(pObject)) {
                pObject->Changed = true;
            }
            status = true;
        }
    }

    return status;
}

/**
 * For a given object instance-number, gets the Fault status flag
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  true the status flag is in Fault
 */
static bool Binary_Output_Fault(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);

    return Binary_Output_Object_Fault(pObject);
}

/**
 * For a given object instance-number, returns the description
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return description text or NULL if not found
 */
const char *Binary_Output_Description(uint32_t object_instance)
{
    const char *name = NULL;
    const struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        name = pObject->Description;
    }

    return name;
}

/**
 * For a given object instance-number, sets the description
 *
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the description to be set
 *
 * @return  true if object-name was set
 */
bool Binary_Output_Description_Set(
    uint32_t object_instance, const char *new_name)
{
    bool status = false; /* return value */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        status = true;
        pObject->Description = new_name;
    }

    return status;
}

/**
 * For a given object instance-number, returns the active text value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return active text or NULL if not found
 */
const char *Binary_Output_Active_Text(uint32_t object_instance)
{
    const char *name = NULL;
    const struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        name = pObject->Active_Text;
    }

    return name;
}

/**
 * For a given object instance-number, sets the description
 *
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the description to be set
 *
 * @return  true if object-name was set
 */
bool Binary_Output_Active_Text_Set(
    uint32_t object_instance, const char *new_name)
{
    bool status = false; /* return value */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        status = true;
        pObject->Active_Text = new_name;
    }

    return status;
}

/**
 * For a given object instance-number, returns the active text value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return active text or NULL if not found
 */
const char *Binary_Output_Inactive_Text(uint32_t object_instance)
{
    const char *name = NULL;
    const struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        name = pObject->Inactive_Text;
    }

    return name;
}

/**
 * For a given object instance-number, sets the description
 *
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the description to be set
 *
 * @return  true if object-name was set
 */
bool Binary_Output_Inactive_Text_Set(
    uint32_t object_instance, const char *new_name)
{
    bool status = false; /* return value */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        status = true;
        pObject->Inactive_Text = new_name;
    }

    return status;
}

/**
 * Get the COV change flag status
 *
 * @param object_instance - object-instance number of the object
 * @return the COV change flag status
 */
bool Binary_Output_Change_Of_Value(uint32_t object_instance)
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
 * Clear the COV change flag
 *
 * @param object_instance - object-instance number of the object
 */
void Binary_Output_Change_Of_Value_Clear(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Changed = false;
    }
}

/**
 * Encode the Value List for Present-Value and Status-Flags
 *
 * @param object_instance - object-instance number of the object
 * @param  value_list - #BACNET_PROPERTY_VALUE with at least 2 entries
 *
 * @return true if values were encoded
 */
bool Binary_Output_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list)
{
    bool status = false;
    struct object_data *pObject;
    bool in_alarm = false;
    bool out_of_service = false;
    bool fault = false;
    bool overridden = false;
    BACNET_BINARY_PV value;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (Binary_Output_Event_State(object_instance) != EVENT_STATE_NORMAL) {
            in_alarm = true;
        }
        fault = Binary_Output_Object_Fault(pObject);
        value = pObject->Prior_Value;
        overridden = pObject->Overridden;
        out_of_service = pObject->Out_Of_Service;
        status = cov_value_list_encode_enumerated(
            value_list, value, in_alarm, fault, overridden,
            out_of_service);
    }
    return status;
}

/**
 * @brief For a given object instance-number, returns the overridden
 * status flag value
 * @param  object_instance - object-instance number of the object
 * @return  out-of-service property value
 */
bool Binary_Output_Overridden(uint32_t object_instance)
{
    bool value = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
void Binary_Output_Overridden_Set(uint32_t object_instance, bool value)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Overridden != value) {
            pObject->Overridden = value;
            pObject->Changed = true;
        }
    }
}

/**
 * ReadProperty handler for this object.  For the given ReadProperty
 * data, the application_data is loaded or the error flags are set.
 *
 * @param  rpdata - BACNET_READ_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 *
 * @return number of APDU bytes in the response, or
 * BACNET_STATUS_ERROR on error.
 */
int Binary_Output_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    BACNET_BINARY_PV present_value = BINARY_INACTIVE;
    BACNET_POLARITY polarity = POLARITY_NORMAL;
    unsigned i = 0;
    bool state = false;
    uint8_t *apdu = NULL;
    int apdu_size = 0;
#if defined(INTRINSIC_REPORTING)
    int len = 0;
    ACKED_INFO *ack_info[MAX_BACNET_EVENT_TRANSITION] = { 0 };
    BACNET_DATE_TIME *timestamp[MAX_BACNET_EVENT_TRANSITION];
#endif

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    if (!property_lists_member(
            Properties_Required, Properties_Optional, Properties_Proprietary,
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
            Binary_Output_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(&apdu[0], Object_Type);
            break;
        case PROP_PRESENT_VALUE:
            present_value =
                Binary_Output_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
#if defined(INTRINSIC_REPORTING)
        case PROP_FEEDBACK_VALUE:
            present_value = Binary_Output_Feedback_Value(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
#endif
        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM,
            Binary_Output_Event_State(rpdata->object_instance) !=
                    EVENT_STATE_NORMAL);
            state = Binary_Output_Fault(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, state);
            state = Binary_Output_Overridden(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, state);
            state = Binary_Output_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_RELIABILITY:
            apdu_len = encode_application_enumerated(
                &apdu[0], Binary_Output_Reliability(rpdata->object_instance));
            break;
        case PROP_EVENT_STATE:
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0], 
                    Binary_Output_Event_State(rpdata->object_instance));
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;
        case PROP_OUT_OF_SERVICE:
            state = Binary_Output_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_POLARITY:
            polarity = Binary_Output_Polarity(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], polarity);
            break;
        case PROP_PRIORITY_ARRAY:
            apdu_len = bacnet_array_encode(
                rpdata->object_instance, rpdata->array_index,
                Binary_Output_Priority_Array_Encode, BACNET_MAX_PRIORITY, apdu,
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
            present_value =
                Binary_Output_Relinquish_Default(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], present_value);
            break;
        case PROP_DESCRIPTION:
            characterstring_init_ansi(
                &char_string,
                Binary_Output_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_ACTIVE_TEXT:
            characterstring_init_ansi(
                &char_string,
                Binary_Output_Active_Text(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_INACTIVE_TEXT:
            characterstring_init_ansi(
                &char_string,
                Binary_Output_Inactive_Text(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
#if (BACNET_PROTOCOL_REVISION >= 17)
        case PROP_CURRENT_COMMAND_PRIORITY:
            i = Binary_Output_Present_Value_Priority(rpdata->object_instance);
            if ((i >= BACNET_MIN_PRIORITY) && (i <= BACNET_MAX_PRIORITY)) {
                apdu_len = encode_application_unsigned(&apdu[0], i);
            } else {
                apdu_len = encode_application_null(&apdu[0]);
            }
            break;
#endif
#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            i = Binary_Output_Time_Delay(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], i);
            break;

        case PROP_NOTIFICATION_CLASS:
            i = Binary_Output_Notification_Class(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], i);
            break;

        case PROP_ALARM_VALUE:
            present_value = Binary_Output_Alarm_Value(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], present_value);
            break;

        case PROP_EVENT_ENABLE:
            i = Binary_Output_Event_Enable(rpdata->object_instance);
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
            state = Binary_Output_Acked_Transitions(rpdata->object_instance, ack_info);
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
            i = Binary_Output_Notify_Type(rpdata->object_instance);
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
                state = Binary_Output_Event_Time_Stamps(rpdata->object_instance, timestamp);
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
                state = Binary_Output_Event_Time_Stamps(rpdata->object_instance, timestamp);
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
        case PROP_EVENT_MESSAGE_TEXTS:
            apdu_len = bacnet_array_encode(
                rpdata->object_instance, rpdata->array_index,
                Binary_Output_Event_Message_Texts_Encode,
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
 * WriteProperty handler for this object.  For the given WriteProperty
 * data, the application_data is loaded or the error flags are set.
 *
 * @param  wp_data - BACNET_WRITE_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 *
 * @return false if an error is loaded, true if no errors
 */
bool Binary_Output_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value = { 0 };
    struct uci_context *ctxw = NULL;
    char *idx_c = NULL;
    int idx_c_len = 0;
    bool value_b = false;
    char *value_c = NULL;

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
    ctxw = ucix_init(sec);
    if (!ctxw)
        fprintf(stderr, "Failed to load config file %s\n",sec);
    idx_c_len = snprintf(NULL, 0, "%d", wp_data->object_instance);
    idx_c = malloc(idx_c_len + 1);
    snprintf(idx_c,idx_c_len + 1,"%d",wp_data->object_instance);
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                if (Binary_Output_Present_Value_Write(
                        wp_data->object_instance, value.type.Enumerated,
                        wp_data->priority, &wp_data->error_class,
                        &wp_data->error_code)) {
                    value_b = Binary_Output_Present_Value(wp_data->object_instance);
                    ucix_add_option_int(ctxw, sec, idx_c, "value", value_b);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_NULL);
                if (status) {
                    if (Binary_Output_Present_Value_Relinquish_Write(
                        wp_data->object_instance, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code)) {
                        value_b = Binary_Output_Present_Value(wp_data->object_instance);
                        ucix_add_option_int(ctxw, sec, idx_c, "value", value_b);
                        ucix_commit(ctxw,sec);
                        free(value_c);
                    }
                }
            }
            break;
        case PROP_OUT_OF_SERVICE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                Binary_Output_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
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
                if (Binary_Output_Name_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "name",
                        strndup(value.type.Character_String.value,value.type.Character_String.length));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_ACTIVE_TEXT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Binary_Output_Active_Text_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "active_text",
                        Binary_Output_Active_Text(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_INACTIVE_TEXT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Binary_Output_Inactive_Text_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "inactive_text",
                        Binary_Output_Inactive_Text(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_POLARITY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                if (Binary_Output_Polarity_Set(
                    wp_data->object_instance, value.type.Enumerated)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "polarity",
                        Binary_Output_Polarity(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_RELIABILITY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status)
                Binary_Output_Reliability_Set(wp_data->object_instance,
                    value.type.Enumerated);
            break;
        case PROP_PRIORITY_ARRAY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_RELINQUISH_DEFAULT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status)
                Binary_Output_Relinquish_Default_Set(wp_data->object_instance,
                    value.type.Enumerated);
            break;
        case PROP_DESCRIPTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Binary_Output_Description_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "description",
                        Binary_Output_Description(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
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
                if (Binary_Output_Time_Delay_Set(
                    wp_data->object_instance, value.type.Unsigned_Int)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "time_delay",
                        Binary_Output_Time_Delay(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_NOTIFICATION_CLASS:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                if (Binary_Output_Notification_Class_Set(
                    wp_data->object_instance, value.type.Unsigned_Int)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "nc",
                        Binary_Output_Notification_Class(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_ALARM_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                if (Binary_Output_Alarm_Value_Set(wp_data->object_instance,
                    value.type.Boolean)) {
                    value_b = Binary_Output_Alarm_Value(wp_data->object_instance);
                    ucix_add_option_int(ctxw, sec, idx_c, "alarm_value", value_b);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            }
            break;
        case PROP_EVENT_ENABLE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BIT_STRING);
            if (status) {
                if (Binary_Output_Event_Enable_Set(
                    wp_data->object_instance, value.type.Bit_String.value[0])) {
                    ucix_add_option_int(ctxw, sec, idx_c, "event",
                        Binary_Output_Event_Enable(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_NOTIFY_TYPE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                if (Binary_Output_Notify_Type_Set(
                    wp_data->object_instance, value.type.Enumerated)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "notify_type",
                        Binary_Output_Notify_Type(wp_data->object_instance));
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
    /* not using len at this time */
    (void)len;

    return status;
}

/**
 * @brief Sets a callback used when present-value is written from BACnet
 * @param cb - callback used to provide indications
 */
void Binary_Output_Write_Present_Value_Callback_Set(
    binary_output_write_present_value_callback cb)
{
    Binary_Output_Write_Present_Value_Callback = cb;
}

#if defined(INTRINSIC_REPORTING)
static const char *Binary_Output_Event_Message(
    uint32_t object_instance,
    enum BACnetEventTransitionBits transition,
    const char *default_text)
{
    const char *text = NULL;
    struct object_data *pObject = NULL;
    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && transition < MAX_BACNET_EVENT_TRANSITION) {
        text = pObject->Event_Message_Texts[transition];
        if (!text) {
            text = Notification_Class_Event_Message_Text(
                    pObject->Notification_Class,
                    transition);
        }
        if (!text) {
            return default_text;
        } else {
            return text;
        }
    }
    return default_text;
}
#endif

void Binary_Output_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    struct object_data *pObject;
    BACNET_EVENT_NOTIFICATION_DATA event_data;
    const char *msgText = NULL;
    BACNET_CHARACTER_STRING msgCharString = { 0 };
    uint8_t FromState = 0;
    uint8_t ToState;
    BACNET_BINARY_PV PresentVal = false;
    BACNET_RELIABILITY Reliability = RELIABILITY_NO_FAULT_DETECTED;
    BACNET_PROPERTY_VALUE propertyValues = { 0 };
    bool SendNotify = false;

    pObject = Keylist_Data(Object_List, object_instance);

    if (!pObject)
        return;

    if (pObject->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        pObject->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = pObject->Ack_notify_data.EventState;

        debug_printf(
            "Binary-Output[%d]: Send Acknotification.\n", object_instance);

        msgText = "AckNotification";

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        PresentVal = Binary_Output_Present_Value(object_instance);
        FromState = pObject->Event_State;
        Reliability = pObject->Reliability;
        if (Reliability != RELIABILITY_NO_FAULT_DETECTED) {
            /*Fault detection takes precedence over the detection of normal and
            offnormal states. As such, when Reliability has a value other than
            NO_FAULT_DETECTED, the event-state-detection process will determine
            the object's event state to be FAULT.*/
            pObject->Event_State = EVENT_STATE_FAULT;
        } else if (FromState == EVENT_STATE_FAULT) {
            pObject->Event_State = EVENT_STATE_NORMAL;
        } else {
            switch (pObject->Event_State) {
                case EVENT_STATE_NORMAL:
                    /* (a) If pCurrentState is NORMAL, and pMonitoredValue is equal
                    to any of the values contained in pAlarmValues for
                        pTimeDelay, then indicate a transition to the OFFNORMAL
                    event state.
                    */
                    if ((PresentVal == pObject->Alarm_Value) &&
                        ((pObject->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                            EVENT_ENABLE_TO_OFFNORMAL)) {
                        if (!pObject->Remaining_Time_Delay) {
                            pObject->Event_State = EVENT_STATE_OFFNORMAL;
                        } else {
                            pObject->Remaining_Time_Delay--;
                        }
                        break;
                    }

                    /* value of the object is still in the same event state */
                    pObject->Remaining_Time_Delay = pObject->Time_Delay;
                    break;

                case EVENT_STATE_OFFNORMAL:
                    /* (b) If pCurrentState is OFFNORMAL, and pMonitoredValue is not
                    equal to any of the values contained in pAlarmValues for
                    pTimeDelayNormal, then indicate a transition to the NORMAL
                    event state.
                    */
                    if (PresentVal != pObject->Alarm_Value &&
                        ((pObject->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                          EVENT_ENABLE_TO_NORMAL)) {
                        if (!pObject->Remaining_Time_Delay) {
                            pObject->Event_State = EVENT_STATE_NORMAL;
                        } else {
                            pObject->Remaining_Time_Delay--;
                        }
                        break;
                    }

                    /* value of the object is still in the same event state */
                    pObject->Remaining_Time_Delay = pObject->Time_Delay;
                    break;

                default:
                    return; /* shouldn't happen */
            } /* switch (FromState) */
        }

        ToState = pObject->Event_State;

        if (FromState != ToState ||
            (ToState == EVENT_STATE_FAULT &&
             Reliability != pObject->Last_ToFault_Event_Reliability)) {
            /* Event_State has changed.
               Need to fill only the basic parameters of this type of event.
               Other parameters will be filled in common function. */

            switch (ToState) {
                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_OFFNORMAL) {
                        msgText = Binary_Output_Event_Message(
                            object_instance, TRANSITION_TO_NORMAL,
                            "Back to normal state from off-normal");
                    } else {
                        msgText = Binary_Output_Event_Message(
                            object_instance, TRANSITION_TO_NORMAL,
                            "Back to normal state from fault");
                    }
                    break;

                case EVENT_STATE_OFFNORMAL:
                    msgText = Binary_Output_Event_Message(
                        object_instance, TRANSITION_TO_OFFNORMAL,
                        "Goes to off-normal");
                    break;

                case EVENT_STATE_FAULT:
                    msgText = Binary_Output_Event_Message(
                        object_instance, TRANSITION_TO_FAULT,
                        bactext_reliability_name(Reliability));
                    pObject->Last_ToFault_Event_Reliability = Reliability;
                    break;

                default:
                    break;
            } /* switch (ToState) */
            debug_printf(
                "Binary-Output[%d]: Event_State goes from %.128s to %.128s.\n",
                object_instance, bactext_event_state_name(FromState),
                bactext_event_state_name(ToState));
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
            /* set eventType and fill Event_Time_Stamps and
             * Event_Message_Texts*/
            switch (ToState) {
                case EVENT_STATE_OFFNORMAL:
                    event_data.eventType = EVENT_CHANGE_OF_STATE;
                    datetime_copy(
                        &pObject->Event_Time_Stamps[TRANSITION_TO_OFFNORMAL],
                        &event_data.timeStamp.value.dateTime);
                    pObject->Event_Message_Texts[TRANSITION_TO_OFFNORMAL] =
                        msgText;
                    break;
                case EVENT_STATE_FAULT:
                    event_data.eventType = EVENT_CHANGE_OF_RELIABILITY;
                    datetime_copy(
                        &pObject->Event_Time_Stamps[TRANSITION_TO_FAULT],
                        &event_data.timeStamp.value.dateTime);
                    pObject->Event_Message_Texts[TRANSITION_TO_FAULT] =
                        msgText;
                    break;
                case EVENT_STATE_NORMAL:
                    event_data.eventType = FromState == EVENT_STATE_FAULT
                        ? EVENT_CHANGE_OF_RELIABILITY
                        : EVENT_CHANGE_OF_STATE;
                    datetime_copy(
                        &pObject->Event_Time_Stamps[TRANSITION_TO_NORMAL],
                        &event_data.timeStamp.value.dateTime);
                    pObject->Event_Message_Texts[TRANSITION_TO_NORMAL] =
                        msgText;
                    break;
                default:
                    break;
            }
        } else {
            /* fill event_data timeStamp */
            switch (ToState) {
                case EVENT_STATE_OFFNORMAL:
                    event_data.eventType = EVENT_CHANGE_OF_STATE;
                    datetime_copy(
                        &event_data.timeStamp.value.dateTime,
                        &pObject->Event_Time_Stamps[TRANSITION_TO_OFFNORMAL]);
                    break;
                case EVENT_STATE_FAULT:
                    event_data.eventType = EVENT_CHANGE_OF_RELIABILITY;
                    datetime_copy(
                        &event_data.timeStamp.value.dateTime,
                        &pObject->Event_Time_Stamps[TRANSITION_TO_FAULT]);
                    break;
                case EVENT_STATE_NORMAL:
                    event_data.eventType = FromState == EVENT_STATE_FAULT
                        ? EVENT_CHANGE_OF_RELIABILITY
                        : EVENT_CHANGE_OF_STATE;
                    datetime_copy(
                        &event_data.timeStamp.value.dateTime,
                        &pObject->Event_Time_Stamps[TRANSITION_TO_NORMAL]);
                    break;
                default:
                    break;
            }
        }

        /* Notification Class */
        event_data.notificationClass = pObject->Notification_Class;

        /* Message Text */
        characterstring_init_ansi(&msgCharString, msgText);
        event_data.messageText = &msgCharString;

        /* Notify Type */
        /* filled before */

        /* From State */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            event_data.fromState = FromState;
        }

        /* To State */
        event_data.toState = pObject->Event_State;

        /* Event Values */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            if (event_data.eventType == EVENT_CHANGE_OF_STATE) {
                /* Value that exceeded a limit. */
                event_data.notificationParams.changeOfState.newState.tag = PROP_STATE_BINARY_VALUE;
                event_data.notificationParams.changeOfState.newState.state.binaryValue = pObject->Prior_Value;
                /* Status_Flags of the referenced object. */
                bitstring_init(
                    &event_data.notificationParams.changeOfState.statusFlags);
                bitstring_set_bit(
                    &event_data.notificationParams.changeOfState.statusFlags,
                    STATUS_FLAG_IN_ALARM,
                    pObject->Event_State != EVENT_STATE_NORMAL);
                bitstring_set_bit(
                    &event_data.notificationParams.changeOfState.statusFlags,
                    STATUS_FLAG_FAULT, false);
                bitstring_set_bit(
                    &event_data.notificationParams.changeOfState.statusFlags,
                    STATUS_FLAG_OVERRIDDEN, false);
                bitstring_set_bit(
                    &event_data.notificationParams.changeOfState.statusFlags,
                    STATUS_FLAG_OUT_OF_SERVICE, pObject->Out_Of_Service);
            } else {
                event_data.notificationParams.changeOfReliability.reliability =
                    Reliability;

                propertyValues.propertyIdentifier = PROP_PRESENT_VALUE;
                propertyValues.propertyArrayIndex = BACNET_ARRAY_ALL;
                propertyValues.value.tag = BACNET_APPLICATION_TAG_BOOLEAN;
                propertyValues.value.type.Boolean = PresentVal;
                event_data.notificationParams.changeOfReliability
                    .propertyValues = &propertyValues;

                bitstring_init(&event_data.notificationParams
                                    .changeOfReliability.statusFlags);
                bitstring_set_bit(
                    &event_data.notificationParams.outOfRange.statusFlags,
                    STATUS_FLAG_IN_ALARM, false);
                bitstring_set_bit(
                    &event_data.notificationParams.outOfRange.statusFlags,
                    STATUS_FLAG_FAULT,
                    pObject->Event_State != EVENT_STATE_NORMAL);
                bitstring_set_bit(
                    &event_data.notificationParams.outOfRange.statusFlags,
                    STATUS_FLAG_OVERRIDDEN, false);
                bitstring_set_bit(
                    &event_data.notificationParams.outOfRange.statusFlags,
                    STATUS_FLAG_OUT_OF_SERVICE, pObject->Out_Of_Service);
            }
        }

        /* add data from notification class */
        debug_printf(
            "Binary-Output[%d]: Notification Class[%d]-%s "
            "%u/%u/%u-%u:%u:%u.%u!\n",
            object_instance, event_data.notificationClass,
            bactext_event_type_name(event_data.eventType),
            (unsigned)event_data.timeStamp.value.dateTime.date.year,
            (unsigned)event_data.timeStamp.value.dateTime.date.month,
            (unsigned)event_data.timeStamp.value.dateTime.date.day,
            (unsigned)event_data.timeStamp.value.dateTime.time.hour,
            (unsigned)event_data.timeStamp.value.dateTime.time.min,
            (unsigned)event_data.timeStamp.value.dateTime.time.sec,
            (unsigned)event_data.timeStamp.value.dateTime.time.hundredths);
        Notification_Class_common_reporting_function(&event_data);

        /* Ack required */
        if ((event_data.notifyType != NOTIFY_ACK_NOTIFICATION) &&
            (event_data.ackRequired == true)) {
            debug_printf("Binary-Output[%d]: Ack Required!\n", object_instance);
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
#else
    (void)object_instance;
#endif /* defined(INTRINSIC_REPORTING) */
}


#if defined(INTRINSIC_REPORTING)
int Binary_Output_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    struct object_data *pObject;
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;

    pObject = Keylist_Data(Object_List, Binary_Output_Index_To_Instance(index));

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
            Binary_Output_Index_To_Instance(index);
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

int Binary_Output_Alarm_Ack(
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

int Binary_Output_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, Binary_Output_Index_To_Instance(index));

    if (getalarm_data == NULL) {
        debug_printf(
            "[%s %d]: NULL pointer parameter! getalarm_data = %p\r\n", __FILE__,
            __LINE__, (void *)getalarm_data);
        return -2;
    }

    /* check index */
    if (pObject) {
        /* Event_State is not equal to NORMAL  and
           Notify_Type property value is ALARM */
        if ((pObject->Event_State != EVENT_STATE_NORMAL) &&
            (pObject->Notify_Type == NOTIFY_ALARM)) {
            /* Object Identifier */
            getalarm_data->objectIdentifier.type = Object_Type;
            getalarm_data->objectIdentifier.instance =
                Binary_Output_Index_To_Instance(index);
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

/**
 * @brief Set the context used with a specific object instance
 * @param object_instance [in] BACnet object instance number
 * @param context [in] pointer to the context
 */
void *Binary_Output_Context_Get(uint32_t object_instance)
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
void Binary_Output_Context_Set(uint32_t object_instance, void *context)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Context = context;
    }
}

/**
 * @brief Creates a Binary Output object
 * @param object_instance - object-instance number of the object
 * @return the object-instance that was created, or BACNET_MAX_INSTANCE
 */
uint32_t  Binary_Output_Create(uint32_t object_instance)
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
            pObject->Reliability = RELIABILITY_NO_FAULT_DETECTED;
            for (priority = 0; priority < BACNET_MAX_PRIORITY; priority++) {
                pObject->Relinquished[priority] = true;
                pObject->Priority_Array[priority] = false;
            }
            pObject->Out_Of_Service = false;
            pObject->Active_Text = "Active";
            pObject->Inactive_Text = "Inactive";
            pObject->Changed = false;
#if defined(INTRINSIC_REPORTING)
            pObject->Event_State = EVENT_STATE_NORMAL;
            pObject->Event_Detection_Enable = true;
            pObject->Event_Enable = true;
            /* notification class not connected */
            pObject->Notification_Class = BACNET_MAX_INSTANCE;

            /* Set handler for GetEventInformation function */
            handler_get_event_information_set(
                Object_Type, Binary_Output_Event_Information);
            /* Set handler for AcknowledgeAlarm function */
            handler_alarm_ack_set(Object_Type, Binary_Output_Alarm_Ack);
            /* Set handler for GetAlarmSummary Service */
            handler_get_alarm_summary_set(
                Object_Type, Binary_Output_Alarm_Summary);
#endif
            /* add to list */
            index = Keylist_Data_Add(Object_List, object_instance, pObject);
            if (index < 0) {
                free(pObject);
                return BACNET_MAX_INSTANCE;
            }
#if defined(INTRINSIC_REPORTING)
            Binary_Output_Reset_Event_Properties(object_instance);
#endif
        } else {
            return BACNET_MAX_INSTANCE;
        }
    }

    return object_instance;
}

/**
 * Initializes the Binary Input object data
 */
void Binary_Output_Cleanup(void)
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

/**
 * Creates a Binary Input object
 */
bool Binary_Output_Delete(uint32_t object_instance)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data_Delete(Object_List, object_instance);
    if (pObject) {
        free(pObject);
        status = true;
        Device_Inc_Database_Revision();
    }

    return status;
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
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
    bool value_b = false;
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
    value_b = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "value", 0);
    pObject->Priority_Array[BACNET_MAX_PRIORITY-1] = value_b;
    pObject->Relinquished[BACNET_MAX_PRIORITY-1] = false;
    pObject->Prior_Value = value_b;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "active_text");
    if (option && characterstring_init_ansi(&option_str, option))
        pObject->Active_Text = strndup(option, option_str.length);
    else
        pObject->Active_Text = strdup(ictx->Object.Active_Text);
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "inactive_text");
    if (option && characterstring_init_ansi(&option_str, option))
        pObject->Inactive_Text = strndup(option, option_str.length);
    else
        pObject->Inactive_Text = strdup(ictx->Object.Inactive_Text);
#if defined(INTRINSIC_REPORTING)
    value_b = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "fb_value", 0);
    pObject->Feedback_Value = value_b;
#endif
#if defined(INTRINSIC_REPORTING)
    pObject->Event_State = EVENT_STATE_NORMAL;
    /* notification class not connected */
    pObject->Notification_Class = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "nc", ictx->Object.Notification_Class);
    pObject->Event_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "event", ictx->Object.Event_Enable);
    pObject->Event_Detection_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "event_detection", ictx->Object.Event_Detection_Enable);
    pObject->Time_Delay = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "time_delay", ictx->Object.Time_Delay);
    value_b = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "alarm_value", 0);
    pObject->Alarm_Value = value_b;

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
 * Initializes the Binary Input object data
 */
void Binary_Output_Init(void)
{
    struct uci_context *ctx;
    struct object_data_t tObject = { 0 };
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
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
        tObject.Description = "Binary Ouput";
    option = ucix_get_option(ctx, sec, "default", "active_text");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Active_Text = strndup(option,option_str.length);
    else
        tObject.Active_Text = "Active";
    option = ucix_get_option(ctx, sec, "default", "inactive_text");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Inactive_Text = strndup(option,option_str.length);
    else
        tObject.Inactive_Text = "Inactive";
#if defined(INTRINSIC_REPORTING)
    tObject.Notification_Class = ucix_get_option_int(ctx, sec, "default", "nc", BACNET_MAX_INSTANCE);
    tObject.Event_Enable = ucix_get_option_int(ctx, sec, "default", "event", 0);
    tObject.Event_Detection_Enable = ucix_get_option_int(ctx, sec, "default", "event_detection", 0);
    tObject.Time_Delay = ucix_get_option_int(ctx, sec, "default", "time_delay", 0);
    option = ucix_get_option(ctx, sec, "default", "alarm_value");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Alarm_Value = strndup(option,option_str.length);
    else
        tObject.Alarm_Value = "0";
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
        Binary_Output_Event_Information);
    /* Set handler for AcknowledgeAlarm function */
    handler_alarm_ack_set(Object_Type, Binary_Output_Alarm_Ack);
    /* Set handler for GetAlarmSummary Service */
    handler_get_alarm_summary_set(Object_Type,
        Binary_Output_Alarm_Summary);
#endif
}
