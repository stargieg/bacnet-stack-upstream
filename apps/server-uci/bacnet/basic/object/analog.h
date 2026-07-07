/**
 * @file
 * @brief BACnet Basic Stack initialization and basic task handler
 * @author Steve Karg <skarg@users.sourceforge.net>
 * @date April 2024
 * @copyright SPDX-License-Identifier: MIT
 */
#ifndef BACNET_OBJECT_ANALOG_H
#define BACNET_OBJECT_ANALOG_H

#include <stdint.h>
#include <stdbool.h>
#include "bacnet/bacdef.h"
#if defined(INTRINSIC_REPORTING)
#include "bacnet/basic/object/nc.h"
#endif


typedef struct object_data *(*bacnet_get_pObject)(
    uint32_t object_instance);

/**
 * @brief Encode a BACnetARRAY property element; a function template
 * @param get_pObject [in] function to get a pointer to Object_List
 * @param object_instance [in] BACnet network port object instance number
 * @param array_index [in] array index requested:
 *    0 to N for individual array members
 * @param apdu [out] Buffer in which the APDU contents are built, or NULL to
 * return the length of buffer if it had been built
 * @return The length of the apdu encoded or
 *   BACNET_STATUS_ERROR for ERROR_CODE_INVALID_ARRAY_INDEX
 */
typedef int (*bacnet_array_property_element_encode_function_object)(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX array_index,
    uint8_t *apdu);

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

typedef struct object_data {
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
    BACNET_ENGINEERING_UNITS Units;
    uint8_t Reliability;
    const char *Object_Name;
    const char *Description;
    void *Context;
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State : 3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    float High_Limit;
    float Low_Limit;
    float Feedback_Value;
    float Deadband;
    unsigned Limit_Enable : 2;
    unsigned Event_Enable : 3;
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
} OBJECT_DATA_ANALOG;

typedef struct object_data_t {
    bool Out_Of_Service : 1;
    const char *COV_Increment;
    const char *Prior_Value;
    const char *Relinquish_Default;
    const char *Min_Pres_Value;
    const char *Max_Pres_Value;
    const char *Resolution;
    BACNET_ENGINEERING_UNITS Units;
    uint8_t Reliability;
    const char *Object_Name;
    const char *Description;
#if defined(INTRINSIC_REPORTING)
    unsigned Event_State : 3;
    uint32_t Time_Delay;
    uint32_t Notification_Class;
    const char *High_Limit;
    const char *Low_Limit;
    const char *Deadband;
    unsigned Limit_Enable : 2;
    unsigned Event_Enable : 3;
    unsigned Event_Detection_Enable : 1;
    unsigned Notify_Type : 1;
    const char *Event_Message_Texts;
    const char *Event_Message_Texts_Custom;
#endif /* INTRINSIC_REPORTING */
} OBJECT_DATA_ANALOG_T;

BACNET_STACK_EXPORT
int bacnet_array_encode_analog(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX array_index,
    bacnet_array_property_element_encode_function_object encoder,
    BACNET_UNSIGNED_INTEGER array_size,
    uint8_t *apdu,
    int max_apdu);
BACNET_STACK_EXPORT
float limit_value_by_resolution(float value_f, float resolution);
BACNET_STACK_EXPORT
bool Analog_Object_Name(
    const struct object_data *pObject, BACNET_CHARACTER_STRING *object_name);
BACNET_STACK_EXPORT
const char *Analog_Description(
    const struct object_data *pObject);
BACNET_STACK_EXPORT
float Analog_Present_Value(
    const struct object_data *pObject);
BACNET_STACK_EXPORT
unsigned Analog_Present_Value_Priority(
    const struct object_data *pObject);
BACNET_STACK_EXPORT
bool Analog_Object_Fault(
    const struct object_data *pObject);
BACNET_STACK_EXPORT
bool Analog_Present_Value_Set(
    struct object_data *pObject, float value, unsigned priority);
BACNET_STACK_EXPORT
void Analog_COV_Detect(
    struct object_data *pObject, float value);
BACNET_STACK_EXPORT
int Analog_Priority_Array_Encode(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX index,
    uint8_t *apdu);
BACNET_STACK_EXPORT
bool Analog_Encode_Value_List(
    struct object_data *pObject,
    BACNET_PROPERTY_VALUE *value_list);
#if defined(INTRINSIC_REPORTING)
BACNET_STACK_EXPORT
const char *Analog_Event_Message_Text(
    bacnet_get_pObject get_pObject,
    const uint32_t object_instance,
    const enum BACnetEventTransitionBits transition);
BACNET_STACK_EXPORT
int Analog_Event_Time_Stamps_Encode(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX index,
    uint8_t *apdu);
BACNET_STACK_EXPORT
int Analog_Event_Message_Texts_Encode(
    bacnet_get_pObject get_pObject,
    uint32_t object_instance,
    BACNET_ARRAY_INDEX index,
    uint8_t *apdu);
BACNET_STACK_EXPORT
const char *Analog_Event_Message(
    struct object_data *pObject,
    enum BACnetEventTransitionBits transition,
    const char *default_text);
BACNET_STACK_EXPORT
void Analog_Intrinsic_Reporting(
    struct object_data *pObject,
    BACNET_OBJECT_TYPE Object_Type,
    uint32_t object_instance);
BACNET_STACK_EXPORT
int Analog_Event_Information(
    struct object_data *pObject,
    BACNET_OBJECT_TYPE Object_Type,
    uint32_t object_instance,
    BACNET_GET_EVENT_INFORMATION_DATA *getevent_data);
BACNET_STACK_EXPORT
int Analog_Alarm_Ack(
    struct object_data *pObject,
    BACNET_ALARM_ACK_DATA *alarmack_data,
    BACNET_ERROR_CODE *error_code);
BACNET_STACK_EXPORT
int Analog_Alarm_Summary(
    struct object_data *pObject,
    BACNET_OBJECT_TYPE Object_Type,
    uint32_t object_instance,
    BACNET_GET_ALARM_SUMMARY_DATA *getalarm_data);
BACNET_STACK_EXPORT
bool Analog_Acked_Transitions(
    struct object_data *pObject, ACKED_INFO *value[MAX_BACNET_EVENT_TRANSITION]);
#endif
BACNET_STACK_EXPORT
bool Analog_Present_Value_Write(
    struct object_data *pObject, float value, uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code);
bool Analog_Present_Value_Relinquish(
    struct object_data *pObject, unsigned priority);
BACNET_STACK_EXPORT
bool Analog_Present_Value_Relinquish_Write(
    struct object_data *pObject, uint8_t priority,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code);
BACNET_STACK_EXPORT
void Analog_Out_Of_Service_Set(
    struct object_data *pObject, bool value);
BACNET_STACK_EXPORT
void Analog_COV_Increment_Set(
    struct object_data *pObject, float value);
BACNET_STACK_EXPORT
bool Analog_Name_Set(
    struct object_data *pObject,
    const char *new_name,
    BACNET_OBJECT_TYPE Object_Type,
    uint32_t object_instance);
BACNET_STACK_EXPORT
bool Analog_Reliability_Set(
    struct object_data *pObject, BACNET_RELIABILITY value);
BACNET_STACK_EXPORT
bool Analog_Relinquish_Default_Set(
    struct object_data *pObject, float value);
BACNET_STACK_EXPORT
bool Analog_Max_Pres_Value_Set(
    struct object_data *pObject, float value);
BACNET_STACK_EXPORT
bool Analog_Min_Pres_Value_Set(
    struct object_data *pObject, float value);
BACNET_STACK_EXPORT
void Analog_Overridden_Set(
    struct object_data *pObject, bool value);
#if defined(INTRINSIC_REPORTING)
BACNET_STACK_EXPORT
bool Analog_High_Limit_Set(
    struct object_data *pObject, float value);
BACNET_STACK_EXPORT
bool Analog_Low_Limit_Set(
    struct object_data *pObject, float value);
BACNET_STACK_EXPORT
bool Analog_Deadband_Set(
    struct object_data *pObject, float value);
BACNET_STACK_EXPORT
void Analog_Reset_Event_Properties(
    struct object_data *pObject);
BACNET_STACK_EXPORT
bool Analog_Event_Detection_Enable_Set(
    struct object_data *pObject, bool value);

#endif //INTRINSIC_REPORTING


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
