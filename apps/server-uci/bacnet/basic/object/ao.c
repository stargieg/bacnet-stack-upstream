/**
 * @file
 * @author Steve Karg
 * @date 2005
 * @brief Analog Output objects, customize for your use
 *
 * @section DESCRIPTION
 *
 * The Analog Output object is an object with a present-value that
 * uses a single precision floating point data type, and includes
 * a present-value derived from the priority array.
 *
 * @section LICENSE
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
 */
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "bacnet/config.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
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
#include "bacnet/basic/object/device.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/ucix/ucix.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#include "bacnet/alarm_ack.h"
#include "bacnet/getevent.h"
#include "bacnet/get_alarm_sum.h"
#endif
/* me! */
#include "bacnet/basic/object/ao.h"

static const char *sec = "bacnet_ao";
static const char *type = "ao";

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
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State:3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    float High_Limit;
    float Low_Limit;
    float Feedback_Value;
    float Deadband;
    unsigned Limit_Enable:2;
    unsigned Event_Enable:3;
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
    unsigned Notify_Type:1;
#endif /* INTRINSIC_REPORTING */
};

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_ANALOG_OUTPUT;
/* callback for present value writes */
static analog_output_write_present_value_callback
    Analog_Output_Write_Present_Value_Callback;

/* These three arrays are used by the ReadPropertyMultiple handler */

static const int Analog_Output_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_PRESENT_VALUE, PROP_STATUS_FLAGS,
    PROP_EVENT_STATE, PROP_OUT_OF_SERVICE, PROP_UNITS, PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
#if (BACNET_PROTOCOL_REVISION >= 17)
    PROP_CURRENT_COMMAND_PRIORITY,
#endif
    -1 };

static const int Analog_Output_Properties_Optional[] = { PROP_RELIABILITY,
    PROP_DESCRIPTION, PROP_COV_INCREMENT, PROP_MIN_PRES_VALUE,
    PROP_MAX_PRES_VALUE, PROP_RESOLUTION,
#if defined(INTRINSIC_REPORTING)
    PROP_TIME_DELAY, PROP_NOTIFICATION_CLASS, PROP_HIGH_LIMIT, PROP_LOW_LIMIT,
    PROP_DEADBAND, PROP_LIMIT_ENABLE, PROP_EVENT_ENABLE, PROP_ACKED_TRANSITIONS,
    PROP_NOTIFY_TYPE, PROP_EVENT_TIME_STAMPS, PROP_FEEDBACK_VALUE,
#endif
    -1 };

static const int Analog_Output_Properties_Proprietary[] = { -1 };

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

/**
 * @brief Returns the list of required, optional, and proprietary properties.
 * Used by ReadPropertyMultiple service.
 * @param pRequired - pointer to list of int terminated by -1, of
 * BACnet required properties for this object.
 * @param pOptional - pointer to list of int terminated by -1, of
 * BACnet optkional properties for this object.
 * @param pProprietary - pointer to list of int terminated by -1, of
 * BACnet proprietary properties for this object.
 */
void Analog_Output_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired) {
        *pRequired = Analog_Output_Properties_Required;
    }
    if (pOptional) {
        *pOptional = Analog_Output_Properties_Optional;
    }
    if (pProprietary) {
        *pProprietary = Analog_Output_Properties_Proprietary;
    }

    return;
}

/**
 * @brief Determines if a given Analog Value instance is valid
 * @param  object_instance - object-instance number of the object
 * @return  true if the instance is valid, and false if not
 */
bool Analog_Output_Valid_Instance(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return true;
    }

    return false;
}

/**
 * @brief Determines the number of Analog Value objects
 * @return  Number of Analog Value objects
 */
unsigned Analog_Output_Count(void)
{
    return Keylist_Count(Object_List)-1;
}

/**
 * @brief Determines the object instance-number for a given 0..N index
 * of Analog Value objects where N is Analog_Output_Count().
 * @param  index - 0..MAX_ANALOG_OUTPUTS value
 * @return  object instance-number for the given index
 */
uint32_t Analog_Output_Index_To_Instance(unsigned index)
{
    return Keylist_Key(Object_List, index);
}

/**
 * @brief For a given object instance-number, determines a 0..N index
 * of Analog Value objects where N is Analog_Output_Count().
 * @param  object_instance - object-instance number of the object
 * @return  index for the given instance-number, or MAX_ANALOG_OUTPUTS
 * if not valid.
 */
unsigned Analog_Output_Instance_To_Index(uint32_t object_instance)
{
    return Keylist_Index(Object_List, object_instance);
}

/**
 * @brief For a given object instance-number, determines the present-value
 * @param  object_instance - object-instance number of the object
 * @return  present-value of the object
 */
float Analog_Output_Present_Value(uint32_t object_instance)
{
    float value = 0.0;
    uint8_t priority = 0; /* loop counter */
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = Analog_Output_Relinquish_Default(object_instance);
        for (priority = 0; priority < BACNET_MAX_PRIORITY; priority++) {
            if (!pObject->Relinquished[priority]) {
                value = pObject->Priority_Array[priority];
                break;
            }
        }
    }

    return value;
}

#if defined(INTRINSIC_REPORTING)
/**
 * @brief For a given object instance-number, determines the present-value
 * @param  object_instance - object-instance number of the object
 * @return  feedback-value of the object
 */
float Analog_Output_Feedback_Value(uint32_t object_instance)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (pObject->Feedback_Value) {
            value = pObject->Feedback_Value;
        } else {
            value = Analog_Output_Present_Value(object_instance);
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
uint32_t Analog_Output_Time_Delay(uint32_t object_instance)
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
bool Analog_Output_Time_Delay_Set(uint32_t object_instance, uint32_t value)
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
uint32_t Analog_Output_Notification_Class(uint32_t object_instance)
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
bool Analog_Output_Notification_Class_Set(uint32_t object_instance, uint32_t value)
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
float Analog_Output_High_Limit(uint32_t object_instance)
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
bool Analog_Output_High_Limit_Set(uint32_t object_instance, float value)
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
float Analog_Output_Low_Limit(uint32_t object_instance)
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
bool Analog_Output_Low_Limit_Set(uint32_t object_instance, float value)
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
float Analog_Output_Deadband(uint32_t object_instance)
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
bool Analog_Output_Deadband_Set(uint32_t object_instance, float value)
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
uint8_t Analog_Output_Limit_Enable(uint32_t object_instance)
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
bool Analog_Output_Limit_Enable_Set(uint32_t object_instance, uint8_t value)
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
uint8_t Analog_Output_Event_Enable(uint32_t object_instance)
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
bool Analog_Output_Event_Enable_Set(uint32_t object_instance, uint8_t value)
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
bool Analog_Output_Acked_Transitions(uint32_t object_instance, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION])
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
uint8_t Analog_Output_Notify_Type(uint32_t object_instance)
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
bool Analog_Output_Notify_Type_Set(uint32_t object_instance, uint8_t value)
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


/**
 * For a given object instance-number, returns the Acked Transitions
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - acked_info struct
 *
 * @return true
 */
bool Analog_Output_Event_Time_Stamps(uint32_t object_instance, BACNET_DATE_TIME *value[MAX_BACNET_EVENT_TRANSITION])
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

#endif

/**
 * @brief For a given object instance-number, determines the priority
 * @param  object_instance - object-instance number of the object
 * @return  active priority 1..16, or 0 if no priority is active
 */
unsigned Analog_Output_Present_Value_Priority(
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
 * @brief For a given object instance-number and priority 1..16, determines the
 *  priority-array value
 * @param  object_instance - object-instance number of the object
 * @param  priority - priority-array index value 1..16
 *
 * @return priority-array value of the object
 */
static float Analog_Output_Priority_Array(
    uint32_t object_instance, unsigned priority)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= BACNET_MIN_PRIORITY) &&
            (priority <= BACNET_MAX_PRIORITY)) {
            value = pObject->Priority_Array[priority - 1];
        }
    }

    return value;
}

/**
 * @brief For a given object instance-number and priority 1..16, determines
 *  if the priority-array slot is NULL
 * @param  object_instance - object-instance number of the object
 * @param  priority - priority-array index value 1..16
 * @return true if the priority array slot is NULL
 */
static bool Analog_Output_Priority_Array_Null(
    uint32_t object_instance, unsigned priority)
{
    bool null_value = false;

    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= BACNET_MIN_PRIORITY) &&
            (priority <= BACNET_MAX_PRIORITY)) {
            if (pObject->Relinquished[priority - 1]) {
                null_value = true;
            }
        }
    }

    return null_value;
}

/**
 * For a given object instance-number, determines the relinquish-default value
 *
 * @param object_instance - object-instance number
 *
 * @return relinquish-default value of the object
 */
float Analog_Output_Relinquish_Default(uint32_t object_instance)
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
bool Analog_Output_Relinquish_Default_Set(uint32_t object_instance, float value)
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
 * For a given object instance-number, checks the present-value for COV
 *
 * @param  pObject - specific object with valid data
 * @param  value - floating point analog value
 */
static void Analog_Output_Present_Value_COV_Detect(
    struct object_data *pObject, float value)
{
    float prior_value = 0.0;
    float cov_increment = 0.0;
    float cov_delta = 0.0;

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
 * For a given object instance-number, sets the present-value
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - floating point analog value
 * @param  priority - priority-array index value 1..16
 * @return  true if values are within range and present-value is set.
 */
bool Analog_Output_Present_Value_Set(
    uint32_t object_instance, float value, unsigned priority)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = false;
            pObject->Priority_Array[priority - 1] = value;
            Analog_Output_Present_Value_COV_Detect(
                pObject, Analog_Output_Present_Value(object_instance));
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
bool Analog_Output_Present_Value_Relinquish(
    uint32_t object_instance, unsigned priority)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY)) {
            pObject->Relinquished[priority - 1] = true;
            pObject->Priority_Array[priority - 1] = 0.0;
            Analog_Output_Present_Value_COV_Detect(
                pObject, Analog_Output_Present_Value(object_instance));
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
static bool Analog_Output_Present_Value_Write(
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
            (value >= pObject->Low_Limit) && (value <= pObject->High_Limit)) {
            if (priority != 6) {
                old_value = Analog_Output_Present_Value(object_instance);
                Analog_Output_Present_Value_Set(object_instance, value, priority);
                if (pObject->Out_Of_Service) {
                    /* The physical point that the object represents
                        is not in service. This means that changes to the
                        Present_Value property are decoupled from the
                        physical output when the value of Out_Of_Service
                        is true. */
                } else if (Analog_Output_Write_Present_Value_Callback) {
                    new_value = Analog_Output_Present_Value(object_instance);
                    Analog_Output_Write_Present_Value_Callback(
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
static bool Analog_Output_Present_Value_Relinquish_Write(
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
                old_value = Analog_Output_Present_Value(object_instance);
                Analog_Output_Present_Value_Relinquish(object_instance, priority);
                if (pObject->Out_Of_Service) {
                    /* The physical point that the object represents
                        is not in service. This means that changes to the
                        Present_Value property are decoupled from the
                        physical output when the value of Out_Of_Service
                        is true. */
                } else if (Analog_Output_Write_Present_Value_Callback) {
                    new_value = Analog_Output_Present_Value(object_instance);
                    Analog_Output_Write_Present_Value_Callback(
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
 * For a given object instance-number, loads the object-name into
 * a characterstring. Note that the object name must be unique
 * within this device.
 *
 * @param  object_instance - object-instance number of the object
 * @param  object_name - holds the object-name retrieved
 *
 * @return  true if object-name was retrieved
 */
bool Analog_Output_Object_Name(
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
            snprintf(name_text, sizeof(name_text), "ANALOG OUTPUT %u",
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
bool Analog_Output_Name_Set(uint32_t object_instance, char *new_name)
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
 * For a given object instance-number, returns the units property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  units property value
 */
uint16_t Analog_Output_Units(uint32_t object_instance)
{
    uint16_t units = UNITS_NO_UNITS;
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
bool Analog_Output_Units_Set(uint32_t object_instance, uint16_t units)
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
 * For a given object instance-number, returns the out-of-service
 * status flag
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  out-of-service status flag
 */
bool Analog_Output_Out_Of_Service(uint32_t object_instance)
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
 * @brief For a given object instance-number, sets the out-of-service status
 * flag
 * @param object_instance - object-instance number of the object
 * @param value - boolean out-of-service value
 * @return true if the out-of-service status flag was set
 */
void Analog_Output_Out_Of_Service_Set(uint32_t object_instance, bool value)
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
 * @brief For a given object instance-number, returns the overridden
 * status flag value
 * @param  object_instance - object-instance number of the object
 * @return  out-of-service property value
 */
bool Analog_Output_Overridden(uint32_t object_instance)
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
void Analog_Output_Overridden_Set(uint32_t object_instance, bool value)
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
 * @brief For a given object instance-number, gets the reliability.
 * @param  object_instance - object-instance number of the object
 * @return reliability value
 */
BACNET_RELIABILITY Analog_Output_Reliability(uint32_t object_instance)
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
 * @brief For a given object instance-number, gets the Fault status flag
 * @param  object_instance - object-instance number of the object
 * @return  true the status flag is in Fault
 */
static bool Analog_Output_Object_Fault(struct object_data *pObject)
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
bool Analog_Output_Reliability_Set(
    uint32_t object_instance, BACNET_RELIABILITY value)
{
    struct object_data *pObject;
    bool status = false;
    bool fault = false;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (value <= 255) {
            fault = Analog_Output_Object_Fault(pObject);
            pObject->Reliability = value;
            if (fault != Analog_Output_Object_Fault(pObject)) {
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
static bool Analog_Output_Fault(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);

    return Analog_Output_Object_Fault(pObject);
}

/**
 * @brief For a given object instance-number, returns the description
 * @param  object_instance - object-instance number of the object
 * @return description text or NULL if not found
 */
char *Analog_Output_Description(uint32_t object_instance)
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
bool Analog_Output_Description_Set(uint32_t object_instance, char *new_name)
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

/**
 * @brief For a given object instance-number, returns the min-pres-value
 * @param  object_instance - object-instance number of the object
 * @return value or 0.0 if not found
 */
float Analog_Output_Min_Pres_Value(uint32_t object_instance)
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
bool Analog_Output_Min_Pres_Value_Set(uint32_t object_instance, float value)
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
float Analog_Output_Max_Pres_Value(uint32_t object_instance)
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
bool Analog_Output_Max_Pres_Value_Set(uint32_t object_instance, float value)
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
 * For a given object instance-number, gets the event-state property value
 *
 * @param  object_instance - object-instance number of the object
 *
 * @return  event-state property value
 */
unsigned Analog_Output_Event_State(uint32_t object_instance)
{
    unsigned state = EVENT_STATE_NORMAL;
#if defined(INTRINSIC_REPORTING)
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        state = pObject->Event_State;
    }
#endif

    return state;
}


/**
 * @brief Get the COV change flag status
 * @param object_instance - object-instance number of the object
 * @return the COV change flag status
 */
bool Analog_Output_Change_Of_Value(uint32_t object_instance)
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
 * @brief Clear the COV change flag
 * @param object_instance - object-instance number of the object
 */
void Analog_Output_Change_Of_Value_Clear(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
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
bool Analog_Output_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list)
{
    bool status = false;
    struct object_data *pObject;
    bool in_alarm = true;
    bool fault = false;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (Analog_Output_Event_State(object_instance) == EVENT_STATE_NORMAL) 
            in_alarm = false;
        if (Analog_Output_Object_Fault(pObject))
            fault = true;
        status = cov_value_list_encode_real(value_list, pObject->Prior_Value,
            in_alarm, fault, pObject->Overridden, pObject->Out_Of_Service);
    }
    return status;
}

/**
 * @brief Get the COV change flag status
 * @param object_instance - object-instance number of the object
 * @return the COV change flag status
 */
float Analog_Output_COV_Increment(uint32_t object_instance)
{
    float value = 0.0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->COV_Increment;
    }

    return value;
}

/**
 * @brief Get the COV change flag status
 * @param object_instance - object-instance number of the object
 * @param value - COV Increment value to set
 * @return the COV change flag status
 */
void Analog_Output_COV_Increment_Set(uint32_t object_instance, float value)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = limit_value_by_resolution(value, pObject->Resolution);
        pObject->COV_Increment = value;
    }
}

/**
 * @brief Get the Resolution
 * @param object_instance - object-instance number of the object
 * @return the Resolution
 */
float Analog_Output_Resolution(uint32_t object_instance)
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
bool Analog_Output_Resolution_Set(uint32_t object_instance, float value)
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
 * @brief ReadProperty handler for this object.  For the given ReadProperty
 * data, the application_data is loaded or the error flags are set.
 * @param  rpdata - BACNET_READ_PROPERTY_DATA data, including
 * requested data and space for the reply, or error response.
 * @return number of APDU bytes in the response, or
 * BACNET_STATUS_ERROR on error.
 */
int Analog_Output_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    int len = 0;
    int apdu_len = 0; /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint8_t *apdu = NULL;
    uint32_t units = 0;
    float real_value = 0.0;
    unsigned i = 0;
    bool state = false;
    ACKED_INFO *ack_info[MAX_BACNET_EVENT_TRANSITION];
    BACNET_DATE_TIME *timestamp[MAX_BACNET_EVENT_TRANSITION];

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;
    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], Object_Type, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Analog_Output_Object_Name(rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0], Object_Type);
            break;
        case PROP_PRESENT_VALUE:
            real_value = Analog_Output_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
#if defined(INTRINSIC_REPORTING)
        case PROP_FEEDBACK_VALUE:
            real_value = Analog_Output_Feedback_Value(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
#endif
        case PROP_MIN_PRES_VALUE:
            real_value = Analog_Output_Min_Pres_Value(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_MAX_PRES_VALUE:
            real_value = Analog_Output_Max_Pres_Value(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_STATUS_FLAGS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM,
            Analog_Output_Event_State(rpdata->object_instance) !=
                    EVENT_STATE_NORMAL);
            state = Analog_Output_Fault(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, state);
            state = Analog_Output_Overridden(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, state);
            state = Analog_Output_Out_Of_Service(rpdata->object_instance);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE, state);
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;
        case PROP_RELIABILITY:
            apdu_len = encode_application_enumerated(
                &apdu[0], Analog_Output_Reliability(rpdata->object_instance));
            break;
        case PROP_EVENT_STATE:
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0], 
                    Analog_Output_Event_State(rpdata->object_instance));
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;
        case PROP_OUT_OF_SERVICE:
            state = Analog_Output_Out_Of_Service(rpdata->object_instance);
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;
        case PROP_UNITS:
            units = Analog_Output_Units(rpdata->object_instance);
            apdu_len = encode_application_enumerated(&apdu[0], units);
            break;
        case PROP_PRIORITY_ARRAY:
            if (rpdata->array_index == 0) {
                /* Array element zero = the number of elements in the array */
                apdu_len =
                    encode_application_unsigned(&apdu[0], BACNET_MAX_PRIORITY);
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                /* no index was specified; try to encode the entire list */
                for (i = 1; i <= BACNET_MAX_PRIORITY; i++) {
                    if (Analog_Output_Priority_Array_Null(
                            rpdata->object_instance, i)) {
                        len = encode_application_null(&apdu[apdu_len]);
                    } else {
                        real_value = Analog_Output_Priority_Array(
                            rpdata->object_instance, i);
                        len = encode_application_real(
                            &apdu[apdu_len], real_value);
                    }
                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU) {
                        apdu_len += len;
                    } else {
                        /* Abort response */
                        rpdata->error_code =
                            ERROR_CODE_ABORT_SEGMENTATION_NOT_SUPPORTED;
                        apdu_len = BACNET_STATUS_ABORT;
                        break;
                    }
                }
            } else {
                if (rpdata->array_index <= BACNET_MAX_PRIORITY) {
                    if (Analog_Output_Priority_Array_Null(
                            rpdata->object_instance, rpdata->array_index)) {
                        apdu_len = encode_application_null(&apdu[apdu_len]);
                    } else {
                        real_value = Analog_Output_Priority_Array(
                            rpdata->object_instance, rpdata->array_index);
                        apdu_len = encode_application_real(
                            &apdu[apdu_len], real_value);
                    }
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;
        case PROP_RELINQUISH_DEFAULT:
            real_value =
                Analog_Output_Relinquish_Default(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;
        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Analog_Output_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_COV_INCREMENT:
            apdu_len = encode_application_real(
                &apdu[0], Analog_Output_COV_Increment(rpdata->object_instance));
            break;
        case PROP_RESOLUTION:
            apdu_len = encode_application_real(
                &apdu[0], Analog_Output_Resolution(rpdata->object_instance));
            break;
#if (BACNET_PROTOCOL_REVISION >= 17)
        case PROP_CURRENT_COMMAND_PRIORITY:
            i = Analog_Output_Present_Value_Priority(rpdata->object_instance);
            if ((i >= BACNET_MIN_PRIORITY) && (i <= BACNET_MAX_PRIORITY)) {
                apdu_len = encode_application_unsigned(&apdu[0], i);
            } else {
                apdu_len = encode_application_null(&apdu[0]);
            }
            break;
#endif
#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            i = Analog_Output_Time_Delay(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], i);
            break;

        case PROP_NOTIFICATION_CLASS:
            i = Analog_Output_Notification_Class(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], i);
            break;

        case PROP_HIGH_LIMIT:
            real_value = Analog_Output_High_Limit(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;

        case PROP_LOW_LIMIT:
            real_value = Analog_Output_Low_Limit(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;

        case PROP_DEADBAND:
            real_value = Analog_Output_Deadband(rpdata->object_instance);
            apdu_len = encode_application_real(&apdu[0], real_value);
            break;

        case PROP_LIMIT_ENABLE:
            i = Analog_Output_Limit_Enable(rpdata->object_instance);
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, 0,
                (i & EVENT_LOW_LIMIT_ENABLE) ? true
                                             : false);
            bitstring_set_bit(&bit_string, 1,
                (i & EVENT_HIGH_LIMIT_ENABLE) ? true
                                              : false);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_EVENT_ENABLE:
            i = Analog_Output_Event_Enable(rpdata->object_instance);
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
            state = Analog_Output_Acked_Transitions(rpdata->object_instance, ack_info);
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
            i = Analog_Output_Notify_Type(rpdata->object_instance);
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
                state = Analog_Output_Event_Time_Stamps(rpdata->object_instance, timestamp);
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
                state = Analog_Output_Event_Time_Stamps(rpdata->object_instance, timestamp);
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
        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->object_property != PROP_EVENT_TIME_STAMPS) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
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
bool Analog_Output_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    bool status = false; /* return value */
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;
    struct uci_context *ctxw = NULL;
    char *idx_c = NULL;
    int idx_c_len = 0;
    float value_f = 0.0;
    float resolution = 0.1;
    char *value_c = NULL;
    int value_c_len = 0;

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
    if ((wp_data->object_property != PROP_PRIORITY_ARRAY) &&
        (wp_data->object_property != PROP_EVENT_TIME_STAMPS) &&
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
    resolution = Analog_Output_Resolution(wp_data->object_instance);
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Output_Present_Value_Write(wp_data->object_instance,
                    value.type.Real, wp_data->priority,
                    &wp_data->error_class, &wp_data->error_code)) {
                    value_f = Analog_Output_Present_Value(wp_data->object_instance);
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
                    if (Analog_Output_Present_Value_Relinquish_Write(
                        wp_data->object_instance, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code)) {
                        value_f = Analog_Output_Present_Value(wp_data->object_instance);
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
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BOOLEAN);
            if (status) {
                Analog_Output_Out_Of_Service_Set(
                    wp_data->object_instance, value.type.Boolean);
            }
            break;
        case PROP_COV_INCREMENT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                Analog_Output_COV_Increment_Set(wp_data->object_instance,
                value.type.Real);
                value_f = Analog_Output_COV_Increment(wp_data->object_instance);
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
                if (Analog_Output_Name_Set(
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
        case PROP_UNITS:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                if (Analog_Output_Units_Set(
                    wp_data->object_instance, value.type.Enumerated)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "units",
                        Analog_Output_Units(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_RELIABILITY:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status)
                Analog_Output_Reliability_Set(wp_data->object_instance,
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
                Analog_Output_Relinquish_Default_Set(wp_data->object_instance,
                    value.type.Real);
            break;
        case PROP_MAX_PRES_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Output_Max_Pres_Value_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Output_Max_Pres_Value(wp_data->object_instance);
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
                if (Analog_Output_Min_Pres_Value_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Output_Min_Pres_Value(wp_data->object_instance);
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
                if (Analog_Output_Resolution_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Output_Resolution(wp_data->object_instance);
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
                if (Analog_Output_Description_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "description",
                        Analog_Output_Description(wp_data->object_instance));
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
                if (Analog_Output_Time_Delay_Set(
                    wp_data->object_instance, value.type.Unsigned_Int)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "time_delay",
                        Analog_Output_Time_Delay(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_NOTIFICATION_CLASS:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                if (Analog_Output_Notification_Class_Set(
                    wp_data->object_instance, value.type.Unsigned_Int)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "nc",
                        Analog_Output_Notification_Class(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_HIGH_LIMIT:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Output_High_Limit_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Output_High_Limit(wp_data->object_instance);
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
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Output_Low_Limit_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Output_Low_Limit(wp_data->object_instance);
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
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                if (Analog_Output_Deadband_Set(wp_data->object_instance,
                    value.type.Real)) {
                    value_f = Analog_Output_Deadband(wp_data->object_instance);
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
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BIT_STRING);
            if (status) {
                if (Analog_Output_Limit_Enable_Set(
                    wp_data->object_instance, value.type.Bit_String.value[0])) {
                    ucix_add_option_int(ctxw, sec, idx_c, "limit",
                        Analog_Output_Limit_Enable(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_EVENT_ENABLE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_BIT_STRING);
            if (status) {
                if (Analog_Output_Event_Enable_Set(
                    wp_data->object_instance, value.type.Bit_String.value[0])) {
                    ucix_add_option_int(ctxw, sec, idx_c, "event",
                        Analog_Output_Event_Enable(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_NOTIFY_TYPE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_ENUMERATED);
            if (status) {
                if (Analog_Output_Notify_Type_Set(
                    wp_data->object_instance, value.type.Enumerated)) {
                    ucix_add_option_int(ctxw, sec, idx_c, "notify_type",
                        Analog_Output_Notify_Type(wp_data->object_instance));
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
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
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
void Analog_Output_Write_Present_Value_Callback_Set(
    analog_output_write_present_value_callback cb)
{
    Analog_Output_Write_Present_Value_Callback = cb;
}

void Analog_Output_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    struct object_data *pObject;
    BACNET_EVENT_NOTIFICATION_DATA event_data;
    BACNET_CHARACTER_STRING msgText;
    uint8_t FromState = 0;
    uint8_t ToState;
    float ExceededLimit = 0.0f;
    float PresentVal = 0.0f;
    bool SendNotify = false;

    pObject = Keylist_Data(Object_List, object_instance);

    if (!pObject)
        return;

    /* check limits */
    if (!pObject->Limit_Enable)
        return; /* limits are not configured */


    if (pObject->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        pObject->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = pObject->Ack_notify_data.EventState;

#if PRINT_ENABLED
        fprintf(stderr, "Send Acknotification for (%s,%d).\n",
            bactext_object_type_name(OBJECT_ANALOG_OUTPUT), object_instance);
#endif /* PRINT_ENABLED */

        characterstring_init_ansi(&msgText, "AckNotification");

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        PresentVal = Analog_Output_Present_Value(object_instance);
        FromState = pObject->Event_State;
        switch (pObject->Event_State) {
            case EVENT_STATE_NORMAL:
                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the High_Limit for a minimum
                   period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-OFFNORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal > pObject->High_Limit) &&
                    ((pObject->Limit_Enable & EVENT_HIGH_LIMIT_ENABLE) ==
                        EVENT_HIGH_LIMIT_ENABLE) &&
                    ((pObject->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                        EVENT_ENABLE_TO_OFFNORMAL)) {
                    if (!pObject->Remaining_Time_Delay)
                        pObject->Event_State = EVENT_STATE_HIGH_LIMIT;
                    else
                        pObject->Remaining_Time_Delay--;
                    break;
                }

                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the Low_Limit plus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (b) the LowLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-NORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal < pObject->Low_Limit) &&
                    ((pObject->Limit_Enable & EVENT_LOW_LIMIT_ENABLE) ==
                        EVENT_LOW_LIMIT_ENABLE) &&
                    ((pObject->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                        EVENT_ENABLE_TO_OFFNORMAL)) {
                    if (!pObject->Remaining_Time_Delay)
                        pObject->Event_State = EVENT_STATE_LOW_LIMIT;
                    else
                        pObject->Remaining_Time_Delay--;
                    break;
                }
                /* value of the object is still in the same event state */
                pObject->Remaining_Time_Delay = pObject->Time_Delay;
                break;

            case EVENT_STATE_HIGH_LIMIT:
                /* Once exceeded, the Present_Value must fall below the High_Limit minus
                   the Deadband before a TO-NORMAL event is generated under these conditions:
                   (a) the Present_Value must fall below the High_Limit minus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-NORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal < pObject->High_Limit - pObject->Deadband)
                    && ((pObject->Limit_Enable & EVENT_HIGH_LIMIT_ENABLE) ==
                        EVENT_HIGH_LIMIT_ENABLE) &&
                    ((pObject->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
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

            case EVENT_STATE_LOW_LIMIT:
                /* Once the Present_Value has fallen below the Low_Limit,
                   the Present_Value must exceed the Low_Limit plus the Deadband
                   before a TO-NORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the Low_Limit plus the Deadband
                   for a minimum period of time, specified in the Time_Delay property, and
                   (b) the LowLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-NORMAL flag must be set in the Event_Enable property. */
                if ((PresentVal > pObject->Low_Limit + pObject->Deadband)
                    && ((pObject->Limit_Enable & EVENT_LOW_LIMIT_ENABLE) ==
                        EVENT_LOW_LIMIT_ENABLE) &&
                    ((pObject->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
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
                case EVENT_STATE_HIGH_LIMIT:
                    ExceededLimit = pObject->High_Limit;
                    characterstring_init_ansi(&msgText, "Goes to high limit");
                    break;

                case EVENT_STATE_LOW_LIMIT:
                    ExceededLimit = pObject->Low_Limit;
                    characterstring_init_ansi(&msgText, "Goes to low limit");
                    break;

                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_HIGH_LIMIT) {
                        ExceededLimit = pObject->High_Limit;
                        characterstring_init_ansi(&msgText,
                            "Back to normal state from high limit");
                    } else {
                        ExceededLimit = pObject->Low_Limit;
                        characterstring_init_ansi(&msgText,
                            "Back to normal state from low limit");
                    }
                    break;

                default:
                    ExceededLimit = 0;
                    break;
            }   /* switch (ToState) */

#if PRINT_ENABLED
            fprintf(stderr, "Event_State for (%s,%d) goes from %s to %s.\n",
                bactext_object_type_name(OBJECT_ANALOG_OUTPUT), object_instance,
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
        event_data.eventObjectIdentifier.type = OBJECT_ANALOG_OUTPUT;
        event_data.eventObjectIdentifier.instance = object_instance;

        /* Time Stamp */
        event_data.timeStamp.tag = TIME_STAMP_DATETIME;
        Device_getCurrentDateTime(&event_data.timeStamp.value.dateTime);

        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* fill Event_Time_Stamps */
            switch (ToState) {
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
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
            /* Value that exceeded a limit. */
            event_data.notificationParams.outOfRange.exceedingValue =
                PresentVal;
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
            /* Deadband used for limit checking. */
            event_data.notificationParams.outOfRange.deadband =
                pObject->Deadband;
            /* Limit that was exceeded. */
            event_data.notificationParams.outOfRange.exceededLimit =
                ExceededLimit;
        }

        /* add data from notification class */
        Notification_Class_common_reporting_function(&event_data);

        /* Ack required */
        if ((event_data.notifyType != NOTIFY_ACK_NOTIFICATION) &&
            (event_data.ackRequired == true)) {
            switch (event_data.toState) {
                case EVENT_STATE_OFFNORMAL:
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
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
                case EVENT_STATE_MAX:
                    break;
            }
        }
    }
#endif /* defined(INTRINSIC_REPORTING) */
}


#if defined(INTRINSIC_REPORTING)
int Analog_Output_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    struct object_data *pObject;
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;

    pObject = Keylist_Data(Object_List, Keylist_Key(Object_List, index));

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
        getevent_data->objectIdentifier.type = OBJECT_ANALOG_OUTPUT;
        getevent_data->objectIdentifier.instance =
            Analog_Output_Index_To_Instance(index);
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

int Analog_Output_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code)
{
    struct object_data *pObject;
    unsigned index = 0;

    pObject = Keylist_Data(Object_List, index);

    /* check index */
    if (!pObject) {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -1;
    }

    switch (alarmack_data->eventStateAcked) {
        case EVENT_STATE_OFFNORMAL:
        case EVENT_STATE_HIGH_LIMIT:
        case EVENT_STATE_LOW_LIMIT:
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

int Analog_Output_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, Keylist_Key(Object_List, index));

    /* check index */
    if (pObject) {
        /* Event_State is not equal to NORMAL  and
           Notify_Type property value is ALARM */
        if ((pObject->Event_State != EVENT_STATE_NORMAL) &&
            (pObject->Notify_Type == NOTIFY_ALARM)) {
            /* Object Identifier */
            getalarm_data->objectIdentifier.type = OBJECT_ANALOG_OUTPUT;
            getalarm_data->objectIdentifier.instance =
                Analog_Output_Index_To_Instance(index);
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
 * @brief Creates a Analog Value object
 * @param object_instance - object-instance number of the object
 * @return true if the object-instance was created
 */
bool Analog_Output_Create(uint32_t object_instance)
{
    bool status = false;
    struct object_data *pObject = NULL;
    int index = 0;
    unsigned priority = 0;

    pObject = Keylist_Data(Object_List, object_instance);
    if (!pObject) {
        pObject = calloc(1, sizeof(struct object_data));
        if (pObject) {
            pObject->Object_Name = NULL;
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
            /* add to list */
            index = Keylist_Data_Add(Object_List, object_instance, pObject);
            if (index >= 0) {
                status = true;
                Device_Inc_Database_Revision();
            }
        }
    }

    return status;
}

/**
 * @brief Deletes an Analog Value object
 * @param object_instance - object-instance number of the object
 * @return true if the object-instance was deleted
 */
bool Analog_Output_Delete(uint32_t object_instance)
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
void Analog_Output_Cleanup(void)
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
	disable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx, "default") == 0)
		return;
	if (disable)
		return;
    idx = atoi(sec_idx);
    struct object_data *pObject = NULL;
    int index = 0;
    unsigned priority = 0;
    pObject = calloc(1, sizeof(struct object_data));
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
    float value_f = 0.0;

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
    pObject->Prior_Value = 0.0;
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
#if defined(INTRINSIC_REPORTING)
        option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "fb_value");
        if (option){
            value_f = strtof(option,(char **) NULL);
            value_f = limit_value_by_resolution(value_f, pObject->Resolution);
            pObject->Feedback_Value = value_f;
        } else
            pObject->Feedback_Value = pObject->Priority_Array[BACNET_MAX_PRIORITY-1];
#endif
    } else {
        pObject->Priority_Array[BACNET_MAX_PRIORITY-1] = 0.0;
        pObject->Relinquished[BACNET_MAX_PRIORITY-1] = false;
#if defined(INTRINSIC_REPORTING)
        pObject->Feedback_Value = 0.0;
#endif
    }
#if defined(INTRINSIC_REPORTING)
    pObject->Event_State = EVENT_STATE_NORMAL;
    /* notification class not connected */
    pObject->Notification_Class = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "nc", ictx->Object.Notification_Class);
    pObject->Event_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "event", ictx->Object.Event_Enable); // or 7?
    pObject->Time_Delay = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "time_delay", ictx->Object.Time_Delay); // or 2s
    pObject->Limit_Enable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "limit", ictx->Object.Limit_Enable); // or 3
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

    pObject->Notify_Type = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "notify_type", ictx->Object.Notify_Type); // 0=Alarm 1=Event

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
void Analog_Output_Init(void)
{
    Object_List = Keylist_Create();
    struct uci_context *ctx;
    ctx = ucix_init(sec);
    if (!ctx)
        fprintf(stderr, "Failed to load config file %s\n",sec);
    struct object_data_t tObject;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;

    struct object_data *pObject = NULL;
    /* add to list */
    Keylist_Data_Add(Object_List, BACNET_MAX_INSTANCE, pObject);

    option = ucix_get_option(ctx, sec, "default", "description");
    if (option && characterstring_init_ansi(&option_str, option))
        tObject.Description = strndup(option,option_str.length);
    else
        tObject.Description = "Analog Ouput";
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
    tObject.Event_Enable = ucix_get_option_int(ctx, sec, "default", "event", 0); // or 7?
    tObject.Time_Delay = ucix_get_option_int(ctx, sec, "default", "time_delay", 0); // or 2s
    tObject.Limit_Enable = ucix_get_option_int(ctx, sec, "default", "limit", 0); // or 3
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
    tObject.Notify_Type = ucix_get_option_int(ctx, sec, "default", "notify_type", 0); // 0=Alarm 1=Event
#endif
    struct itr_ctx itr_m;
	itr_m.section = sec;
	itr_m.ctx = ctx;
	itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void (*)(const char *, void *))uci_list, &itr_m);
    ucix_cleanup(ctx);
#if defined(INTRINSIC_REPORTING)
    /* Set handler for GetEventInformation function */
    handler_get_event_information_set(Object_Type,
        Analog_Output_Event_Information);
    /* Set handler for AcknowledgeAlarm function */
    handler_alarm_ack_set(Object_Type, Analog_Output_Alarm_Ack);
    /* Set handler for GetAlarmSummary Service */
    handler_get_alarm_summary_set(Object_Type,
        Analog_Output_Alarm_Summary);
#endif
}
