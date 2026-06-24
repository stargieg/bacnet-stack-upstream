/**
 * @file
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date 2006
 * @brief API for a basic BACnet Binary Value object implementation.
 * Binary Value objects are input objects with a present-value that
 * uses an enumerated two state active/inactive data type.
 * @copyright SPDX-License-Identifier: MIT
 */
#ifndef BACNET_BASIC_UCI_OBJECT_BINARY_VALUE_H
#define BACNET_BASIC_UCI_OBJECT_BINARY_VALUE_H
#define BACNET_BASIC_OBJECT_BINARY_VALUE_H
#include <stdbool.h>
#include <stdint.h>
/* BACnet Stack defines - first */
#include "bacnet/bacdef.h"
/* BACnet Stack API */
#include "bacnet/bacerror.h"
#include "bacnet/rp.h"
#include "bacnet/wp.h"

#if (INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#include "bacnet/getevent.h"
#include "bacnet/alarm_ack.h"
#include "bacnet/get_alarm_sum.h"
#endif
/**
 * @brief Callback for gateway write present value request
 * @param  object_instance - object-instance number of the object
 * @param  old_value - binary preset-value prior to write
 * @param  value - binary preset-value of the write
 */
typedef void (*binary_value_write_present_value_callback)(
    uint32_t object_instance,
    BACNET_BINARY_PV old_value,
    BACNET_BINARY_PV value);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
/* Default_Object_Table Start */
BACNET_STACK_EXPORT
void Binary_Value_Init(void);
BACNET_STACK_EXPORT
unsigned Binary_Value_Count(void);
BACNET_STACK_EXPORT
uint32_t Binary_Value_Index_To_Instance(unsigned index);
BACNET_STACK_EXPORT
bool Binary_Value_Valid_Instance(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Value_Object_Name(
    uint32_t object_instance, BACNET_CHARACTER_STRING *object_name);
BACNET_STACK_EXPORT
int Binary_Value_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata);
BACNET_STACK_EXPORT
bool Binary_Value_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data);
BACNET_STACK_EXPORT
void Binary_Value_Property_Lists(
    const int32_t **pRequired,
    const int32_t **pOptional,
    const int32_t **pProprietary);
/* BACNET_STACK_EXPORT */
/* ReadRangeInfo */
/* BACNET_STACK_EXPORT */
/* Iterator */
BACNET_STACK_EXPORT
bool Binary_Value_Encode_Value_List(
    uint32_t object_instance, BACNET_PROPERTY_VALUE *value_list);
BACNET_STACK_EXPORT
bool Binary_Value_Change_Of_Value(uint32_t instance);
BACNET_STACK_EXPORT
void Binary_Value_Change_Of_Value_Clear(uint32_t instance);
/* note: header of Intrinsic_Reporting function is required
   even when INTRINSIC_REPORTING is not defined */
BACNET_STACK_EXPORT
void Binary_Value_Intrinsic_Reporting(uint32_t object_instance);
/* BACNET_STACK_EXPORT */
/* Add_List_Element */
/* BACNET_STACK_EXPORT */
/* Remove_List_Element */
BACNET_STACK_EXPORT
uint32_t Binary_Value_Create(uint32_t object_instance);
BACNET_STACK_EXPORT
bool Binary_Value_Delete(uint32_t object_instance);
/* BACNET_STACK_EXPORT */
/* Timer */
BACNET_STACK_EXPORT
void Binary_Value_Writable_Property_List(
    uint32_t object_instance, const int32_t **properties);
/* Default_Object_Table End */

#if defined(INTRINSIC_REPORTING)
/* event handler start */
BACNET_STACK_EXPORT
int Binary_Value_Event_Information(
    unsigned index, BACNET_GET_EVENT_INFORMATION_DATA *getevent_data);
BACNET_STACK_EXPORT
int Binary_Value_Alarm_Ack(
    BACNET_ALARM_ACK_DATA *alarmack_data, BACNET_ERROR_CODE *error_code);
BACNET_STACK_EXPORT
int Binary_Value_Alarm_Summary(
    unsigned index, BACNET_GET_ALARM_SUMMARY_DATA *getalarm_data);
/* event handler end */
#endif
BACNET_STACK_EXPORT
void *Binary_Value_Context_Get(uint32_t object_instance);
BACNET_STACK_EXPORT
void Binary_Value_Context_Set(uint32_t object_instance, void *context);
BACNET_STACK_EXPORT
void Binary_Value_Cleanup(void);

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
