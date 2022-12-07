/**************************************************************************
*
* Copyright (C) 2011 Krzysztof Malorny <malornykrzysztof@gmail.com>
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
*
*********************************************************************/
#ifndef NC_H
#define NC_H

#include "bacnet/bacnet_stack_exports.h"
#include "bacnet/event.h"


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */


#define NC_RESCAN_RECIPIENTS_SECS   60

/* max "length" of recipient_list */
#define NC_MAX_RECIPIENTS 10
/* Recipient types */
    typedef enum {
        RECIPIENT_TYPE_NOTINITIALIZED = 0,
        RECIPIENT_TYPE_DEVICE = 1,
        RECIPIENT_TYPE_ADDRESS = 2
    } NC_RECIPIENT_TYPE;


#if defined(INTRINSIC_REPORTING)
/* BACnetRecipient structure */
/*
BACnetRecipient ::= CHOICE {
    device [0] BACnetObjectIdentifier,
    address [1] BACnetAddress
}
*/
    typedef struct BACnet_Recipient {
        uint8_t RecipientType;  /* Type of Recipient */
        union {
            uint32_t DeviceIdentifier;
            BACNET_ADDRESS Address;
        } _;
    } BACNET_RECIPIENT;


/* BACnetDestination structure */
    typedef struct BACnet_Destination {
        uint8_t ValidDays;
        BACNET_TIME FromTime;
        BACNET_TIME ToTime;
        BACNET_RECIPIENT Recipient;
        uint32_t ProcessIdentifier;
        uint8_t Transitions;
        bool ConfirmedNotify;
    } BACNET_DESTINATION;

/* Indicates whether the transaction has been confirmed */
    typedef struct Acked_info {
        bool bIsAcked;  /* true when transitions is acked */
        BACNET_DATE_TIME Time_Stamp;    /* time stamp of when a alarm was generated */
    } ACKED_INFO;


/* Information needed to send AckNotification */
    typedef struct Ack_Notification {
        bool bSendAckNotify;    /* true if need to send AckNotification */
        uint8_t EventState;
    } ACK_NOTIFICATION;



    BACNET_STACK_EXPORT
    void Notification_Class_Property_Lists(
        const int **pRequired,
        const int **pOptional,
        const int **pProprietary);

    BACNET_STACK_EXPORT
    void Notification_Class_Init(
        void);

    bool Notification_Class_Valid_Instance(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    unsigned Notification_Class_Count(
        void);
    BACNET_STACK_EXPORT
    uint32_t Notification_Class_Index_To_Instance(
        unsigned index);
    BACNET_STACK_EXPORT
    unsigned Notification_Class_Instance_To_Index(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Notification_Class_Object_Name(
        uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);

    BACNET_STACK_EXPORT
    bool Notification_Class_Name_Set(
        uint32_t object_instance,
        char *new_name);
    BACNET_STACK_EXPORT
    char *Notification_Class_Description(
        uint32_t instance);
    BACNET_STACK_EXPORT
    bool Notification_Class_Description_Set(
        uint32_t instance,
        char *new_name);
    BACNET_STACK_EXPORT
    bool Notification_Class_Priority(
        uint32_t object_instance,
        uint8_t value[MAX_BACNET_EVENT_TRANSITION]);
    BACNET_STACK_EXPORT
    bool Notification_Class_Priority_Set(
        uint32_t object_instance,
        uint8_t value[MAX_BACNET_EVENT_TRANSITION],
        uint8_t a);
    BACNET_STACK_EXPORT
    uint8_t Notification_Class_Ack_Required(
        uint32_t object_instance);
    BACNET_STACK_EXPORT
    bool Notification_Class_Ack_Required_Set(
        uint32_t object_instance,
        uint8_t value);
    BACNET_STACK_EXPORT
    BACNET_DESTINATION * Notification_Class_Recipient_List(
        uint32_t object_instance,
        uint8_t b);
    BACNET_STACK_EXPORT
    bool Notification_Class_Recipient_List_Set(
        uint32_t object_instance,
        BACNET_DESTINATION value[NC_MAX_RECIPIENTS]);

    BACNET_STACK_EXPORT
    int Notification_Class_Read_Property(
        BACNET_READ_PROPERTY_DATA * rpdata);

    BACNET_STACK_EXPORT
    bool Notification_Class_Write_Property(
        BACNET_WRITE_PROPERTY_DATA * wp_data);

    BACNET_STACK_EXPORT
    void Notification_Class_Get_Priorities(
        uint32_t Object_Instance,
        uint32_t * pPriorityArray);

    BACNET_STACK_EXPORT
    void Notification_Class_common_reporting_function(
        BACNET_EVENT_NOTIFICATION_DATA * event_data);

    BACNET_STACK_EXPORT
    void Notification_Class_find_recipient(
        void);
#endif /* defined(INTRINSIC_REPORTING) */


#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* NC_H */
