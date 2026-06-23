/**
 * @file
 * @brief API for basic BACnet Analog Input Object implementation.
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @author Krzysztof Malorny <malornykrzysztof@gmail.com>
 * @date 2005, 2011
 * @copyright SPDX-License-Identifier: MIT
 */
#ifndef BACNET_BASIC_UCI_OBJECT_ANALOG_INPUT_H
#define BACNET_BASIC_UCI_OBJECT_ANALOG_INPUT_H
#define BACNET_BASIC_OBJECT_ANALOG_INPUT_H
#include <stdbool.h>
#include <stdint.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/rp.h"
#include "bacnet/wp.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#include "bacnet/getevent.h"
#include "bacnet/alarm_ack.h"
#include "bacnet/get_alarm_sum.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/* Default_Object_Table Start */
BACNET_STACK_EXPORT
void Analog_Input_Init(void);
BACNET_STACK_EXPORT
unsigned Analog_Input_Count(void);
BACNET_STACK_EXPORT
uint32_t Analog_Input_Index_To_Instance(unsigned index);
BACNET_STACK_EXPORT
bool Analog_Input_Valid_Instance(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Analog_Input_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name);
BACNET_STACK_EXPORT
int Analog_Input_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata);
BACNET_STACK_EXPORT
bool Analog_Input_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data);
BACNET_STACK_EXPORT
void Analog_Input_Property_Lists(
    const int32_t **pRequired,
    const int32_t **pOptional,
    const int32_t **pProprietary);
/* BACNET_STACK_EXPORT */
/* ReadRangeInfo */
/* BACNET_STACK_EXPORT */
/* Iterator */
BACNET_STACK_EXPORT
bool Analog_Input_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list);
BACNET_STACK_EXPORT
bool Analog_Input_Change_Of_Value(uint32_t instance);
BACNET_STACK_EXPORT
void Analog_Input_Change_Of_Value_Clear(uint32_t instance);
/* note: header of Intrinsic_Reporting function is required
   even when INTRINSIC_REPORTING is not defined */
BACNET_STACK_EXPORT
void Analog_Input_Intrinsic_Reporting(uint32_t object_instance);
/* BACNET_STACK_EXPORT */
/* Add_List_Element */
/* BACNET_STACK_EXPORT */
/* Remove_List_Element */
BACNET_STACK_EXPORT
uint32_t Analog_Input_Create(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Analog_Input_Delete(uint32_t object_instance);
/* BACNET_STACK_EXPORT */
/* Timer */
BACNET_STACK_EXPORT
void Analog_Input_Writable_Property_List(
    uint32_t object_instance, const int32_t **properties);
/* Default_Object_Table End */

#if defined(INTRINSIC_REPORTING)
/* event handler start */
BACNET_STACK_EXPORT
int Analog_Input_Event_Information(
    unsigned index, BACNET_GET_EVENT_INFORMATION_DATA *getevent_data);
BACNET_STACK_EXPORT
int Analog_Input_Alarm_Ack(
    BACNET_ALARM_ACK_DATA *alarmack_data, BACNET_ERROR_CODE *error_code);
BACNET_STACK_EXPORT
int Analog_Input_Alarm_Summary(
    unsigned index, BACNET_GET_ALARM_SUMMARY_DATA *getalarm_data);
/* event handler end */
#endif
BACNET_STACK_EXPORT
void *Analog_Input_Context_Get(uint32_t object_instance);
BACNET_STACK_EXPORT
void Analog_Input_Context_Set(uint32_t object_instance, void *context);
BACNET_STACK_EXPORT
void Analog_Input_Cleanup(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
