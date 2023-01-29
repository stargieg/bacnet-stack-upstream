/**
 * @file
 * @author Steve Karg
 * @date 2005
 * @brief Binary Input objects, customize for your use
 *
 * @section DESCRIPTION
 *
 * The Binary Input object is a command object, and the present-value
 * property uses a priority array and an enumerated 2-state data type.
 *
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
#ifndef BI_H
#define BI_H

#include <stdbool.h>
#include <stdint.h>
#include "bacnet/config.h"
#include "bacnet/bacnet_stack_exports.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacerror.h"
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
 * @param  old_value - binary preset-value prior to write
 * @param  value - binary preset-value of the write
 */
typedef void (*binary_input_write_present_value_callback)(
    uint32_t object_instance, BACNET_BINARY_PV old_value,
    BACNET_BINARY_PV value);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    BACNET_STACK_EXPORT
    void Binary_Input_Property_Lists(
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);
    BACNET_STACK_EXPORT
    bool Binary_Input_Valid_Instance(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    unsigned Binary_Input_Count(
        void);
    BACNET_STACK_EXPORT
    uint32_t Binary_Input_Index_To_Instance(
        unsigned index);
    BACNET_STACK_EXPORT
    unsigned Binary_Input_Instance_To_Index(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Object_Instance_Add(
        uint32_t instance);

    BACNET_STACK_EXPORT
    BACNET_BINARY_PV Binary_Input_Present_Value(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Present_Value_Set(
        uint32_t instance,
        BACNET_BINARY_PV binary_input,
        unsigned priority);
    BACNET_STACK_EXPORT
    bool Binary_Input_Present_Value_Relinquish(
        uint32_t instance,
        unsigned priority);
    BACNET_STACK_EXPORT
    unsigned Binary_Input_Present_Value_Priority(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    void Binary_Input_Write_Present_Value_Callback_Set(
        binary_input_write_present_value_callback cb);

    BACNET_STACK_EXPORT
    BACNET_BINARY_PV Binary_Input_Relinquish_Default(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Relinquish_Default_Set(
        uint32_t object_instance,
        BACNET_BINARY_PV value);

    BACNET_STACK_EXPORT
    unsigned Binary_Input_Event_State(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Event_State_Set(
        uint32_t object_instance,
        unsigned state);

    BACNET_STACK_EXPORT
    bool Binary_Input_Change_Of_Value(
        uint32_t instance);
    BACNET_STACK_EXPORT
    void Binary_Input_Change_Of_Value_Clear(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Encode_Value_List(
        uint32_t object_instance,
        BACNET_PROPERTY_VALUE * value_list);

    BACNET_STACK_EXPORT
    bool Binary_Input_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);
    BACNET_STACK_EXPORT
    bool Binary_Input_Name_Set(
        uint32_t object_instance,
        char *new_name);

    BACNET_STACK_EXPORT
    char *Binary_Input_Description(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Description_Set(
        uint32_t instance,
        char *new_name);

    BACNET_STACK_EXPORT
    char *Binary_Input_Inactive_Text(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Inactive_Text_Set(
        uint32_t instance,
        char *new_name);
    BACNET_STACK_EXPORT
    char *Binary_Input_Active_Text(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Active_Text_Set(
        uint32_t instance,
        char *new_name);
    BACNET_STACK_EXPORT
    BACNET_POLARITY Binary_Input_Polarity(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Polarity_Set(
        uint32_t object_instance,
        BACNET_POLARITY polarity);

    BACNET_STACK_EXPORT
    bool Binary_Input_Out_Of_Service(
        uint32_t instance);
    BACNET_STACK_EXPORT
    void Binary_Input_Out_Of_Service_Set(
        uint32_t object_instance,
        bool value);

    BACNET_STACK_EXPORT
    bool Binary_Input_Overridden(
        uint32_t instance);
    BACNET_STACK_EXPORT
    void Binary_Input_Overridden_Set(
        uint32_t instance,
        bool oos_flag);

    BACNET_STACK_EXPORT
    bool Binary_Input_Reliability_Set(
        uint32_t object_instance, BACNET_RELIABILITY value);
    BACNET_STACK_EXPORT
    BACNET_RELIABILITY Binary_Input_Reliability(
        uint32_t object_instance);

    BACNET_STACK_EXPORT
    int Binary_Input_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata);
    BACNET_STACK_EXPORT
    bool Binary_Input_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data);
    /* note: header of Intrinsic_Reporting function is required
       even when INTRINSIC_REPORTING is not defined */
    BACNET_STACK_EXPORT
    void Binary_Input_Intrinsic_Reporting(
        uint32_t object_instance);

#if defined(INTRINSIC_REPORTING)
    BACNET_STACK_EXPORT
    BACNET_BINARY_PV Binary_Input_Feedback_Value(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    uint32_t Binary_Input_Time_Delay(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Time_Delay_Set(
        uint32_t object_instance, uint32_t value);
    BACNET_STACK_EXPORT
    uint32_t Binary_Input_Notification_Class(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Notification_Class_Set(
        uint32_t object_instance, uint32_t value);
    BACNET_STACK_EXPORT
    BACNET_BINARY_PV Binary_Input_Alarm_Value(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Alarm_Value_Set(
        uint32_t object_instance, BACNET_BINARY_PV value);
    BACNET_STACK_EXPORT
    uint8_t Binary_Input_Event_Enable(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Event_Enable_Set(
        uint32_t object_instance, uint8_t value);
    BACNET_STACK_EXPORT
    bool Binary_Input_Acked_Transitions(
        uint32_t object_instance, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION]);
    BACNET_STACK_EXPORT
    uint8_t Binary_Input_Notify_Type(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Notify_Type_Set(uint32_t object_instance, uint8_t value);
    BACNET_STACK_EXPORT
    bool Binary_Input_Event_Time_Stamps(
        uint32_t object_instance, BACNET_DATE_TIME *value[MAX_BACNET_EVENT_TRANSITION]);

    BACNET_STACK_EXPORT
    int Binary_Input_Event_Information(
        unsigned index,
        BACNET_GET_EVENT_INFORMATION_DATA * getevent_data);

    BACNET_STACK_EXPORT
    int Binary_Input_Alarm_Ack(
        BACNET_ALARM_ACK_DATA * alarmack_data,
        BACNET_ERROR_CODE * error_code);

    BACNET_STACK_EXPORT
    int Binary_Input_Alarm_Summary(
        unsigned index,
        BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data);
#endif

    BACNET_STACK_EXPORT
    bool Binary_Input_Create(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Binary_Input_Delete(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    void Binary_Input_Cleanup(
        void);
    BACNET_STACK_EXPORT
    void Binary_Input_Init(
        void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
