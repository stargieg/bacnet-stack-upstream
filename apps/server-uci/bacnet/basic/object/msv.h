/**
 * @file
 * @author Steve Karg
 * @date 2005
 * @brief Multi-State Output objects, customize for your use
 *
 * @section DESCRIPTION
 *
 * The Multi-State object is an object with a present-value that
 * uses an integer data type with a sequence of 1 to N values.
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
#ifndef MULTISTATE_VALUE_H
#define MULTISTATE_VALUE_H

#include <stdbool.h>
#include <stdint.h>
#include "bacnet/config.h"
#include "bacnet/bacnet_stack_exports.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacerror.h"
#include "bacnet/cov.h"
#include "bacnet/rp.h"
#include "bacnet/wp.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#include "bacnet/alarm_ack.h"
#include "bacnet/getevent.h"
#include "bacnet/get_alarm_sum.h"
#endif

/**
 * @brief Callback for gateway write present value request
 * @param  object_instance - object-instance number of the object
 * @param  old_value - multistate preset-value prior to write
 * @param  value - multistate preset-value of the write
 */
typedef void (*multistate_value_write_present_value_callback)(
    uint32_t object_instance, uint32_t old_value,
    uint32_t value);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    BACNET_STACK_EXPORT
    void Multistate_Value_Property_Lists(
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Valid_Instance(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    unsigned Multistate_Value_Count(
        void);
    BACNET_STACK_EXPORT
    uint32_t Multistate_Value_Index_To_Instance(
        unsigned index);
    BACNET_STACK_EXPORT
    unsigned Multistate_Value_Instance_To_Index(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Object_Instance_Add(
        uint32_t instance);

    BACNET_STACK_EXPORT
    uint32_t Multistate_Value_Present_Value(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Present_Value_Set(
        uint32_t object_instance,
        uint32_t value,
        unsigned priority);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Present_Value_Relinquish(
        uint32_t instance,
        unsigned priority);
    BACNET_STACK_EXPORT
    unsigned Multistate_Value_Present_Value_Priority(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    void Multistate_Value_Write_Present_Value_Callback_Set(
        multistate_value_write_present_value_callback cb);

    BACNET_STACK_EXPORT
    uint32_t Multistate_Value_Relinquish_Default(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Relinquish_Default_Set(
        uint32_t object_instance,
        uint32_t value);

    BACNET_STACK_EXPORT
    unsigned Multistate_Value_Event_State(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Event_State_Set(
        uint32_t object_instance,
        unsigned state);

    BACNET_STACK_EXPORT
    bool Multistate_Value_Change_Of_Value(
        uint32_t instance);
    BACNET_STACK_EXPORT
    void Multistate_Value_Change_Of_Value_Clear(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Encode_Value_List(
        uint32_t object_instance,
        BACNET_PROPERTY_VALUE * value_list);

    BACNET_STACK_EXPORT
    bool Multistate_Value_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Name_Set(
        uint32_t object_instance,
        char *new_name);

    BACNET_STACK_EXPORT
    char *Multistate_Value_Description(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Description_Set(
        uint32_t object_instance,
        char *new_name);

    BACNET_STACK_EXPORT
    uint32_t Multistate_Value_Max_States(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Max_States_Set(
        uint32_t instance,
        uint32_t count);
    BACNET_STACK_EXPORT
    char *Multistate_Value_State_Text(
        uint32_t object_instance,
        uint32_t state_index);
    BACNET_STACK_EXPORT
    bool Multistate_Value_State_Text_Set(
        uint32_t object_instance,
        uint32_t state_index,
        BACNET_CHARACTER_STRING *new_name);

    BACNET_STACK_EXPORT
    uint32_t Multistate_Value_Relinquish_Default(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Relinquish_Default_Set(
        uint32_t object_instance,
        uint32_t value);

    BACNET_STACK_EXPORT
    bool Multistate_Value_Out_Of_Service(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    void Multistate_Value_Out_Of_Service_Set(
        uint32_t object_instance,
        bool value);

    BACNET_STACK_EXPORT
    bool Multistate_Value_Overridden(
        uint32_t instance);
    BACNET_STACK_EXPORT
    void Multistate_Value_Overridden_Set(
        uint32_t instance,
        bool oos_flag);

    BACNET_STACK_EXPORT
    bool Multistate_Value_Reliability_Set(
        uint32_t object_instance,
        BACNET_RELIABILITY value);
    BACNET_STACK_EXPORT
    BACNET_RELIABILITY Multistate_Value_Reliability(
        uint32_t object_instance);

    BACNET_STACK_EXPORT
    int Multistate_Value_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data);
    /* note: header of Intrinsic_Reporting function is required
       even when INTRINSIC_REPORTING is not defined */
    BACNET_STACK_EXPORT
    void Multistate_Value_Intrinsic_Reporting(
        uint32_t object_instance);

#if defined(INTRINSIC_REPORTING)
    BACNET_STACK_EXPORT
    uint32_t Multistate_Value_Time_Delay(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Time_Delay_Set(
        uint32_t object_instance, uint32_t value);
    BACNET_STACK_EXPORT
    uint32_t Multistate_Value_Notification_Class(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Notification_Class_Set(
        uint32_t object_instance, uint32_t value);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Alarm_Value(
        uint32_t object_instance,
        uint32_t state);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Alarm_Value_Set(
        uint32_t object_instance, uint32_t state, bool value);
    BACNET_STACK_EXPORT
    uint8_t Multistate_Value_Event_Enable(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Event_Enable_Set(
        uint32_t object_instance, uint8_t value);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Acked_Transitions(
        uint32_t object_instance, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION]);
    BACNET_STACK_EXPORT
    uint8_t Multistate_Value_Notify_Type(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Notify_Type_Set(uint32_t object_instance, uint8_t value);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Event_Time_Stamps(
        uint32_t object_instance, BACNET_DATE_TIME *value[MAX_BACNET_EVENT_TRANSITION]);

    BACNET_STACK_EXPORT
    int Multistate_Value_Event_Information(
        unsigned index,
        BACNET_GET_EVENT_INFORMATION_DATA * getevent_data);

    BACNET_STACK_EXPORT
    int Multistate_Value_Alarm_Ack(
        BACNET_ALARM_ACK_DATA * alarmack_data,
        BACNET_ERROR_CODE * error_code);

    BACNET_STACK_EXPORT
    int Multistate_Value_Alarm_Summary(
        unsigned index,
        BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data);
#endif

    BACNET_STACK_EXPORT
    bool Multistate_Value_Create(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Multistate_Value_Delete(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    void Multistate_Value_Cleanup(
        void);
    BACNET_STACK_EXPORT
    void Multistate_Value_Init(
        void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
