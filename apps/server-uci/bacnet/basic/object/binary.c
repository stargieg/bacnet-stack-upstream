/**
 * @file
 * @brief BACnet Stack initialization and task handler
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date March 2024
 * @copyright SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <stdbool.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
#include "bacnet/bactext.h"
#include "bacnet/cov.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/getevent.h"
#include "bacnet/alarm_ack.h"
#include "bacnet/get_alarm_sum.h"
#endif
#include "bacnet/basic/sys/debug.h"
#include "bacnet/basic/object/device.h"
/* me! */
#include "bacnet/basic/object/binary.h"


/**
 * @brief Encode a BACnetARRAY property value
 * @param get_pObject [in] function to get a pointer to Object_List
 * @param object_instance [in] BACnet network port object instance number
 * @param array_index [in] array index requested:
 *    0 for the array size
 *    1 to n for individual array members
 *    BACNET_ARRAY_ALL for the full array to be read.
 * @param encoder [in] function to encode one property array element
 * @param array_size [in] number of elements in the array
 * @param apdu [out] Buffer in which the APDU contents are built.
 * @param max_apdu [in] Max length of the APDU buffer.
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for an invalid array index
 *   BACNET_STATUS_ABORT for abort message.
 */
int bacnet_array_encode_binary(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX array_index,
    bacnet_array_property_element_encode_function_object encoder,
    BACNET_UNSIGNED_INTEGER array_size,
    uint8_t *apdu,
    int max_apdu)
{
    int apdu_len = 0, len = 0;
    BACNET_ARRAY_INDEX index;

    if (array_index == 0) {
        /* Array element zero is the number of objects in the list */
        len = encode_application_unsigned(NULL, array_size);
        if (len > max_apdu) {
            apdu_len = BACNET_STATUS_ABORT;
        } else {
            len = encode_application_unsigned(apdu, array_size);
            apdu_len = len;
        }
    } else if (array_index == BACNET_ARRAY_ALL) {
        /* if no index was specified, then try to encode the entire list */
        /* into one packet. */
        for (index = 0; index < array_size; index++) {
            len += encoder(get_pObject, object_instance, index, NULL);
        }
        if (len > max_apdu) {
            /* encoded size is larger than APDU size */
            apdu_len = BACNET_STATUS_ABORT;
        } else {
            for (index = 0; index < array_size; index++) {
                len = encoder(get_pObject, object_instance, index, apdu);
                if (apdu) {
                    apdu += len;
                }
                apdu_len += len;
            }
        }
    } else if (array_index <= array_size) {
        /* index was specified; encode a single array element */
        index = array_index - 1;
        len = encoder(get_pObject, object_instance, index, NULL);
        if (len > max_apdu) {
            apdu_len = BACNET_STATUS_ABORT;
        } else {
            len = encoder(get_pObject, object_instance, index, apdu);
            apdu_len = len;
        }
    } else {
        /* array_index was specified out of range */
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/**
 * @brief Get the object name
 * @param  object pointer - struct object_data
 * @param  object_name - holds the object-name to be retrieved
 * @return  true if object-name was retrieved
 */
bool Binary_Object_Name(
    const struct object_data *pObject, BACNET_CHARACTER_STRING *object_name)
{
    bool status = false;
    if (pObject) {
        if (pObject->Object_Name) {
            status =
                characterstring_init_ansi(object_name, pObject->Object_Name);
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the description
 * @param  object_instance - object-instance number of the object
 * @return description text or NULL if not found
 */
const char *Binary_Description(const struct object_data *pObject)
{
    const char *name = NULL;
    if (pObject) {
        name = pObject->Description;
    }

    return name;
}

/**
 * @brief For a given object instance-number, determines the present-value
 * @param  object pointer - struct object_data
 * @return  present-value of the object
 */
BACNET_BINARY_PV Binary_Present_Value(const struct object_data *pObject)
{
    BACNET_BINARY_PV value = 0;
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
 * @brief For a given object instance-number, determines the priority
 * @param  object_instance - object-instance number of the object
 * @return  active priority 1..16, or 0 if no priority is active
 */
unsigned Binary_Present_Value_Priority(
    const struct object_data *pObject)
{
    unsigned p = 0; /* loop counter */
    unsigned priority = 0; /* return value */
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
 * @brief For a given object, gets the Fault status flag
 * @param  object pointer - struct object_data
 * @return  true the status flag is in Fault
 */
bool Binary_Object_Fault(const struct object_data *pObject)
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
 * @brief For a given object instance-number, checks the present-value for COV
 * @param object pointer - struct object_data
 * @param value  Given present value.
 */
void Binary_COV_Detect(struct object_data *pObject, BACNET_BINARY_PV value)
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

/**
 * @brief Encode a BACnetARRAY property element
 * @param object pointer - struct object_data
 * @param index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
int Binary_Priority_Array_Encode(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX index,
    uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    struct object_data *pObject;
    BACNET_BINARY_PV value = BINARY_INACTIVE;

    pObject = get_pObject(object_instance);
    if (pObject && (index < BACNET_MAX_PRIORITY)) {
        if (pObject->Relinquished[index]) {
            apdu_len = encode_application_null(apdu);
        } else {
            value = pObject->Priority_Array[index];
            apdu_len = encode_application_enumerated(apdu, value);
        }
    }

    return apdu_len;
}

/**
 * For a given object instance-number, loads the value_list with the COV data.
 *
 * @param  object pointer - struct object_data
 * @param  object_instance - object-instance number of the object
 * @param  value_list - list of COV data
 *
 * @return  true if the value list is encoded
 */
bool Binary_Encode_Value_List(
    struct object_data *pObject,
    BACNET_PROPERTY_VALUE *value_list)
{
    bool status = false;
    bool in_alarm = false;
    bool out_of_service = false;
    bool fault = false;
    bool overridden = false;
    BACNET_BINARY_PV present_value = BINARY_INACTIVE;

    if (pObject) {
        if (pObject->Event_State != EVENT_STATE_NORMAL){
            in_alarm = true;
        }
        if (Binary_Object_Fault(pObject)){
            fault = true;
        }
        overridden = pObject->Overridden;
        out_of_service = pObject->Out_Of_Service;
        present_value = pObject->Prior_Value;
        status = cov_value_list_encode_enumerated(
            value_list, present_value, in_alarm, fault, overridden,
            out_of_service);
    }

    return status;
}

#if defined(INTRINSIC_REPORTING)
/**
 * @brief For a given object instance-number and event transition, returns the
 * event message text
 * @param  object pointer - struct object_data
 * @param  object_instance - object-instance number of the object
 * @param  transition - transition type
 * @return event message text or NULL if object not found or transition invalid
 */
const char *Binary_Event_Message_Text(
    bacnet_get_pObject get_pObject,
    const uint32_t object_instance,
    const enum BACnetEventTransitionBits transition)
{
    const char *text = NULL;
    const struct object_data *pObject;

    pObject = get_pObject(object_instance);
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
 * @brief Encode a EventTimeStamps property element
 * @param object pointer - struct object_data
 * @param object_instance [in] BACnet network port object instance number
 * @param index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
int Binary_Event_Time_Stamps_Encode(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX index,
    uint8_t *apdu)
{
    int apdu_len = 0, len = 0;
    struct object_data *pObject;

    pObject = get_pObject(object_instance);
    if (pObject) {
        if (index < MAX_BACNET_EVENT_TRANSITION) {
            len = encode_opening_tag(apdu, TIME_STAMP_DATETIME);
            apdu_len += len;
            if (apdu) {
                apdu += len;
            }
            len = encode_application_date(
                apdu, &pObject->Event_Time_Stamps[index].date);
            apdu_len += len;
            if (apdu) {
                apdu += len;
            }
            len = encode_application_time(
                apdu, &pObject->Event_Time_Stamps[index].time);
            apdu_len += len;
            if (apdu) {
                apdu += len;
            }
            len = encode_closing_tag(apdu, TIME_STAMP_DATETIME);
            apdu_len += len;
        } else {
            apdu_len = BACNET_STATUS_ERROR;
        }
    } else {
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/**
 * @brief Encode a BACnetARRAY property element
 * @param object pointer - struct object_data
 * @param object_instance [in] object instance number
 * @param index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
int Binary_Event_Message_Texts_Encode(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX index,
    uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    const char *text = NULL; /* return value */
    BACNET_CHARACTER_STRING char_string = { 0 };

    text = Binary_Event_Message_Text(get_pObject, object_instance, index);
    if (text) {
        characterstring_init_ansi(&char_string, text);
        apdu_len = encode_application_character_string(apdu, &char_string);
    }

    return apdu_len;
}

/**
 * @brief Encode Event_Message
 * @param object pointer - struct object_data
 * @param transition [in] BACnetEventTransitionBits
 * @param default_text [in] default message
 * @return Event_Message char
 */
const char *Binary_Event_Message(
    struct object_data *pObject,
    enum BACnetEventTransitionBits transition,
    const char *default_text)
{
    const char *text = NULL;
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

/**
 * @brief Handles the Intrinsic Reporting Service for the Analog Input Object
 * @param  object pointer - struct object_data
 * @param  object_type - type of the object
 * @param  object_instance - object-instance number of the object
 * export
 */
void Binary_Intrinsic_Reporting(
    struct object_data *pObject,
    BACNET_OBJECT_TYPE Object_Type,
    uint32_t object_instance)
{
    BACNET_EVENT_NOTIFICATION_DATA event_data = { 0 };
    const char *msgText = NULL;
    BACNET_CHARACTER_STRING msgCharString = { 0 };
    uint8_t FromState = 0;
    uint8_t ToState = 0;
    BACNET_BINARY_PV PresentVal = BINARY_INACTIVE;
    BACNET_RELIABILITY Reliability = RELIABILITY_NO_FAULT_DETECTED;
    BACNET_PROPERTY_VALUE propertyValues = { 0 };
    bool SendNotify = false;

    if (!pObject) {
        return;
    }

    /* check limits */
    if (!pObject->Event_Enable) {
        return; /* limits are not configured */
    }

    if (pObject->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        pObject->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = pObject->Ack_notify_data.EventState;
        debug_printf(
            "Binary [%d]: Send AckNotification.\n", object_instance);
        msgText = "AckNotification";
        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;
        /* Send EventNotification. */
        SendNotify = true;
    } else {
        PresentVal = Binary_Present_Value(pObject);
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
                case EVENT_STATE_OFFNORMAL:
                    msgText = Binary_Event_Message(
                        pObject, TRANSITION_TO_OFFNORMAL,
                        "Goes to off-normal");
                    break;

                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_OFFNORMAL) {
                        msgText = Binary_Event_Message(
                            pObject, TRANSITION_TO_NORMAL,
                            "Back to normal state from off-normal");
                    } else {
                        msgText = Binary_Event_Message(
                            pObject, TRANSITION_TO_NORMAL,
                            "Back to normal state from fault");
                    }
                    break;

                case EVENT_STATE_FAULT:
                    msgText = Binary_Event_Message(
                            pObject, TRANSITION_TO_FAULT,
                        bactext_reliability_name(Reliability));
                    pObject->Last_ToFault_Event_Reliability = Reliability;
                    break;

                default:
                    break;
            } /* switch (ToState) */
            debug_printf(
                "Analog-Input[%d]: Event_State goes from %s to %s.\n",
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
                        : EVENT_OUT_OF_RANGE;
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
                        : EVENT_OUT_OF_RANGE;
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
            if (event_data.eventType == EVENT_OUT_OF_RANGE) {
                /* Value that exceeded a limit. */
                event_data.notificationParams.changeOfState.newState.tag = PROP_STATE_BINARY_VALUE;
                event_data.notificationParams.changeOfState.newState.state.binaryValue = pObject->Prior_Value;
                /* Status_Flags of the referenced object. */
                bitstring_init(
                    &event_data.notificationParams.outOfRange.statusFlags);
                bitstring_set_bit(
                    &event_data.notificationParams.outOfRange.statusFlags,
                    STATUS_FLAG_IN_ALARM,
                    pObject->Event_State != EVENT_STATE_NORMAL);
                bitstring_set_bit(
                    &event_data.notificationParams.outOfRange.statusFlags,
                    STATUS_FLAG_FAULT, false);
                bitstring_set_bit(
                    &event_data.notificationParams.outOfRange.statusFlags,
                    STATUS_FLAG_OVERRIDDEN, false);
                bitstring_set_bit(
                    &event_data.notificationParams.outOfRange.statusFlags,
                    STATUS_FLAG_OUT_OF_SERVICE, pObject->Out_Of_Service);
            } else {
                event_data.notificationParams.changeOfReliability.reliability =
                    Reliability;

                propertyValues.propertyIdentifier = PROP_PRESENT_VALUE;
                propertyValues.propertyArrayIndex = BACNET_ARRAY_ALL;
                propertyValues.value.tag = BACNET_APPLICATION_TAG_BOOLEAN;
                propertyValues.value.type.Real = PresentVal;
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
            "Binary [%d]: Notification Class[%d]-%s "
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
            debug_printf("Binary [%d]: Ack Required!\n", object_instance);
            switch (event_data.toState) {
                case EVENT_STATE_OFFNORMAL:
                    pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL]
                        .bIsAcked = false;
                    pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL]
                        .Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;
                case EVENT_STATE_FAULT:
                    pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                        false;
                    pObject->Acked_Transitions[TRANSITION_TO_FAULT]
                        .Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;
                case EVENT_STATE_NORMAL:
                    pObject->Acked_Transitions[TRANSITION_TO_NORMAL]
                        .bIsAcked = false;
                    pObject->Acked_Transitions[TRANSITION_TO_NORMAL]
                        .Time_Stamp = event_data.timeStamp.value.dateTime;
                    break;
                default: /* shouldn't happen */
                    break;
            }
        }
    }
}

/**
 * @brief Handles getting the Event Information for the Analog Input Object
 * @param  object pointer - struct object_data
 * @param  object_type - type of the object
 * @param  object_instance - object-instance number of the object
 * @param  getevent_data - data for the Event Information
 * @return 1 if an active event is found, 0 if no active event, -1 if
 * end of list
 */
int Binary_Event_Information(
    struct object_data *pObject,
    BACNET_OBJECT_TYPE Object_Type,
    uint32_t object_instance,
    BACNET_GET_EVENT_INFORMATION_DATA *getevent_data)
{
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;
    if (pObject) {
        /* Event_State not equal to NORMAL */
        IsActiveEvent = (pObject->Event_State != EVENT_STATE_NORMAL);

        /* Acked_Transitions property, which has at least one of the bits
           (TO-OFFNORMAL, TO-FAULT, TONORMAL) set to FALSE. */
        IsNotAckedTransitions =
            (pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked ==
             false) ||
            (pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
             false) ||
            (pObject->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked ==
             false);
    } else {
        return -1; /* end of list  */
    }
    if ((IsActiveEvent) || (IsNotAckedTransitions)) {
        /* Object Identifier */
        getevent_data->objectIdentifier.type = Object_Type;
        getevent_data->objectIdentifier.instance = object_instance;
        /* Event State */
        getevent_data->eventState = pObject->Event_State;
        /* Acknowledged Transitions */
        bitstring_init(&getevent_data->acknowledgedTransitions);
        bitstring_set_bit(
            &getevent_data->acknowledgedTransitions, TRANSITION_TO_OFFNORMAL,
            pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
        bitstring_set_bit(
            &getevent_data->acknowledgedTransitions, TRANSITION_TO_FAULT,
            pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
        bitstring_set_bit(
            &getevent_data->acknowledgedTransitions, TRANSITION_TO_NORMAL,
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
        bitstring_set_bit(
            &getevent_data->eventEnable, TRANSITION_TO_OFFNORMAL,
            (pObject->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true : false);
        bitstring_set_bit(
            &getevent_data->eventEnable, TRANSITION_TO_FAULT,
            (pObject->Event_Enable & EVENT_ENABLE_TO_FAULT) ? true : false);
        bitstring_set_bit(
            &getevent_data->eventEnable, TRANSITION_TO_NORMAL,
            (pObject->Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true : false);
        /* Event Priorities */
        Notification_Class_Get_Priorities(
            pObject->Notification_Class, getevent_data->eventPriorities);

        return 1; /* active event */
    } else {
        return 0; /* no active event at this index */
    }
}

/**
 * @brief Acknowledges the Event Information for the Analog Input Object
 * @param object pointer - struct object_data
 * @param alarmack_data - data for the Event Acknowledgement
 * @param error_code - error code for the Event Acknowledgement
 * @return 1 if successful, -1 if error, -2 if request is out-of-range
 */
int Binary_Alarm_Ack(
    struct object_data *pObject,
    BACNET_ALARM_ACK_DATA *alarmack_data,
    BACNET_ERROR_CODE *error_code)
{
    if (!alarmack_data) {
        return -1;
    }
    if (!pObject) {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -1;
    }
    switch (alarmack_data->eventStateAcked) {
        case EVENT_STATE_OFFNORMAL:
            if (pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL]
                    .bIsAcked == false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(
                        &pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL]
                             .Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                /* Send ack notification */
                pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked =
                    true;
            } else if (
                alarmack_data->eventStateAcked == pObject->Event_State) {
                /* Send ack notification */
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
                if (datetime_compare(
                        &pObject->Acked_Transitions[TRANSITION_TO_FAULT]
                             .Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                /* Send ack notification */
                pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                    true;
            } else if (
                alarmack_data->eventStateAcked == pObject->Event_State) {
                /* Send ack notification */
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
                if (datetime_compare(
                        &pObject->Acked_Transitions[TRANSITION_TO_NORMAL]
                             .Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                /* Send ack notification */
                pObject->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
                    true;
            } else if (
                alarmack_data->eventStateAcked == pObject->Event_State) {
                /* Send ack notification */
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

    return 1;
}

/**
 * @brief Handles getting the Alarm Summary for the Analog Input Object
 * @param  object pointer - struct object_data
 * @param  object_type - type of the object
 * @param  object_instance - object-instance number of the object
 * @param  getalarm_data - data for the Alarm Summary
 * @return 1 if an active alarm is found, 0 if no active alarm, -1 if
 * end of list
 */
int Binary_Alarm_Summary(
    struct object_data *pObject,
    BACNET_OBJECT_TYPE Object_Type,
    uint32_t object_instance,
    BACNET_GET_ALARM_SUMMARY_DATA *getalarm_data)
{

    if (getalarm_data == NULL) {
        debug_printf(
            "[%s %d]: NULL pointer parameter! getalarm_data = %p\r\n", __FILE__,
            __LINE__, (void *)getalarm_data);
        return -2;
    }

    if (pObject) {
        /* Event_State is not equal to NORMAL  and
           Notify_Type property value is ALARM */
        if ((pObject->Event_State != EVENT_STATE_NORMAL) &&
            (pObject->Notify_Type == NOTIFY_ALARM)) {
            /* Object Identifier */
            getalarm_data->objectIdentifier.type = Object_Type;
            getalarm_data->objectIdentifier.instance = object_instance;
            /* Alarm State */
            getalarm_data->alarmState = pObject->Event_State;
            /* Acknowledged Transitions */
            bitstring_init(&getalarm_data->acknowledgedTransitions);
            bitstring_set_bit(
                &getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_OFFNORMAL,
                pObject->Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
            bitstring_set_bit(
                &getalarm_data->acknowledgedTransitions, TRANSITION_TO_FAULT,
                pObject->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
            bitstring_set_bit(
                &getalarm_data->acknowledgedTransitions, TRANSITION_TO_NORMAL,
                pObject->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);
            return 1; /* active alarm */
        } else {
            return 0; /* no active alarm at this index */
        }
    } else {
        return -1; /* end of list  */
    }
}

#endif
/**
 * For a given object instance-number, returns the Acked Transitions
 *
 * @param  object pointer - struct object_data
 * @param  value - acked_info struct
 *
 * @return true
 */
bool Binary_Acked_Transitions(struct object_data *pObject, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION])
{
    uint8_t b = 0;

    if (pObject) {
        for (b = 0; b < MAX_BACNET_EVENT_TRANSITION; b++) {
            value[b] = &pObject->Acked_Transitions[b];
        }
        return true;
    } else
        return false;
}

/**
 * For a given object instance-number, sets the present-value
 *
 * @param  object pointer - struct object_data
 * @param  value - enumerated binary present-value
 * @param  priority - priority-array index value 1..16
 * @return  true if values are within range and present-value is set.
 */
bool Binary_Present_Value_Set(
    struct object_data *pObject, BACNET_BINARY_PV value, unsigned priority)
{
    bool status = false;
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = false;
            pObject->Priority_Array[priority - 1] = value;
            Binary_COV_Detect(
                pObject, Binary_Present_Value(pObject));
            status = true;
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, writes the present-value to the
 * remote node
 * @param  object pointer - struct object_data
 * @param  value - floating point analog value
 * @param  priority - priority-array index value 1..16
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 * @return  true if values are within range and present-value is set.
 */
bool Binary_Present_Value_Write(
    struct object_data *pObject, BACNET_BINARY_PV value, uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;

    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            if (priority != 6) {
                Binary_Present_Value_Set(pObject, value, priority);
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
 * @brief For a given object instance-number, relinquishes the present-value
 * @param  object pointer - struct object_data
 * @param  priority - priority-array index value 1..16
 * @return  true if values are within range and present-value is relinquished.
 */
bool Binary_Present_Value_Relinquish(
    struct object_data *pObject, unsigned priority)
{
    bool status = false;
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = true;
            pObject->Priority_Array[priority - 1] = false;
            Binary_COV_Detect(
                pObject, Binary_Present_Value(pObject));
            status = true;
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, writes the present-value to the
 * remote node
 * @param  object pointer - struct object_data
 * @param  priority - priority-array index value 1..16
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 * @return  true if values are within range and write is requested
 */
bool Binary_Present_Value_Relinquish_Write(
    struct object_data *pObject, uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;

    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            if (priority != 6) {
                Binary_Present_Value_Relinquish(pObject, priority);
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
 * @brief For a given object instance-number, sets the out-of-service property
 * value
 * @param object pointer - struct object_data
 * @param value - boolean out-of-service value
 * @return true if the out-of-service property value was set
 */
void Binary_Out_Of_Service_Set(struct object_data *pObject, bool value)
{
    if (pObject) {
        if (pObject->Out_Of_Service != value) {
            pObject->Out_Of_Service = value;
            pObject->Changed = true;
        }
    }
}

/**
 * For a given object instance-number, sets the object-name
 *
 * @param  object pointer - struct object_data
 * @param  new_name - holds the object-name to be set
 *
 * @return  true if object-name was set
 */
bool Binary_Name_Set(
    struct object_data *pObject,
    const char *new_name,
    BACNET_OBJECT_TYPE Object_Type,
    uint32_t object_instance)
{
    bool status = false;
    BACNET_CHARACTER_STRING object_name;
    BACNET_OBJECT_TYPE found_type = 0;
    uint32_t found_instance = 0;
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
 * @brief For a given object instance-number, sets the reliability
 * @param  object pointer - struct object_data
 * @param  value - reliability property value
 * @return  true if the reliability property value was set
 */
bool Binary_Reliability_Set(
    struct object_data *pObject, BACNET_RELIABILITY value)
{
    bool status = false;
    bool fault = false;
    if (pObject) {
        fault = Binary_Object_Fault(pObject);
        pObject->Reliability = value;
        if (fault != Binary_Object_Fault(pObject)) {
            pObject->Changed = true;
        }
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, sets the overridden status flag
 * @param object pointer - struct object_data
 * @param value - boolean out-of-service value
 * @return true if the overridden status flag was set
 */
void Binary_Overridden_Set(struct object_data *pObject, bool value)
{
    if (pObject) {
        if (pObject->Overridden != value) {
            pObject->Overridden = value;
            pObject->Changed = true;
        }
    }
}

#if defined(INTRINSIC_REPORTING)
/**
 * @brief For a given object instance-number, sets the High Limit
 * @param  object pointer - struct object_data
 * @param  value - value to be set
 * @return true if valid object-instance and value within range
 */
bool Binary_Alarm_Value_Set(struct object_data *pObject, BACNET_BINARY_PV value)
{
    bool status = false;

    if (pObject) {
        if (pObject->Polarity != POLARITY_NORMAL) {
            value =
                (value == BINARY_INACTIVE) ? BINARY_ACTIVE : BINARY_INACTIVE;
        }
        pObject->Alarm_Value = value;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object reset events
 * @param object pointer - struct object_data
 * @return void
 */
void Binary_Reset_Event_Properties(struct object_data *pObject)
{
    unsigned j;
    /* initialize Event time stamps using wildcards
            and set Acked_transitions */
    for (j = 0; j < MAX_BACNET_EVENT_TRANSITION; j++) {
        datetime_wildcard_set(&pObject->Event_Time_Stamps[j]);
        pObject->Acked_Transitions[j].bIsAcked = true;
        pObject->Event_Message_Texts[j] = NULL;
    }
    pObject->Event_State = EVENT_STATE_NORMAL;
    pObject->Last_ToFault_Event_Reliability = RELIABILITY_NO_FAULT_DETECTED;
}

/**
 * For a given object instance-number, sets the event-detection-enable property
 * value
 *
 * @param object pointer - struct object_data
 *
 * @return event-detection-enable property value
 */
bool Binary_Event_Detection_Enable_Set(
    struct object_data *pObject, bool value)
{
    bool retval = false;

    if (pObject) {
        pObject->Event_Detection_Enable = value;
        if (!pObject->Event_Detection_Enable) {
            /*When this property is FALSE, Event_State shall be NORMAL, and the
            properties Acked_Transitions, Event_Time_Stamps, and
            Event_Message_Texts shall be equal to their respective initial
            conditions.*/
            Binary_Reset_Event_Properties(pObject);
        }
        retval = true;
    }

    return retval;
}

#endif //INTRINSIC_REPORTING
