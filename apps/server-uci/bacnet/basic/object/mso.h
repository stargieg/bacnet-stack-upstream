/**
 * @file
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2005
 * @brief API for basic Multi-State Output objects
 * @copyright SPDX-License-Identifier: MIT
 */
#ifndef BACNET_BASIC_OBJECT_MULTI_STATE_OUTPUT_H
#define BACNET_BASIC_OBJECT_MULTI_STATE_OUTPUT_H

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
 * @param  old_value - multistate preset-value prior to write
 * @param  value - multistate preset-value of the write
 */
typedef void (*multistate_output_write_present_value_callback)(
    uint32_t object_instance, uint32_t old_value, uint32_t value);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

BACNET_STACK_EXPORT
void Multistate_Output_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary);
BACNET_STACK_EXPORT
bool Multistate_Output_Valid_Instance(uint32_t object_instance);
BACNET_STACK_EXPORT
unsigned Multistate_Output_Count(void);
BACNET_STACK_EXPORT
uint32_t Multistate_Output_Index_To_Instance(unsigned index);
BACNET_STACK_EXPORT
unsigned Multistate_Output_Instance_To_Index(uint32_t object_instance);

BACNET_STACK_EXPORT
int Multistate_Output_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata);

BACNET_STACK_EXPORT
bool Multistate_Output_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data);

BACNET_STACK_EXPORT
bool Multistate_Output_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name);
BACNET_STACK_EXPORT
bool Multistate_Output_Name_Set(uint32_t object_instance, const char *new_name);
BACNET_STACK_EXPORT
const char *Multistate_Output_Name_ASCII(uint32_t object_instance);

BACNET_STACK_EXPORT
uint32_t Multistate_Output_Present_Value(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Present_Value_Set(
    uint32_t object_instance, uint32_t value, unsigned priority);
BACNET_STACK_EXPORT
bool Multistate_Output_Present_Value_Relinquish(
    uint32_t instance, unsigned priority);
BACNET_STACK_EXPORT
unsigned Multistate_Output_Present_Value_Priority(uint32_t object_instance);
BACNET_STACK_EXPORT
void Multistate_Output_Write_Present_Value_Callback_Set(
    multistate_output_write_present_value_callback cb);

BACNET_STACK_EXPORT
uint32_t Multistate_Output_Relinquish_Default(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Relinquish_Default_Set(
    uint32_t object_instance,
    uint32_t value);

BACNET_STACK_EXPORT
unsigned Multistate_Output_Event_State(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Event_State_Set(
    uint32_t object_instance,
    unsigned state);

BACNET_STACK_EXPORT
bool Multistate_Output_Change_Of_Value(uint32_t instance);
BACNET_STACK_EXPORT
void Multistate_Output_Change_Of_Value_Clear(uint32_t instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list);

BACNET_STACK_EXPORT
bool Multistate_Output_Out_Of_Service(uint32_t object_instance);
BACNET_STACK_EXPORT
void Multistate_Output_Out_Of_Service_Set(uint32_t object_instance, bool value);

BACNET_STACK_EXPORT
const char *Multistate_Output_Description(uint32_t instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Description_Set(
    uint32_t object_instance, const char *text_string);

BACNET_STACK_EXPORT
bool Multistate_Output_State_Text_List_Set(
    uint32_t object_instance, const char *state_text_list);
BACNET_STACK_EXPORT
bool Multistate_Output_State_Text_Set(
    uint32_t object_instance, uint32_t state_index, BACNET_CHARACTER_STRING *new_name);
BACNET_STACK_EXPORT
bool Multistate_Output_Max_States_Set(
    uint32_t instance, uint32_t max_states_requested);
BACNET_STACK_EXPORT
uint32_t Multistate_Output_Max_States(uint32_t instance);
BACNET_STACK_EXPORT
char *
Multistate_Output_State_Text(uint32_t object_instance, uint32_t state_index);

BACNET_STACK_EXPORT
BACNET_RELIABILITY Multistate_Output_Reliability(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Reliability_Set(
    uint32_t object_instance, BACNET_RELIABILITY value);

BACNET_STACK_EXPORT
uint32_t Multistate_Output_Relinquish_Default(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Relinquish_Default_Set(
    uint32_t object_instance, uint32_t value);

BACNET_STACK_EXPORT
bool Multistate_Output_Overridden(uint32_t instance);
BACNET_STACK_EXPORT
void Multistate_Output_Overridden_Set(
    uint32_t instance, bool oos_flag);

/* note: header of Intrinsic_Reporting function is required
    even when INTRINSIC_REPORTING is not defined */
BACNET_STACK_EXPORT
void Multistate_Output_Intrinsic_Reporting(uint32_t object_instance);

#if defined(INTRINSIC_REPORTING)
BACNET_STACK_EXPORT
uint32_t Multistate_Output_Feedback_Value(
    uint32_t object_instance);
BACNET_STACK_EXPORT
uint32_t Multistate_Output_Time_Delay(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Time_Delay_Set(
    uint32_t object_instance, uint32_t value);
BACNET_STACK_EXPORT
uint32_t Multistate_Output_Notification_Class(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Notification_Class_Set(
    uint32_t object_instance, uint32_t value);
BACNET_STACK_EXPORT
bool Multistate_Output_Alarm_Value(
    uint32_t object_instance,
    uint32_t state);
BACNET_STACK_EXPORT
bool Multistate_Output_Alarm_Value_Set(
    uint32_t object_instance, uint32_t state, bool value);
BACNET_STACK_EXPORT
uint8_t Multistate_Output_Event_Enable(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Event_Enable_Set(
    uint32_t object_instance, uint8_t value);
BACNET_STACK_EXPORT
bool Multistate_Output_Acked_Transitions(
    uint32_t object_instance, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION]);
BACNET_STACK_EXPORT
uint8_t Multistate_Output_Notify_Type(
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Notify_Type_Set(uint32_t object_instance, uint8_t value);
BACNET_STACK_EXPORT
bool Multistate_Output_Event_Time_Stamps(
    uint32_t object_instance, BACNET_DATE_TIME *value[MAX_BACNET_EVENT_TRANSITION]);

BACNET_STACK_EXPORT
int Multistate_Output_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data);

BACNET_STACK_EXPORT
int Multistate_Output_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code);

BACNET_STACK_EXPORT
int Multistate_Output_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data);
#endif

bool Multistate_Output_Object_Instance_Add(
    uint32_t instance);

BACNET_STACK_EXPORT
uint32_t Multistate_Output_Create(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Multistate_Output_Delete(uint32_t object_instance);
BACNET_STACK_EXPORT
void Multistate_Output_Cleanup(void);

BACNET_STACK_EXPORT
void Multistate_Output_Init(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
