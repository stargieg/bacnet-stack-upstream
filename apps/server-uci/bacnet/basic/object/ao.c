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
#include "device.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/ucix/ucix.h"
/* me! */
#include "ao.h"

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
    uint16_t Units;
    uint8_t Reliability;
    const char *Object_Name;
    const char *Description;
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
    PROP_MAX_PRES_VALUE, -1 };

static const int Analog_Output_Properties_Proprietary[] = { -1 };

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
    return Keylist_Count(Object_List);
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
        if ((priority >= 1) && (priority <= BACNET_MAX_PRIORITY) &&
            (value >= 0.0) && (value <= 100.0)) {
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
        pObject->Max_Pres_Value = value;
        status = true;
    }

    return status;
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
    const bool in_alarm = false;
    const bool fault = false;
    const bool overridden = false;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        status = cov_value_list_encode_real(value_list, pObject->Prior_Value,
            in_alarm, fault, overridden, pObject->Out_Of_Service);
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
        pObject->COV_Increment = value;
    }
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
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
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
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
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
    switch (wp_data->object_property) {
        case PROP_PRESENT_VALUE:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_REAL);
            if (status) {
                status =
                    Analog_Output_Present_Value_Write(wp_data->object_instance,
                        value.type.Real, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code);
            } else {
                status = write_property_type_valid(wp_data, &value,
                    BACNET_APPLICATION_TAG_NULL);
                if (status) {
                    status = Analog_Output_Present_Value_Relinquish_Write(
                        wp_data->object_instance, wp_data->priority,
                        &wp_data->error_class, &wp_data->error_code);
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
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_TYPE:
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
        case PROP_UNITS:
        case PROP_RELIABILITY:
        case PROP_PRIORITY_ARRAY:
        case PROP_RELINQUISH_DEFAULT:
        case PROP_MAX_PRES_VALUE:
        case PROP_MIN_PRES_VALUE:
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
#endif
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }

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
    struct object_data Object;
};

static void uci_list(const char *sec_idx,
	struct itr_ctx *ictx)
{
	int disable,idx;
	disable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx,"default") == 0)
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

    pObject->Reliability = RELIABILITY_NO_FAULT_DETECTED;
    pObject->Overridden = false;
    for (priority = 0; priority < BACNET_MAX_PRIORITY; priority++) {
        pObject->Relinquished[priority] = true;
        pObject->Priority_Array[priority] = 0.0;
    }
    pObject->Relinquish_Default = 0.0;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "cov_increment");
    if (option)
        pObject->COV_Increment = strtof(option,(char **) NULL);
    else
        pObject->COV_Increment = ictx->Object.COV_Increment;
    pObject->Prior_Value = 0.0;
    pObject->Units = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "si_unit", ictx->Object.Units);
    pObject->Out_Of_Service = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "Out_Of_Service", false);
    pObject->Changed = false;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "min_value");
    if (option)
        pObject->Min_Pres_Value = strtof(option,(char **) NULL);
    else
        pObject->Min_Pres_Value = ictx->Object.Min_Pres_Value;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "max_value");
    if (option)
        pObject->Max_Pres_Value = strtof(option,(char **) NULL);
    else
        pObject->Max_Pres_Value = ictx->Object.Max_Pres_Value;
    option = ucix_get_option(ictx->ctx, ictx->section, sec_idx, "value");
    if (option)
        pObject->Priority_Array[BACNET_MAX_PRIORITY-1] = strtof(option,(char **) NULL);

    /* add to list */
    index = Keylist_Data_Add(Object_List, idx, pObject);
    if (index >= 0) {
        Device_Inc_Database_Revision();
    }
    //Analog_Output_Name_Set(idx,
    //    ucix_get_option(ictx->ctx, ictx->section, sec_idx, "name"));
    //Analog_Output_Description_Set(idx,
    //    ucix_get_option(ictx->ctx, ictx->section, sec_idx, "description"));
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
    struct object_data tObject;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;
    option = ucix_get_option(ctx, sec, "default", "description");
    if (option)
        if (characterstring_init_ansi(&option_str, option))
            tObject.Description = strndup(option,option_str.length);

    tObject.COV_Increment = strtof(ucix_get_option(ctx, sec, "default", "cov_increment"),(char **) NULL);
    tObject.Units = ucix_get_option_int(ctx, sec, "default", "si_unit", 0);
    tObject.Min_Pres_Value = strtof(ucix_get_option(ctx, sec, "default", "min_value"),(char **) NULL);
    tObject.Max_Pres_Value = strtof(ucix_get_option(ctx, sec, "default", "max_value"),(char **) NULL);
    struct itr_ctx itr_m;
	itr_m.section = sec;
	itr_m.ctx = ctx;
	itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void *)uci_list,&itr_m);
    ucix_cleanup(ctx);
}
