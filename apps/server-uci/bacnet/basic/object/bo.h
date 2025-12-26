/**
 * @file
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2005
 * @brief API for a basic BACnet Binary Output object implementation.
 * The Binary Output object is a commandable object, and the present-value
 * property uses a priority array and an enumerated 2-state data type.
 * @copyright SPDX-License-Identifier: MIT
 */
#ifndef BACNET_BASIC_OBJECT_BINARY_OUTPUT_H
#define BACNET_BASIC_OBJECT_BINARY_OUTPUT_H
#include <stdbool.h>
#include <stdint.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
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
 * @param  old_value - binary preset-value prior to write
 * @param  value - binary preset-value of the write
 */
typedef void (*binary_output_write_present_value_callback)(
    uint32_t object_instance,
    BACNET_BINARY_PV old_value,
    BACNET_BINARY_PV value);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BACNET_STACK_EXPORT
void Binary_Output_Init(void);

BACNET_STACK_EXPORT
void Binary_Output_Property_Lists(
    const int32_t **pRequired,
    const int32_t **pOptional,
    const int32_t **pProprietary);

BACNET_STACK_EXPORT
bool Binary_Output_Valid_Instance(uint32_t object_instance);
BACNET_STACK_EXPORT
unsigned Binary_Output_Count(void);
BACNET_STACK_EXPORT
uint32_t Binary_Output_Index_To_Instance(unsigned index);
BACNET_STACK_EXPORT
unsigned Binary_Output_Instance_To_Index(uint32_t instance);
BACNET_STACK_EXPORT
bool Binary_Output_Object_Instance_Add(uint32_t instance);

BACNET_STACK_EXPORT
bool Binary_Output_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name);
BACNET_STACK_EXPORT
bool Binary_Output_Name_Set(uint32_t object_instance, const char *new_name);
BACNET_STACK_EXPORT
const char *Binary_Output_Name_ASCII(uint32_t object_instance);

BACNET_STACK_EXPORT
const char *Binary_Output_Inactive_Text(uint32_t instance);
BACNET_STACK_EXPORT
bool Binary_Output_Inactive_Text_Set(uint32_t instance, const char *new_name);
BACNET_STACK_EXPORT
const char *Binary_Output_Active_Text(uint32_t instance);
BACNET_STACK_EXPORT
bool Binary_Output_Active_Text_Set(uint32_t instance, const char *new_name);

BACNET_STACK_EXPORT
BACNET_BINARY_PV Binary_Output_Present_Value(uint32_t instance);
BACNET_STACK_EXPORT
bool Binary_Output_Present_Value_Set(
    uint32_t instance, BACNET_BINARY_PV binary_value, unsigned priority);
BACNET_STACK_EXPORT
bool Binary_Output_Present_Value_Relinquish(
    uint32_t instance, unsigned priority);
BACNET_STACK_EXPORT
unsigned Binary_Output_Present_Value_Priority(uint32_t object_instance);

BACNET_STACK_EXPORT
void Binary_Output_Write_Present_Value_Callback_Set(
    binary_output_write_present_value_callback cb);

BACNET_STACK_EXPORT
bool Binary_Output_Out_Of_Service(uint32_t instance);
BACNET_STACK_EXPORT
void Binary_Output_Out_Of_Service_Set(uint32_t object_instance, bool value);

BACNET_STACK_EXPORT
const char *Binary_Output_Event_Message_Text(
    uint32_t object_instance, enum BACnetEventTransitionBits transition);
BACNET_STACK_EXPORT
bool Binary__Output_Event_Message_Text_Custom_Set(
    uint32_t object_instance,
    enum BACnetEventTransitionBits transition,
    const char *custom_text);

BACNET_STACK_EXPORT
const char *Binary_Output_Description(uint32_t instance);
BACNET_STACK_EXPORT
bool Binary_Output_Description_Set(
    uint32_t object_instance, const char *text_string);

BACNET_STACK_EXPORT
BACNET_POLARITY Binary_Output_Polarity(uint32_t instance);
BACNET_STACK_EXPORT
bool Binary_Output_Polarity_Set(
    uint32_t object_instance, BACNET_POLARITY polarity);

BACNET_STACK_EXPORT
bool Binary_Output_Reliability_Set(
    uint32_t object_instance, BACNET_RELIABILITY value);
BACNET_STACK_EXPORT
BACNET_RELIABILITY Binary_Output_Reliability(uint32_t object_instance);

BACNET_STACK_EXPORT
BACNET_BINARY_PV Binary_Output_Relinquish_Default(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Output_Relinquish_Default_Set(
    uint32_t object_instance, BACNET_BINARY_PV value);

BACNET_STACK_EXPORT
bool Binary_Output_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list);
BACNET_STACK_EXPORT
bool Binary_Output_Change_Of_Value(uint32_t instance);
BACNET_STACK_EXPORT
void Binary_Output_Change_Of_Value_Clear(uint32_t instance);

BACNET_STACK_EXPORT
int Binary_Output_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata);
BACNET_STACK_EXPORT
bool Binary_Output_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data);

/* note: header of Intrinsic_Reporting function is required
    even when INTRINSIC_REPORTING is not defined */
BACNET_STACK_EXPORT
void Binary_Output_Intrinsic_Reporting(
    uint32_t object_instance);

#if defined(INTRINSIC_REPORTING)
BACNET_STACK_EXPORT
BACNET_BINARY_PV Binary_Output_Feedback_Value(
    uint32_t object_instance);
BACNET_STACK_EXPORT
uint32_t Binary_Output_Time_Delay(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Output_Time_Delay_Set(
    uint32_t object_instance, uint32_t value);
BACNET_STACK_EXPORT
uint32_t Binary_Output_Notification_Class(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Output_Notification_Class_Set(
    uint32_t object_instance, uint32_t value);
BACNET_STACK_EXPORT
BACNET_BINARY_PV Binary_Output_Alarm_Value(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Output_Alarm_Value_Set(
    uint32_t object_instance, BACNET_BINARY_PV value);
BACNET_STACK_EXPORT
uint8_t Binary_Output_Event_Enable(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Output_Event_Enable_Set(
    uint32_t object_instance, uint8_t value);
BACNET_STACK_EXPORT
bool Binary_Output_Acked_Transitions(
    uint32_t object_instance, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION]);
BACNET_STACK_EXPORT
uint8_t Binary_Output_Notify_Type(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Output_Notify_Type_Set(uint32_t object_instance, uint8_t value);
BACNET_STACK_EXPORT
bool Binary_Output_Event_Time_Stamps(
    uint32_t object_instance, BACNET_DATE_TIME *value[MAX_BACNET_EVENT_TRANSITION]);

BACNET_STACK_EXPORT
int Binary_Output_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data);

BACNET_STACK_EXPORT
int Binary_Output_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code);

BACNET_STACK_EXPORT
int Binary_Output_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data);
#endif

BACNET_STACK_EXPORT
void *Binary_Output_Context_Get(uint32_t object_instance);
BACNET_STACK_EXPORT
void Binary_Output_Context_Set(uint32_t object_instance, void *context);

BACNET_STACK_EXPORT
uint32_t Binary_Output_Create(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Output_Delete(uint32_t object_instance);
BACNET_STACK_EXPORT
void Binary_Output_Cleanup(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
