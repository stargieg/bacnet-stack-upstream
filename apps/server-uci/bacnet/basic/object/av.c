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
#include "bacnet/bactext.h"
#include "bacnet/datetime.h"
#include "bacnet/proplist.h"
#include "bacnet/timestamp.h"
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
#include "bacnet/basic/object/av.h"

static const char *sec = "bacnet_av";
static const char *type = "av";

struct object_data {
    bool Out_Of_Service : 1;
    bool Overridden : 1;
    bool Changed : 1;
    float COV_Increment;
    float Prior_Value;
    bool Relinquished[BACNET_MAX_PRIORITY];
    float Priority_Array[BACNET_MAX_PRIORITY];
    float Relinquish_Default;
    float Min_Pres_Value;
    float Max_Pres_Value;
    float Resolution;
    uint16_t Units;
    uint8_t Reliability;
    const char *Object_Name;
    const char *Description;
    void *Context;
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State:3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    float High_Limit;
    float Low_Limit;
    float Deadband;
    unsigned Limit_Enable:2;
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
    const char *COV_Increment;
    const char *Prior_Value;
    const char *Relinquish_Default;
    const char *Min_Pres_Value;
    const char *Max_Pres_Value;
    const char *Resolution;
    uint16_t Units;
    uint8_t Reliability;
    const char *Object_Name;
    const char *Description;
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State:3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    const char *High_Limit;
    const char *Low_Limit;
    const char *Deadband;
    unsigned Limit_Enable:2;
    unsigned Event_Enable:3;
    unsigned Event_Detection_Enable : 1;
    unsigned Notify_Type:1;
#endif /* INTRINSIC_REPORTING */
};

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_ANALOG_VALUE;
/* callback for present value writes */
static analog_value_write_present_value_callback
    Analog_Value_Write_Present_Value_Callback;

/* clang-format off */
/* These three arrays are used by the ReadPropertyMultiple handler */
static const int32_t Analog_Value_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER, PROP_OBJECT_NAME, PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE, PROP_STATUS_FLAGS, PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE, PROP_UNITS, PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
#if (BACNET_PROTOCOL_REVISION >= 17)
    PROP_CURRENT_COMMAND_PRIORITY,
#endif
    -1
};

static const int32_t Analog_Value_Properties_Optional[] = {
    PROP_DESCRIPTION, PROP_RELIABILITY, PROP_COV_INCREMENT,
    PROP_MIN_PRES_VALUE, PROP_MAX_PRES_VALUE, PROP_RESOLUTION,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY, PROP_NOTIFICATION_CLASS, PROP_HIGH_LIMIT,
    PROP_LOW_LIMIT, PROP_DEADBAND, PROP_LIMIT_ENABLE, PROP_EVENT_ENABLE,
    PROP_ACKED_TRANSITIONS, PROP_NOTIFY_TYPE, PROP_EVENT_TIME_STAMPS,
    PROP_EVENT_DETECTION_ENABLE, PROP_EVENT_MESSAGE_TEXTS,
#endif
    -1
};

static const int32_t Analog_Value_Properties_Proprietary[] = {
    -1
};

/**
 * @brief Set zero depends on resolution / precision
 * @param value_f - Value
 * @param resolution - Resolution
 * @return rounded value
 */
static float limit_value_by_resolution(float value_f, float resolution) {
    float ret = 0.0;
    float prec = 0.0;
    if (resolution < 1) {
        prec = roundf(1 / resolution);
        ret = roundf(value_f * prec);
        ret = ret / prec;
    } else {
        ret = roundf(value_f / resolution);
        ret = ret * resolution;
    }
    return ret;
}


/**
 * @brief snprintf with resolution / precision
 * @param value_c - Value
 * @param value_c_len - Value Len
 * @param resolution - Resolution
 * @param value_f - Real Value
 * @return rounded value
 */
static int snprintf_res(char *value_c, int value_c_len, float resolution, float value_f) {
    int ret = 0;
    int prec = 0;
    if (resolution < 1) {
        prec = (int)log10(roundf(1 / resolution));
        ret = snprintf(value_c, value_c_len, "%.*f", prec, value_f);
    } else {
        ret = snprintf(value_c, value_c_len, "%i", (int)value_f);
    }
    return ret;
}

/* clang-format on */

/**
 * Initialize the pointers for the required, the optional and the properitary
 * value properties.
 *
 * @param pRequired - Pointer to the pointer of required values.
 * @param pOptional - Pointer to the pointer of optional values.
 * @param pProprietary - Pointer to the pointer of properitary values.
 */
void Analog_Value_Property_Lists(
    const int32_t **pRequired,
    const int32_t **pOptional,
    const int32_t **pProprietary)
{
    if (pRequired) {
        *pRequired = Analog_Value_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Analog_Value_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Analog_Value_Properties_Proprietary;
    }

    return;
}

/**
* Analog_Value_Object() replaced by
* Keylist_Data(Object_List, object_instance)
* 
* Analog_Value_Object_Index() replaced by
* Keylist_Data(Object_List, Analog_Value_Index_To_Instance(index)
* 
*/
#if 0
/**
 * @brief Gets an object from the list using an instance number as the key
 * @param  object_instance - object-instance number of the object
 * @return object found in the list, or NULL if not found
 */
static struct analog_value_descr *Analog_Value_Object(uint32_t object_instance)
{
    return Keylist_Data(Object_List, object_instance);
}

#if defined(INTRINSIC_REPORTING)
/**
 * @brief Gets an object from the list using its index in the list
 * @param index - index of the object in the list
 * @return object found in the list, or NULL if not found
 */
static struct analog_value_descr *Analog_Value_Object_Index(int index)
{
    return Keylist_Data_Index(Object_List, index);
}
#endif
#endif

/**
 * @brief Determines if a given object instance is valid
 * @param  object_instance - object-instance number of the object
 * @return  true if the instance is valid, and false if not
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
 */
unsigned Analog_Value_Count(void)
{
    return Keylist_Count(Object_List)-1;
}

/**
 * @brief Determines the object instance-number for a given 0..(N-1) index
 * of objects where N is Analog_Value_Count().
 * @param  index - 0..(N-1) where N is Analog_Value_Count().
 * @return  object instance-number for the given index
 */
uint32_t Analog_Value_Index_To_Instance(unsigned index)
{
    KEY key = UINT32_MAX;

    Keylist_Index_Key(Object_List, index, &key);

    return key;
}

/**
 * @brief For a given object instance-number, determines a 0..(N-1) index
 * of objects where N is Analog_Value_Count().
 * @param  object_instance - object-instance number of the object
 * @return  index for the given instance-number, or >= Analog_Value_Count()
 * if not valid.
 */
unsigned Analog_Value_Instance_To_Index(uint32_t object_instance)
{
    return Keylist_Index(Object_List, object_instance);
}

/**
 * @brief For a given object instance-number, determines the present value.
 * @param  object_instance - object-instance number of the object
 * @return  present-value of the object
 */
float Analog_Value_Present_Value(uint32_t object_instance)
{
    float value = 0.0f;
    uint8_t priority = 0; /* loop counter */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = Analog_Value_Relinquish_Default(object_instance);
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
 * This function is used to detect a value change,
 * using the new value compared against the prior
 * value, using a delta as threshold.
 *
 * This method will update the COV-changed attribute.
 *
 * @param index  Object index
 * @param value  Given present value.
 */
static void
Analog_Value_COV_Detect(struct object_data *pObject, float value)
{
    float prior_value = 0.0f;
    float cov_increment = 0.0f;
    float cov_delta = 0.0f;

    if (pObject) {
        prior_value = pObject->Prior_Value;
        cov_increment = pObject->COV_Increment;
        if (prior_value > value) {
            cov_delta = prior_value - value;
        } else {
            cov_delta = value - prior_value;
        }
        if (cov_delta >= cov_increment) {
            pObject->Changed = true;
            pObject->Prior_Value = value;
        }
    }
}

/**
 * For a given object instance-number, sets the present-value at a given
 * priority 1..16.
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point analog value
 * @param  priority - priority 1..16
 *
 * @return  true if values are within range and present-value is set.
 */
bool Analog_Value_Present_Value_Set(
    uint32_t object_instance, float value, uint8_t priority)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = false;
            pObject->Priority_Array[priority - 1] = value;
            Analog_Value_COV_Detect(
                pObject, Analog_Value_Present_Value(object_instance));
            status = true;
        }
    }

    return status;
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
 * For a given object instance-number, sets the object-name
 *
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the object-name to be set
 *
 * @return  true if object-name was set
 */
bool Analog_Value_Name_Set(uint32_t object_instance, const char *new_name)
{
    bool status = false;
    BACNET_CHARACTER_STRING object_name;
    BACNET_OBJECT_TYPE found_type = 0;
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
const char *Analog_Value_Name_ASCII(uint32_t object_instance)
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
 * For a given object instance-number, gets the event-state property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  event-state property value
 */
unsigned Analog_Value_Event_State(uint32_t object_instance)
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
 * @brief For a given object instance-number, returns the description
 * @param  object_instance - object-instance number of the object
 * @return description text or NULL if not found
 */
const char *Analog_Value_Description(uint32_t object_instance)
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
 * @brief For a given object instance-number, sets the description
 * @param  object_instance - object-instance number of the object
 * @param  new_name - holds the description to be set
 * @return  true if object-name was set
 */
bool Analog_Value_Description_Set(
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
 * @brief For a given object instance-number, returns the reliability
 * @param  object_instance - object-instance number of the object
 * @return reliability property value
 */
BACNET_RELIABILITY Analog_Value_Reliability(uint32_t object_instance)
{
    BACNET_RELIABILITY value = RELIABILITY_NO_FAULT_DETECTED;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = (BACNET_RELIABILITY)pObject->Reliability;
    }

    return value;
}

/**
 * @brief For a given object instance-number, gets the Fault status flag
 * @param  object_instance - object-instance number of the object
 * @return  true the status flag is in Fault
 */
static bool Analog_Value_Object_Fault(struct object_data *pObject)
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
 * @param  value - reliability property value
 * @return  true if the reliability property value was set
 */
bool Analog_Value_Reliability_Set(
    uint32_t object_instance, BACNET_RELIABILITY value)
{
    bool status = false;
    bool fault = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (value <= 255) {
            fault = Analog_Value_Object_Fault(pObject);
            pObject->Reliability = value;
            if (fault != Analog_Value_Object_Fault(pObject)) {
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
static bool Analog_Value_Fault(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);

    return Analog_Value_Object_Fault(pObject);
}

/**
 * @brief For a given object instance-number, determines the COV status
 * @param  object_instance - object-instance number of the object
 * @return  true if the COV flag is set
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
 */
bool Analog_Value_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list)
{
    bool status = false;
    bool in_alarm = false;
    bool out_of_service = false;
    bool fault = false;
    bool overridden = false;
    float present_value = 0.0f;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (Analog_Value_Event_State(object_instance) != EVENT_STATE_NORMAL) {
            in_alarm = true;
        }
        if (Analog_Value_Object_Fault(pObject)) {
            fault = true;
        }
        out_of_service = pObject->Out_Of_Service;
        present_value = pObject->Prior_Value;
        overridden = pObject->Overridden;
        status = cov_value_list_encode_real(
            value_list, present_value, in_alarm, fault, overridden,
            out_of_service);
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the COV-Increment value
 * @param  object_instance - object-instance number of the object
 * @return  COV-Increment value
 */
float Analog_Value_COV_Increment(uint32_t object_instance)
{
    float value = 0.0f;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->COV_Increment;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the COV-Increment value
 * @param  object_instance - object-instance number of the object
 * @param  value - COV-Increment value
 */
void Analog_Value_COV_Increment_Set(uint32_t object_instance, float value)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        pObject->COV_Increment = value;
        Analog_Value_COV_Detect(pObject, pObject->Prior_Value);
    }
}

/**
 * For a given object instance-number, returns the units property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  units property value
 */
BACNET_ENGINEERING_UNITS Analog_Value_Units(uint32_t object_instance)
{
    BACNET_ENGINEERING_UNITS units = UNITS_NO_UNITS;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        units = pObject->Units;
    }

    return units;
}

/**
 * For a given object instance-number, sets the units property value
 *
 * @param object_instance - object-instance number of the object
 * @param units - units property value
 *
 * @return true if the units property value was set
 */
bool Analog_Value_Units_Set(uint32_t object_instance, BACNET_ENGINEERING_UNITS units)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Units = units;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the out-of-service
 * property value
 * @param object_instance - object-instance number of the object
 * @return out-of-service property value
 */
bool Analog_Value_Out_Of_Service(uint32_t object_instance)
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
 * @brief For a given object instance-number, sets the out-of-service property
 * value
 * @param object_instance - object-instance number of the object
 * @param value - boolean out-of-service value
 * @return true if the out-of-service property value was set
 */
void Analog_Value_Out_Of_Service_Set(uint32_t object_instance, bool value)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Out_Of_Service != value) {
            pObject->Changed = true;
        }
        pObject->Out_Of_Service = value;
    }
}

#if defined(INTRINSIC_REPORTING)
/**
 * For a given object instance-number, returns the units property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  time delay property value
 */
uint32_t Analog_Value_Time_Delay(uint32_t object_instance)
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
bool Analog_Value_Time_Delay_Set(uint32_t object_instance, uint32_t value)
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
uint32_t Analog_Value_Notification_Class(uint32_t object_instance)
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
bool Analog_Value_Notification_Class_Set(uint32_t object_instance, uint32_t value)
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
float Analog_Value_High_Limit(uint32_t object_instance)
{
    float value = 100.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->High_Limit;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the High Limit
 * @param  object_instance - object-instance number of the object
 * @param  value - value to be set
 * @return true if valid object-instance and value within range
 */
bool Analog_Value_High_Limit_Set(uint32_t object_instance, float value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        pObject->High_Limit = value;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the Low Limit
 * @param  object_instance - object-instance number of the object
 * @return value or 0.0 if not found
 */
float Analog_Value_Low_Limit(uint32_t object_instance)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Low_Limit;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the Low Limit
 * @param  object_instance - object-instance number of the object
 * @param  value - value to be set
 * @return true if valid object-instance and value within range
 */
bool Analog_Value_Low_Limit_Set(uint32_t object_instance, float value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        pObject->Low_Limit = value;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the Deadband
 * @param  object_instance - object-instance number of the object
 * @return value or 0.0 if not found
 */
float Analog_Value_Deadband(uint32_t object_instance)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Deadband;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the Deadband
 * @param  object_instance - object-instance number of the object
 * @param  value - value to be set
 * @return true if valid object-instance and value within range
 */
bool Analog_Value_Deadband_Set(uint32_t object_instance, float value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        pObject->Deadband = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, returns the Limit Enable value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  Limit Enable value
 */
BACNET_LIMIT_ENABLE Analog_Value_Limit_Enable(uint32_t object_instance)
{
    uint8_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Limit_Enable;
    }

    return value;
}

/**
 * For a given object instance-number, sets the Limit Enable value
 *
 * @param object_instance - object-instance number of the object
 * @param value - Limit Enable value
 *
 * @return true if the Limit Enable value was set
 */
bool Analog_Value_Limit_Enable_Set(uint32_t object_instance, uint8_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Limit_Enable = value;
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
uint8_t Analog_Value_Event_Enable(uint32_t object_instance)
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
bool Analog_Value_Event_Enable_Set(uint32_t object_instance, uint8_t value)
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
bool Analog_Value_Acked_Transitions(uint32_t object_instance, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION])
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
BACNET_NOTIFY_TYPE Analog_Value_Notify_Type(uint32_t object_instance)
{
    BACNET_NOTIFY_TYPE value = 0;
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
bool Analog_Value_Notify_Type_Set(uint32_t object_instance, BACNET_NOTIFY_TYPE value)
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
Analog_Value_Reset_Event_Properties(uint32_t object_instance)
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
 * For a given object instance-number, gets the event-detection-enable property
 * value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  event-detection-enable property value
 */
bool Analog_Value_Event_Detection_Enable(uint32_t object_instance)
{
    bool retval = false;
#if !defined(INTRINSIC_REPORTING)
    (void)object_instance;
#else
    struct object_data *pObject = Keylist_Data(Object_List, object_instance);

    if (pObject) {
        retval = pObject->Event_Detection_Enable;
    }
#endif

    return retval;
}

/**
 * For a given object instance-number, sets the event-detection-enable property
 * value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  event-detection-enable property value
 */
bool Analog_Value_Event_Detection_Enable_Set(
    uint32_t object_instance, bool value)
{
    bool retval = false;
#if !defined(INTRINSIC_REPORTING)
    (void)object_instance;
    (void)value;
#else
    struct object_data *pObject = Keylist_Data(Object_List, object_instance);

    if (pObject) {
        pObject->Event_Detection_Enable = value;
        if (!pObject->Event_Detection_Enable) {
            /*When this property is FALSE, Event_State shall be NORMAL, and the
            properties Acked_Transitions, Event_Time_Stamps, and
            Event_Message_Texts shall be equal to their respective initial
            conditions.*/
            Analog_Value_Reset_Event_Properties(object_instance);
        }
        retval = true;
    }
#endif

    return retval;
}

/**
 * @brief For a given object instance-number and event transition, returns the
 * event message text
 * @param  object_instance - object-instance number of the object
 * @param  transition - transition type
 * @return event message text or NULL if object not found or transition invalid
 */
const char *Analog_Value_Event_Message_Text(
    const uint32_t object_instance,
    const enum BACnetEventTransitionBits transition)
{
    const char *text = NULL;
    const struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && transition < MAX_BACNET_EVENT_TRANSITION) {
        text = pObject->Event_Message_Texts[transition];
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

bool Analog_Value_Event_Message_Text_Custom_Set(
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
 * @brief Encode a EventTimeStamps property element
 * @param object_instance [in] BACnet network port object instance number
 * @param index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Analog_Value_Event_Time_Stamps_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX index, uint8_t *apdu)
{
    int apdu_len = 0, len = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
 * @param object_instance [in] object instance number
 * @param index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Analog_Value_Event_Message_Texts_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX index, uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    const char *text = NULL; /* return value */
    BACNET_CHARACTER_STRING char_string = { 0 };

    text = Analog_Value_Event_Message_Text(object_instance, index);
    if (text) {
        characterstring_init_ansi(&char_string, text);
        apdu_len = encode_application_character_string(apdu, &char_string);
    }

    return apdu_len;
}
#endif

/**
 * @brief For a given object instance-number, determines the priority
 * @param  object_instance - object-instance number of the object
 * @return  active priority 1..16, or 0 if no priority is active
 */
unsigned Analog_Value_Present_Value_Priority(
    uint32_t object_instance)
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
 * @brief Encode a BACnetARRAY property element
 * @param object_instance [in] BACnet network port object instance number
 * @param index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
static int Analog_Value_Priority_Array_Encode(
    uint32_t object_instance, BACNET_ARRAY_INDEX index, uint8_t *apdu)
{
    int apdu_len = BACNET_STATUS_ERROR;
    struct object_data *pObject;
    float real_value;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && (index < BACNET_MAX_PRIORITY)) {
        if (pObject->Relinquished[index]) {
            apdu_len = encode_application_null(apdu);
        } else {
            real_value = pObject->Priority_Array[index];
            apdu_len = encode_application_real(apdu, real_value);
        }
    }

    return apdu_len;
}

/**
 * For a given object instance-number, determines the relinquish-default value
 *
 * @param object_instance - object-instance number
 *
 * @return relinquish-default value of the object
 */
float Analog_Value_Relinquish_Default(uint32_t object_instance)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Relinquish_Default;
    }

    return value;
}

/**
 * For a given object instance-number, sets the relinquish-default value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point analog output relinquish-default value
 *
 * @return  true if values are within range and relinquish-default value is set.
 */
bool Analog_Value_Relinquish_Default_Set(uint32_t object_instance, float value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        pObject->Relinquish_Default = value;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, relinquishes the present-value
 * @param  object_instance - object-instance number of the object
 * @param  priority - priority-array index value 1..16
 * @return  true if values are within range and present-value is relinquished.
 */
bool Analog_Value_Present_Value_Relinquish(
    uint32_t object_instance, unsigned priority)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = true;
            pObject->Priority_Array[priority - 1] = 0.0;
            Analog_Value_COV_Detect(
                pObject, Analog_Value_Present_Value(object_instance));
            status = true;
        }
    }

    return status;
}

/**
 * @brief For a given object instance-number, writes the present-value to the
 * remote node
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point analog value
 * @param  priority - priority-array index value 1..16
 * @param  error_class - the BACnet error class
 * @param  error_code - BACnet Error code
 * @return  true if values are within range and present-value is set.
 */
static bool Analog_Value_Present_Value_Write(
    uint32_t object_instance, float value, uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;
    struct object_data *pObject;
    float old_value = 0.0;
    float new_value = 0.0;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY) &&
            (value >= pObject->Min_Pres_Value) && (value <= pObject->Max_Pres_Value)) {
            if (priority != 6) {
                old_value = Analog_Value_Present_Value(object_instance);
                Analog_Value_Present_Value_Set(object_instance, value, priority);
                if (pObject->Out_Of_Service) {
                    /* The physical point that the object represents
                        is not in service. This means that changes to the
                        Present_Value property are decoupled from the
                        physical output when the value of Out_Of_Service
                        is true. */
                } else if (Analog_Value_Write_Present_Value_Callback) {
                    new_value = Analog_Value_Present_Value(object_instance);
                    Analog_Value_Write_Present_Value_Callback(
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
static bool Analog_Value_Present_Value_Relinquish_Write(
    uint32_t object_instance, uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    bool status = false;
    struct object_data *pObject;
    float old_value = 0.0;
    float new_value = 0.0;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            if (priority != 6) {
                old_value = Analog_Value_Present_Value(object_instance);
                Analog_Value_Present_Value_Relinquish(object_instance, priority);
                if (pObject->Out_Of_Service) {
                    /* The physical point that the object represents
                        is not in service. This means that changes to the
                        Present_Value property are decoupled from the
                        physical output when the value of Out_Of_Service
                        is true. */
                } else if (Analog_Value_Write_Present_Value_Callback) {
                    new_value = Analog_Value_Present_Value(object_instance);
                    Analog_Value_Write_Present_Value_Callback(
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
 * @brief For a given object instance-number, returns the overridden
 * status flag value
 * @param  object_instance - object-instance number of the object
 * @return  out-of-service property value
 */
bool Analog_Value_Overridden(uint32_t object_instance)
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
void Analog_Value_Overridden_Set(uint32_t object_instance, bool value)
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
 * @brief For a given object instance-number, returns the min-pres-value
 * @param  object_instance - object-instance number of the object
 * @return value or 0.0 if not found
 */
float Analog_Value_Min_Pres_Value(uint32_t object_instance)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Min_Pres_Value;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the min-pres-value
 * @param  object_instance - object-instance number of the object
 * @param  value - value to be set
 * @return true if valid object-instance and value within range
 */
bool Analog_Value_Min_Pres_Value_Set(uint32_t object_instance, float value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        pObject->Min_Pres_Value = value;
        status = true;
    }

    return status;
}

/**
 * @brief For a given object instance-number, returns the max-pres-value
 * @param  object_instance - object-instance number of the object
 * @return value or 0.0 if not found
 */
float Analog_Value_Max_Pres_Value(uint32_t object_instance)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Max_Pres_Value;
    }

    return value;
}

/**
 * @brief For a given object instance-number, sets the max-pres-value
 * @param  object_instance - object-instance number of the object
 * @param  value - value to be set
 * @return true if valid object-instance and value within range
 */
bool Analog_Value_Max_Pres_Value_Set(uint32_t object_instance, float value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        pObject->Max_Pres_Value = value;
        status = true;
    }

    return status;
}


/**
 * @brief Get the Resolution
 * @param object_instance - object-instance number of the object
 * @return the Resolution
 */
float Analog_Value_Resolution(uint32_t object_instance)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Resolution;
    }

    return value;
}

/**
 * @brief Set the Resolution
 * @param object_instance - object-instance number of the object
 * @param value - Resolution value to set
 * @return true if valid object-instance and value within range
 */
bool Analog_Value_Resolution_Set(uint32_t object_instance, float value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Resolution = value;
        status = true;
    }
    return status;
}

/**
 * @brief For a given object instance-number, handles the ReadProperty service
 * @param rpdata  Property requested, see for BACNET_READ_PROPERTY_DATA details.
 * @return apdu len, or BACNET_STATUS_ERROR on error
 */
int Analog_Value_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    float real_value = 0.0;
    uint8_t *apdu = NULL;
    BACNET_ENGINEERING_UNITS units = 0;
    unsigned i = 0;
    bool state = false;
#if defined(INTRINSIC_REPORTING)
    int apdu_size = 0;
    ACKED_INFO *ack_info[MAX_BACNET_EVENT_TRANSITION] = { 0 };
#else
    // for PROP_PRIORITY_ARRAY
    int apdu_size = 0;
#endif

    /* Valid data? */
    if (rpdata == NULL) {
        return 0;
    }
    if ((rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;
#if defined(INTRINSIC_REPORTING)
    apdu_size = rpdata->application_data_len;
#else
    // for PROP_PRIORITY_ARRAY
    apdu_size = rpdata->application_data_len;
#endif
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], Object_Type, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            if (Analog_Value_Object_Name(
                    rpdata->object_instance, &char_string)) {
                apdu_len =
                    encode_application_character_string(&apdu[0], &char_string);
            }
            break;
        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(&apdu[0], Object_Type);
            break;
        case PROP_PRESENT_VALUE:
            real_value = Analog_Value_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_MIN_PRES_VALUE:
            real_value = Analog_Value_Min_Pres_Value(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_MAX_PRES_VALUE:
            real_value = Analog_Value_Max_Pres_Value(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            bitstring_set_bit(
                &bit_string, STATUS_FLAG_IN_ALARM,
                Analog_Value_Event_State(rpdata->object_instance) != EVENT_STATE_NORMAL);
            state = Analog_Value_Fault(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, state);
            state = Analog_Value_Overridden(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, state);
            state = Analog_Value_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_EVENT_STATE:
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0], Analog_Value_Event_State(rpdata->object_instance));
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;
        case PROP_RELIABILITY:
            apdu_len =
                encode_application_enumerated(&apdu[0], Analog_Value_Reliability(rpdata->object_instance));
            break;
        case PROP_OUT_OF_SERVICE:
            state = Analog_Value_Out_Of_Service(rpdata->object_instance);
            apdu_len =
                encode_application_boolean(&apdu[0], state);
            break;
        case PROP_UNITS:
            units = Analog_Value_Units(rpdata->object_instance);
            apdu_len =
                encode_application_enumerated(&apdu[0], units);
            break;
        case PROP_PRIORITY_ARRAY:
            apdu_len = bacnet_array_encode(
                rpdata->object_instance, rpdata->array_index,
                Analog_Value_Priority_Array_Encode, BACNET_MAX_PRIORITY, apdu,
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
            real_value =
                Analog_Value_Relinquish_Default(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_DESCRIPTION:
            characterstring_init_ansi(
                &char_string,
                Analog_Value_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_COV_INCREMENT:
            apdu_len =
                encode_application_real(&apdu[0], Analog_Value_COV_Increment(rpdata->object_instance));
            break;
        case PROP_RESOLUTION:
            apdu_len = encode_application_real(
                &apdu[0], Analog_Value_Resolution(rpdata->object_instance));
            break;
#if (BACNET_PROTOCOL_REVISION >= 17)
        case PROP_CURRENT_COMMAND_PRIORITY:
            i = Analog_Value_Present_Value_Priority(rpdata->object_instance);
            if ((i >= BACNET_MIN_PRIORITY) && (i <= BACNET_MAX_PRIORITY)) {
                apdu_len = encode_application_unsigned(&apdu[0], i);
            } else {
                apdu_len = encode_application_null(&apdu[0]);
            }
            break;
#endif
#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            i = Analog_Value_Time_Delay(rpdata->object_instance);
            apdu_len =
                encode_application_unsigned(&apdu[0], i);
            break;
        case PROP_NOTIFICATION_CLASS:
            i = Analog_Value_Notification_Class(rpdata->object_instance);
            apdu_len = encode_application_unsigned(
                &apdu[0], i);
            break;
        case PROP_HIGH_LIMIT:
            real_value = Analog_Value_High_Limit(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_LOW_LIMIT:
            real_value = Analog_Value_Low_Limit(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_DEADBAND:
            real_value = Analog_Value_Deadband(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_LIMIT_ENABLE:
            i = Analog_Value_Limit_Enable(rpdata->object_instance);
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
            i = Analog_Value_Event_Enable(rpdata->object_instance);
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
            state = Analog_Value_Event_Detection_Enable(rpdata->object_instance);
            apdu_len = encode_application_boolean(
                &apdu[0], state );
            break;
        case PROP_ACKED_TRANSITIONS:
            state = Analog_Value_Acked_Transitions(rpdata->object_instance, ack_info);
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
            i = Analog_Value_Notify_Type(rpdata->object_instance);
            apdu_len = encode_application_enumerated(
                &apdu[0], i ? NOTIFY_EVENT : NOTIFY_ALARM);
            break;
        case PROP_EVENT_TIME_STAMPS:
            apdu_len = bacnet_array_encode(
                rpdata->object_instance, rpdata->array_index,
                Analog_Value_Event_Time_Stamps_Encode,
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
            apdu_len = bacnet_array_encode(
                rpdata->object_instance, rpdata->array_index,
                Analog_Value_Event_Message_Texts_Encode,
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
 */
bool Analog_Value_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value = { 0 };
    struct uci_context *ctxw = NULL;
    char *idx_c = NULL;
    int idx_c_len = 0;
    float value_f = 0.0;
    float resolution = 0.1;
    char *value_c = NULL;
    int value_c_len = 0;

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
    ctxw = ucix_init(sec);
    if (!ctxw)
        fprintf(stderr, "Failed to load config file %s\n",sec);
    idx_c_len = snprintf(NULL, 0, "%d", wp_data->object_instance);
    idx_c = malloc(idx_c_len + 1);
    snprintf(idx_c,idx_c_len + 1,"%d",wp_data->object_instance);
    resolution = Analog_Value_Resolution(wp_data->object_instance);
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Value_Present_Value_Write(wp_data->object_instance,
                    value.type.Real, wp_data->priority,
                    &wp_data->error_class, &wp_data->error_code)) {
                    value_f = Analog_Value_Present_Value(wp_data->object_instance);
                    value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                    value_c = malloc(value_c_len + 1);
                    snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                    ucix_add_option(ctxw, sec, idx_c, "value", value_c);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_NULL);
                if (status) {
                    if (Analog_Value_Present_Value_Relinquish_Write(
                        wp_data->object_instance, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code)) {
                        value_f = Analog_Value_Present_Value(wp_data->object_instance);
                        value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                        value_c = malloc(value_c_len + 1);
                        snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                        ucix_add_option(ctxw, sec, idx_c, "value", value_c);
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
                Analog_Value_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_UNITS:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                if (Analog_Value_Units_Set(
                    wp_data->object_instance, value.type.Enumerated)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "units",
                        Analog_Value_Units(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                } else {
                    status = false;
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            }
            break;
        case PROP_COV_INCREMENT:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_REAL);
            if (status) {
                Analog_Value_COV_Increment_Set(wp_data->object_instance,
                value.type.Real);
                value_f = Analog_Value_COV_Increment(wp_data->object_instance);
                value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                value_c = malloc(value_c_len + 1);
                snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                ucix_add_option(ctxw, sec, idx_c, "cov_increment", value_c);
                ucix_commit(ctxw,sec);
                free(value_c);
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
                if (Analog_Value_Name_Set(
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
        case PROP_RELIABILITY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status)
                Analog_Value_Reliability_Set(wp_data->object_instance,
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
                Analog_Value_Relinquish_Default_Set(wp_data->object_instance,
                    value.type.Real);
            break;
        case PROP_MAX_PRES_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Value_Max_Pres_Value_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Value_Max_Pres_Value(wp_data->object_instance);
                    value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                    value_c = malloc(value_c_len + 1);
                    snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                    ucix_add_option(ctxw, sec, idx_c, "max_value", value_c);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            }
            break;
        case PROP_MIN_PRES_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Value_Min_Pres_Value_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Value_Min_Pres_Value(wp_data->object_instance);
                    value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                    value_c = malloc(value_c_len + 1);
                    snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                    ucix_add_option(ctxw, sec, idx_c, "min_value", value_c);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            }
            break;
        case PROP_RESOLUTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Value_Resolution_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Value_Resolution(wp_data->object_instance);
                    value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                    value_c = malloc(value_c_len + 1);
                    snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                    ucix_add_option(ctxw, sec, idx_c, "resolution", value_c);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            }
            break;
        case PROP_DESCRIPTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Analog_Value_Description_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "description",
                        Analog_Value_Description(wp_data->object_instance));
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
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                if (Analog_Value_Time_Delay_Set(
                    wp_data->object_instance, value.type.Unsigned_Int)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "time_delay",
                        Analog_Value_Time_Delay(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_NOTIFICATION_CLASS:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                if (Analog_Value_Notification_Class_Set(
                    wp_data->object_instance, value.type.Unsigned_Int)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "nc",
                        Analog_Value_Notification_Class(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_HIGH_LIMIT:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Value_High_Limit_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Value_High_Limit(wp_data->object_instance);
                    value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                    value_c = malloc(value_c_len + 1);
                    snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                    ucix_add_option(ctxw, sec, idx_c, "high_limit", value_c);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            }
            break;
        case PROP_LOW_LIMIT:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Value_Low_Limit_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Value_Low_Limit(wp_data->object_instance);
                    value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                    value_c = malloc(value_c_len + 1);
                    snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                    ucix_add_option(ctxw, sec, idx_c, "low_limit", value_c);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            }
            break;
        case PROP_DEADBAND:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Value_Deadband_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Value_Deadband(wp_data->object_instance);
                    value_c_len = snprintf_res(NULL, 0, resolution, value_f);
                    value_c = malloc(value_c_len + 1);
                    snprintf_res(value_c, value_c_len + 1, resolution, value_f);
                    ucix_add_option(ctxw, sec, idx_c, "dead_limit", value_c);
                    ucix_commit(ctxw,sec);
                    free(value_c);
                }
            }
            break;
        case PROP_LIMIT_ENABLE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BIT_STRING);
            if (status) {
                if (Analog_Value_Limit_Enable_Set(
                    wp_data->object_instance, value.type.Bit_String.value[0])) {
                    ucix_add_option_int(ctxw, sec, idx_c, "limit",
                        Analog_Value_Limit_Enable(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_EVENT_ENABLE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BIT_STRING);
            if (status) {
                if (Analog_Value_Event_Enable_Set(
                    wp_data->object_instance, value.type.Bit_String.value[0])) {
                    ucix_add_option_int(ctxw, sec, idx_c, "event",
                        Analog_Value_Event_Enable(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_NOTIFY_TYPE:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                if (Analog_Value_Notify_Type_Set(
                    wp_data->object_instance, value.type.Enumerated)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "notify_type",
                        Analog_Value_Notify_Type(wp_data->object_instance));
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
                    Analog_Value_Properties_Required,
                    Analog_Value_Properties_Optional,
                    Analog_Value_Properties_Proprietary,
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
 * @brief Sets a callback used when present-value is written from BACnet
 * @param cb - callback used to provide indications
 */
void Analog_Value_Write_Present_Value_Callback_Set(
    analog_value_write_present_value_callback cb)
{
    Analog_Value_Write_Present_Value_Callback = cb;
}

#if defined(INTRINSIC_REPORTING)
static const char *Analog_Value_Event_Message(
    uint32_t object_instance,
    enum BACnetEventTransitionBits transition,
    const char *default_text)
{
    struct object_data *pObject = NULL;
    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject && transition < MAX_BACNET_EVENT_TRANSITION &&
        pObject->Event_Message_Texts_Custom[transition]) {
        return pObject->Event_Message_Texts_Custom[transition];
    }
    return default_text;
}
#endif

/**
 * @brief Analog Value intrinsic reporting function.
 * @param object_instance [in] BACnet object-instance number of the object
 */
void Analog_Value_Intrinsic_Reporting(uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    BACNET_EVENT_NOTIFICATION_DATA event_data = { 0 };
    const char *msgText = NULL;
    BACNET_CHARACTER_STRING msgCharString = { 0 };
    struct object_data *pObject = NULL;
    uint8_t FromState = 0;
    uint8_t ToState = 0;
    float ExceededLimit = 0.0f;
    float PresentVal = 0.0f;
    BACNET_RELIABILITY Reliability = RELIABILITY_NO_FAULT_DETECTED;
    BACNET_PROPERTY_VALUE propertyValues = { 0 };
    bool SendNotify = false;

    pObject = Keylist_Data(Object_List, object_instance);
    if (!pObject) {
        return;
    }

    /* check whether Intrinsic reporting is enabled */
    if (!pObject->Event_Detection_Enable) {
        return; /* limits are not configured */
    }

    /* check limits */
    if (!pObject->Limit_Enable){
        return; /* limits are not configured */
    }

    if (pObject->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        pObject->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = pObject->Ack_notify_data.EventState;
        debug_printf(
            "Analog-Value[%d]: Send AckNotification.\n", object_instance);
        msgText = "AckNotification";

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        PresentVal = Analog_Value_Present_Value(object_instance);
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
                    /* A TO-OFFNORMAL event is generated under these conditions:
                    (a) the Present_Value must exceed the High_Limit for a
                    minimum period of time, specified in the Time_Delay
                    property, and (b) the HighLimitEnable flag must be set in
                    the Limit_Enable property, and (c) the TO-OFFNORMAL flag
                    must be set in the Event_Enable property. */
                    if ((PresentVal > pObject->High_Limit) &&
                        ((pObject->Limit_Enable & EVENT_HIGH_LIMIT_ENABLE) ==
                         EVENT_HIGH_LIMIT_ENABLE) &&
                        ((pObject->Event_Enable &
                          EVENT_ENABLE_TO_OFFNORMAL) ==
                         EVENT_ENABLE_TO_OFFNORMAL)) {
                        if (!pObject->Remaining_Time_Delay) {
                            pObject->Event_State = EVENT_STATE_HIGH_LIMIT;
                        } else {
                            pObject->Remaining_Time_Delay--;
                        }
                        break;
                    }
                    /* A TO-OFFNORMAL event is generated under these conditions:
                    (a) the Present_Value must exceed the Low_Limit plus the
                    Deadband for a minimum period of time, specified in the
                    Time_Delay property, and (b) the LowLimitEnable flag must be
                    set in the Limit_Enable property, and
                    (c) the TO-NORMAL flag must be set in the Event_Enable
                    property. */
                    if ((PresentVal < pObject->Low_Limit) &&
                        ((pObject->Limit_Enable & EVENT_LOW_LIMIT_ENABLE) ==
                         EVENT_LOW_LIMIT_ENABLE) &&
                        ((pObject->Event_Enable &
                          EVENT_ENABLE_TO_OFFNORMAL) ==
                         EVENT_ENABLE_TO_OFFNORMAL)) {
                        if (!pObject->Remaining_Time_Delay) {
                            pObject->Event_State = EVENT_STATE_LOW_LIMIT;
                        } else {
                            pObject->Remaining_Time_Delay--;
                        }
                        break;
                    }
                    /* value of the object is still in the same event state */
                    pObject->Remaining_Time_Delay = pObject->Time_Delay;
                    break;
                case EVENT_STATE_HIGH_LIMIT:
                    /* Once exceeded, the Present_Value must fall below the
                    High_Limit minus the Deadband before a TO-NORMAL event is
                    generated under these conditions: (a) the Present_Value must
                    fall below the High_Limit minus the Deadband for a minimum
                    period of time, specified in the Time_Delay property, and
                    (b) the HighLimitEnable flag must be set in the Limit_Enable
                    property, and (c) the TO-NORMAL flag must be set in the
                    Event_Enable property. */
                    if (((PresentVal <
                          pObject->High_Limit - pObject->Deadband) &&
                         ((pObject->Limit_Enable & EVENT_HIGH_LIMIT_ENABLE) ==
                          EVENT_HIGH_LIMIT_ENABLE) &&
                         ((pObject->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                          EVENT_ENABLE_TO_NORMAL)) ||
                        /* 13.3.6 (c) If pCurrentState is HIGH_LIMIT, and the
                         * HighLimitEnable flag of pLimitEnable is FALSE, then
                         * indicate a transition to the NORMAL event state. */
                        (!(pObject->Limit_Enable &
                           EVENT_HIGH_LIMIT_ENABLE))) {
                        if ((!pObject->Remaining_Time_Delay) ||
                            (!(pObject->Limit_Enable &
                               EVENT_HIGH_LIMIT_ENABLE))) {
                            pObject->Event_State = EVENT_STATE_NORMAL;
                        } else {
                            pObject->Remaining_Time_Delay--;
                        }
                        break;
                    }
                    /* value of the object is still in the same event state */
                    pObject->Remaining_Time_Delay = pObject->Time_Delay;
                    break;
                case EVENT_STATE_LOW_LIMIT:
                    /* Once the Present_Value has fallen below the Low_Limit,
                    the Present_Value must exceed the Low_Limit plus the
                    Deadband before a TO-NORMAL event is generated under these
                    conditions: (a) the Present_Value must exceed the Low_Limit
                    plus the Deadband for a minimum period of time, specified in
                    the Time_Delay property, and (b) the LowLimitEnable flag
                    must be set in the Limit_Enable property, and (c) the
                    TO-NORMAL flag must be set in the Event_Enable property. */
                    if (((PresentVal >
                          pObject->Low_Limit + pObject->Deadband) &&
                         ((pObject->Limit_Enable & EVENT_LOW_LIMIT_ENABLE) ==
                          EVENT_LOW_LIMIT_ENABLE) &&
                         ((pObject->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                          EVENT_ENABLE_TO_NORMAL)) ||
                        /* 13.3.6 (f) If pCurrentState is LOW_LIMIT, and the
                         * LowLimitEnable flag of pLimitEnable is FALSE, then
                         * indicate a transition to the NORMAL event state. */
                        (!(pObject->Limit_Enable & EVENT_LOW_LIMIT_ENABLE))) {
                        if ((!pObject->Remaining_Time_Delay) ||
                            (!(pObject->Limit_Enable &
                               EVENT_LOW_LIMIT_ENABLE))) {
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
                case EVENT_STATE_HIGH_LIMIT:
                    ExceededLimit = pObject->High_Limit;
                    msgText = Analog_Value_Event_Message(
                        object_instance, TRANSITION_TO_OFFNORMAL,
                        "Goes to high limit");
                    break;

                case EVENT_STATE_LOW_LIMIT:
                    ExceededLimit = pObject->Low_Limit;
                    msgText = Analog_Value_Event_Message(
                        object_instance, TRANSITION_TO_OFFNORMAL,
                        "Goes to low limit");
                    break;

                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_HIGH_LIMIT) {
                        ExceededLimit = pObject->High_Limit;
                        msgText = Analog_Value_Event_Message(
                            object_instance, TRANSITION_TO_NORMAL,
                            "Back to normal state from high limit");
                    } else if (FromState == EVENT_STATE_LOW_LIMIT) {
                        ExceededLimit = pObject->Low_Limit;
                        msgText = Analog_Value_Event_Message(
                            object_instance, TRANSITION_TO_NORMAL,
                            "Back to normal state from low limit");
                    } else {
                        ExceededLimit = 0;
                        msgText = Analog_Value_Event_Message(
                            object_instance, TRANSITION_TO_NORMAL,
                            "Back to normal state from fault");
                    }
                    break;

                case EVENT_STATE_FAULT:
                    ExceededLimit = 0;
                    msgText = Analog_Value_Event_Message(
                        object_instance, TRANSITION_TO_FAULT,
                        bactext_reliability_name(Reliability));
                    pObject->Last_ToFault_Event_Reliability = Reliability;
                    break;

                default:
                    ExceededLimit = 0;
                    break;
            } /* switch (ToState) */
            debug_printf(
                "Analog-Value[%d]: Event_State goes from %.128s to %.128s.\n",
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
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            datetime_local(
                &event_data.timeStamp.value.dateTime.date,
                &event_data.timeStamp.value.dateTime.time, NULL, NULL);
            /* set eventType and fill Event_Time_Stamps and
             * Event_Message_Texts*/
            switch (ToState) {
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
                    event_data.eventType = EVENT_OUT_OF_RANGE;
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
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
                    event_data.eventType = EVENT_OUT_OF_RANGE;
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
                event_data.notificationParams.outOfRange.exceedingValue =
                    PresentVal;
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
                /* Deadband used for limit checking. */
                event_data.notificationParams.outOfRange.deadband =
                    pObject->Deadband;
                /* Limit that was exceeded. */
                event_data.notificationParams.outOfRange.exceededLimit =
                    ExceededLimit;
            } else {
                event_data.notificationParams.changeOfReliability.reliability =
                    Reliability;

                propertyValues.propertyIdentifier = PROP_PRESENT_VALUE;
                propertyValues.propertyArrayIndex = BACNET_ARRAY_ALL;
                propertyValues.value.tag = BACNET_APPLICATION_TAG_REAL;
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
            "Analog-Value[%d]: Notification Class[%d]-%s "
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
            debug_printf("Analog-Value[%d]: Ack Required!\n", object_instance);
            switch (event_data.toState) {
                case EVENT_STATE_OFFNORMAL:
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
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
    unsigned index, BACNET_GET_EVENT_INFORMATION_DATA *getevent_data)
{
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, Analog_Value_Index_To_Instance(index));
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
        getevent_data->objectIdentifier.instance =
            Analog_Value_Index_To_Instance(index);
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
 * @brief Acknowledges the Event Information for this object.
 * @param alarmack_data - data for the Event Acknowledgement
 * @param error_code - error code for the Event Acknowledgement
 * @return 1 if successful, -1 if error, -2 if request is out-of-range
 */
int Analog_Value_Alarm_Ack(
    BACNET_ALARM_ACK_DATA *alarmack_data, BACNET_ERROR_CODE *error_code)
{
    struct object_data *pObject;

    if (!alarmack_data) {
        return -1;
    }
    pObject =
        Keylist_Data(Object_List, alarmack_data->eventObjectIdentifier.instance);
    if (!pObject) {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -1;
    }
    switch (alarmack_data->eventStateAcked) {
        case EVENT_STATE_OFFNORMAL:
        case EVENT_STATE_HIGH_LIMIT:
        case EVENT_STATE_LOW_LIMIT:
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
                /* Clean transitions flag. */
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
                        &pObject-> Acked_Transitions[TRANSITION_TO_FAULT]
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

    /* Return OK */
    return 1;
}

/**
 * @brief Handles getting the Alarm Summary for this object.
 * @param  index - index number of the object 0..count
 * @param  getalarm_data - data for the Alarm Summary
 * @return 1 if an active alarm is found, 0 if no active alarm, -1 if
 * end of list
 */
int Analog_Value_Alarm_Summary(
    unsigned index, BACNET_GET_ALARM_SUMMARY_DATA *getalarm_data)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, Analog_Value_Index_To_Instance(index));

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
            getalarm_data->objectIdentifier.instance =
                Analog_Value_Index_To_Instance(index);
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
#endif /* defined(INTRINSIC_REPORTING) */

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
            Analog_Value_Reset_Event_Properties(object_instance);
#endif
        } else {
            return BACNET_MAX_INSTANCE;
        }
    }

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
#if defined(INTRINSIC_REPORTING)
    unsigned j;
#endif
    struct object_data *pObject = NULL;
    int index = 0;
    unsigned priority = 0;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
    float value_f = 0.0;
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
        pObject->Priority_Array[priority] = 0.0;
    }
    pObject->Relinquish_Default = 0.0;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "cov_increment");
    if (!option)
        option = ictx->Object.COV_Increment;
    pObject->COV_Increment = strtof(option,(char **) NULL);
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "resolution");
    if (!option)
        option = ictx->Object.Resolution;
    value_f = strtof(option,(char **) NULL);
    if (!(value_f > 0)) value_f = 1;
    pObject->Resolution = value_f;
    pObject->Units = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "si_unit", ictx->Object.Units);
    pObject->Out_Of_Service = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "Out_Of_Service", false);
    pObject->Changed = false;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "min_value");
    if (!option)
        option = ictx->Object.Min_Pres_Value;
    value_f = strtof(option,(char **) NULL);
    value_f = limit_value_by_resolution(value_f, pObject->Resolution);
    pObject->Min_Pres_Value = value_f;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "max_value");
    if (!option)
        option = ictx->Object.Max_Pres_Value;
    value_f = strtof(option,(char **) NULL);
    value_f = limit_value_by_resolution(value_f, pObject->Resolution);
    pObject->Max_Pres_Value = value_f;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "value");
    if (option) {
        value_f = strtof(option,(char **) NULL);
        value_f = limit_value_by_resolution(value_f, pObject->Resolution);
        pObject->Priority_Array[BACNET_MAX_PRIORITY-1] = value_f;
        pObject->Relinquished[BACNET_MAX_PRIORITY-1] = false;
        pObject->Prior_Value = value_f;
    } else {
        pObject->Priority_Array[BACNET_MAX_PRIORITY-1] = 0.0;
        pObject->Relinquished[BACNET_MAX_PRIORITY-1] = false;
        pObject->Prior_Value = 0.0;
    }
#if defined(INTRINSIC_REPORTING)
    pObject->Event_State = EVENT_STATE_NORMAL;
    /* notification class not connected */
    pObject->Notification_Class = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "nc", ictx->Object.Notification_Class);
    pObject->Event_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "event", ictx->Object.Event_Enable);
    pObject->Event_Detection_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "event_detection", ictx->Object.Event_Detection_Enable);
    pObject->Time_Delay = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "time_delay", ictx->Object.Time_Delay);
    pObject->Limit_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "limit", ictx->Object.Limit_Enable);
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "high_limit");
    if (!option)
        option = ictx->Object.High_Limit;
    value_f = strtof(option,(char **) NULL);
    value_f = limit_value_by_resolution(value_f, pObject->Resolution);
    pObject->High_Limit = value_f;

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "low_limit");
    if (!option)
        option = ictx->Object.Low_Limit;
    value_f = strtof(option,(char **) NULL);
    value_f = limit_value_by_resolution(value_f, pObject->Resolution);
    pObject->Low_Limit = value_f;

    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "dead_limit");
    if (!option)
        option = ictx->Object.Deadband;
    value_f = strtof(option,(char **) NULL);
    value_f = limit_value_by_resolution(value_f, pObject->Resolution);
    pObject->Deadband = value_f;

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
 * @brief Initializes the Analog Value object data
 */
void Analog_Value_Init(void)
{
    struct uci_context *ctx;
    struct object_data_t tObject;
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
        tObject.Description = "Analog Value";
    option = ucix_get_option(ctx, sec, "default", "cov_increment");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.COV_Increment = strndup(option,option_str.length);
    else
        tObject.COV_Increment = "0.1";
    option = ucix_get_option(ctx, sec, "default", "resolution");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Resolution = strndup(option,option_str.length);
    else
        tObject.Resolution = "0.1";
    tObject.Units = ucix_get_option_int(ctx, sec, "default", "si_unit", 0);
    option = ucix_get_option(ctx, sec, "default", "min_value");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Min_Pres_Value = strndup(option,option_str.length);
    else
        tObject.Min_Pres_Value = "0.0";
    option = ucix_get_option(ctx, sec, "default", "max_value");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Max_Pres_Value = strndup(option,option_str.length);
    else
        tObject.Max_Pres_Value = "100.0";
#if defined(INTRINSIC_REPORTING)
    tObject.Notification_Class = ucix_get_option_int(ctx, sec, "default", "nc", BACNET_MAX_INSTANCE);
    tObject.Event_Enable = ucix_get_option_int(ctx, sec, "default", "event", 0);
    tObject.Event_Detection_Enable = ucix_get_option_int(ctx, sec, "default", "event_detection", 0);
    tObject.Time_Delay = ucix_get_option_int(ctx, sec, "default", "time_delay", 0);
    tObject.Limit_Enable = ucix_get_option_int(ctx, sec, "default", "limit", 0);
    option = ucix_get_option(ctx, sec, "default", "high_limit");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.High_Limit = strndup(option,option_str.length);
    else
        tObject.High_Limit = "100.0";
    option = ucix_get_option(ctx, sec, "default", "low_limit");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Low_Limit = strndup(option,option_str.length);
    else
        tObject.Low_Limit = "0.0";
    option = ucix_get_option(ctx, sec, "default", "dead_limit");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Deadband = strndup(option,option_str.length);
    else
        tObject.Deadband = "0.0";
    tObject.Notify_Type = ucix_get_option_int(ctx, sec, "default", "notify_type", 0);
#endif
	itr_m.section = sec;
	itr_m.ctx = ctx;
	itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void (*)(const char *, void *))uci_list, &itr_m);
    ucix_cleanup(ctx);
#if defined(INTRINSIC_REPORTING)
    /* Set handler for GetEventInformation function */
    handler_get_event_information_set(
        Object_Type, Analog_Value_Event_Information);
    /* Set handler for AcknowledgeAlarm function */
    handler_alarm_ack_set(Object_Type, Analog_Value_Alarm_Ack);
    /* Set handler for GetAlarmSummary Service */
    handler_get_alarm_summary_set(Object_Type, Analog_Value_Alarm_Summary);
#endif
}
