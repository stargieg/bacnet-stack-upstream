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
 *   Additional changes, Copyright (c) 2018 Ed Hague <edward@bac-test.com>
 *
 *   2018.06.17 -    Attempting to write to Object_Name returned
 *UNKNOWN_PROPERTY. Now returns WRITE_ACCESS_DENIED
 *
 *********************************************************************/

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bacnet/basic/binding/address.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacdcode.h"
#include "bacnet/bacenum.h"
#include "bacnet/bacapp.h"
#include "bacnet/basic/services.h"
#include "bacnet/basic/sys/debug.h"
#include "bacnet/basic/sys/keylist.h"
#include "bacnet/basic/ucix/ucix.h"
#include "bacnet/config.h"
#include "bacnet/basic/object/device.h"
#include "bacnet/event.h"
#include "bacnet/basic/tsm/tsm.h"
#include "bacnet/wp.h"
#include "bacnet/basic/object/nc.h"
#include "bacnet/datalink/datalink.h"

#define PRINTF debug_perror

#if defined(INTRINSIC_REPORTING)
static const char *sec = "bacnet_nc";
static const char *type = "nc";

struct object_data {
    uint8_t Priority[MAX_BACNET_EVENT_TRANSITION];
    uint8_t Ack_Required;
    BACNET_DESTINATION Recipient_List[NC_MAX_RECIPIENTS];
    const char *Object_Name;
    const char *Description;
};

struct object_data_t {
    uint8_t Priority[MAX_BACNET_EVENT_TRANSITION];
    uint8_t Ack_Required;
    BACNET_DESTINATION Recipient_List[NC_MAX_RECIPIENTS];
    const char *Object_Name;
    const char *Description;
};

/* Key List for storing the object data sorted by instance number  */
static OS_Keylist Object_List;
/* common object type */
static const BACNET_OBJECT_TYPE Object_Type = OBJECT_NOTIFICATION_CLASS;

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Notification_Properties_Required[] = { PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME, PROP_OBJECT_TYPE, PROP_NOTIFICATION_CLASS, PROP_PRIORITY,
    PROP_ACK_REQUIRED, PROP_RECIPIENT_LIST, -1 };

static const int Notification_Properties_Optional[] = { PROP_DESCRIPTION, -1 };

static const int Notification_Properties_Proprietary[] = { -1 };

void Notification_Class_Property_Lists(
    const int **pRequired, const int **pOptional, const int **pProprietary)
{
    if (pRequired)
        *pRequired = Notification_Properties_Required;
    if (pOptional)
        *pOptional = Notification_Properties_Optional;
    if (pProprietary)
        *pProprietary = Notification_Properties_Proprietary;
    return;
}

/**
 * @brief Determines if a given Notification Value instance is valid
 * @param  object_instance - object-instance number of the object
 * @return  true if the instance is valid, and false if not
 */
bool Notification_Class_Valid_Instance(uint32_t object_instance)
{
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        return true;
    }

    return false;
}

/**
 * @brief Determines the number of Notification Value objects
 * @return  Number of Notification Value objects
 */
unsigned Notification_Class_Count(void)
{
    return Keylist_Count(Object_List);
}

/**
 * @brief Determines the object instance-number for a given 0..N index
 * of Notification Value objects where N is Notification_Class_Count().
 * @param  index - 0..MAX_ANALOG_OUTPUTS value
 * @return  object instance-number for the given index
 */
uint32_t Notification_Class_Index_To_Instance(unsigned index)
{
    return Keylist_Key(Object_List, index);
}

/**
 * @brief For a given object instance-number, determines a 0..N index
 * of Notification Value objects where N is Notification_Class_Count().
 * @param  object_instance - object-instance number of the object
 * @return  index for the given instance-number, or MAX_ANALOG_OUTPUTS
 * if not valid.
 */
unsigned Notification_Class_Instance_To_Index(uint32_t object_instance)
{
    return Keylist_Index(Object_List, object_instance);
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
bool Notification_Class_Object_Name(
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
            snprintf(name_text, sizeof(name_text), "NC %u",
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
bool Notification_Class_Name_Set(uint32_t object_instance, char *new_name)
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
 * @brief For a given object instance-number, returns the description
 * @param  object_instance - object-instance number of the object
 * @return description text or NULL if not found
 */
char *Notification_Class_Description(uint32_t object_instance)
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
bool Notification_Class_Description_Set(uint32_t object_instance, char *new_name)
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
 * For a given object instance-number, returns the Priority
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - Priority Array
 *
 * @return true
 */
bool Notification_Class_Priority(uint32_t object_instance, uint8_t value[MAX_BACNET_EVENT_TRANSITION])
{
    struct object_data *pObject;
    uint8_t b = 0;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        for (b = 0; b < MAX_BACNET_EVENT_TRANSITION; b++) {
            value[b] = pObject->Priority[b];
        }
        return true;
    } else
        return false;
}

/**
 * For a given object instance-number, write the Priority
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - Priority array
 * @param  a - Priority array index
 *
 * @return true
 */
bool Notification_Class_Priority_Set(uint32_t object_instance, uint8_t value[MAX_BACNET_EVENT_TRANSITION], uint8_t a)
{
    struct object_data *pObject;
    uint8_t b = 0;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        if (a < MAX_BACNET_EVENT_TRANSITION)
            pObject->Priority[a] = value[a];
        else {
            for (b = 0; b < MAX_BACNET_EVENT_TRANSITION; b++) {
                pObject->Priority[b] = value[b];
            }
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
 * @return  Ack Required value
 */
uint8_t Notification_Class_Ack_Required(uint32_t object_instance)
{
    uint8_t value = 0;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = pObject->Ack_Required;
    }

    return value;
}

/**
 * For a given object instance-number, sets the Ack Required value
 *
 * @param object_instance - object-instance number of the object
 * @param value - Ack Required value
 *
 * @return true if the Ack Required value was set
 */
bool Notification_Class_Ack_Required_Set(uint32_t object_instance, uint8_t value)
{
    bool status = false;
    struct object_data *pObject;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        pObject->Ack_Required = value;
        status = true;
    }

    return status;
}

/**
 * For a given object instance-number, returns the Recipient from List
 *
 * @param  object_instance - object-instance number of the object
 * @param  b - Recipient index
 *
 * @return Recipient struct
 */
BACNET_DESTINATION * Notification_Class_Recipient_List(uint32_t object_instance, uint8_t b)
{
    struct object_data *pObject;
    BACNET_DESTINATION *value;
    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        value = &pObject->Recipient_List[b];
        return value;
    } else
        return NULL;
}

/**
 * For a given object instance-number, write the Recipient List with index
 *
 * @param  object_instance - object-instance number of the object
 * @param  value - Recipient struct
 * @param  b - list len
 *
 * @return true
 */
bool Notification_Class_Recipient_List_Set(uint32_t object_instance, BACNET_DESTINATION value[NC_MAX_RECIPIENTS])
{
    struct object_data *pObject;
    pObject = Keylist_Data(Object_List, object_instance);
    bool ret = false;
    int idx = 0;
    if (pObject) {
        /* Decoded all recipient list */
        /* copy elements from temporary object */
        for (idx = 0; idx < NC_MAX_RECIPIENTS; idx++) {
            BACNET_ADDRESS src = { 0 };
            unsigned max_apdu = 0;
            uint32_t DeviceID = 0;
            if ((value[idx].Recipient.RecipientType == RECIPIENT_TYPE_DEVICE) &&
            (value[idx].Recipient._.DeviceIdentifier > 0)) {
                pObject->Recipient_List[idx] = value[idx];
                DeviceID = pObject->Recipient_List[idx].Recipient._.DeviceIdentifier;
                address_bind_request(DeviceID, &max_apdu, &src);
                ret = true;
            } else if ((value[idx].Recipient.RecipientType == RECIPIENT_TYPE_ADDRESS) &&
            (value[idx].Recipient._.Address.net == 0) &&
            (value[idx].Recipient._.Address.mac_len > 0)) {
                pObject->Recipient_List[idx] = value[idx];
                /* copy Address */
                src = pObject->Recipient_List[idx].Recipient._.Address;
                address_bind_request(BACNET_MAX_INSTANCE, &max_apdu, &src);
                ret = true;
            } else if ((value[idx].Recipient.RecipientType == RECIPIENT_TYPE_ADDRESS) &&
            (value[idx].Recipient._.Address.net != 65535) &&
            (value[idx].Recipient._.Address.len > 0)) {
                pObject->Recipient_List[idx] = value[idx];
                /* copy Address */
                src = pObject->Recipient_List[idx].Recipient._.Address;
                address_bind_request(BACNET_MAX_INSTANCE, &max_apdu, &src);
                ret = true;
            } else if ((value[idx].Recipient.RecipientType == RECIPIENT_TYPE_ADDRESS) &&
            (value[idx].Recipient._.Address.net == 65535)) {
                pObject->Recipient_List[idx] = value[idx];
                ret = true;
            } else {
                if (pObject->Recipient_List[idx].Recipient.RecipientType == RECIPIENT_TYPE_DEVICE) {
                    /* remove Device_ID */
                    DeviceID = pObject->Recipient_List[idx].Recipient._.DeviceIdentifier;
                    address_remove_device(DeviceID);
                } else if (pObject->Recipient_List[idx].Recipient.RecipientType == RECIPIENT_TYPE_ADDRESS) {
                    /* remove Address if not broadcast */
                    if (pObject->Recipient_List[idx].Recipient._.Address.net != 65535) {
                        src = pObject->Recipient_List[idx].Recipient._.Address;
                        if (address_get_device_id(&src, &DeviceID))
                            address_remove_device(DeviceID);
                    }
                }
                pObject->Recipient_List[idx].Recipient.RecipientType = RECIPIENT_TYPE_NOTINITIALIZED;
                ret = true;
            }
        }
    }
    return ret;
}


int Notification_Class_Read_Property(BACNET_READ_PROPERTY_DATA *rpdata)
{
    BACNET_CHARACTER_STRING char_string;
    BACNET_OCTET_STRING octet_string;
    BACNET_BIT_STRING bit_string;
    uint8_t *apdu = NULL;
    uint8_t u8Val;
    uint8_t prio[MAX_BACNET_EVENT_TRANSITION];
    BACNET_DESTINATION *RecipientEntry = NULL;
    int idx;
    int apdu_len = 0; /* return value */

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }

    apdu = rpdata->application_data;

    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len = encode_application_object_id(
                &apdu[0], OBJECT_NOTIFICATION_CLASS, rpdata->object_instance);
            break;
        case PROP_OBJECT_NAME:
            Notification_Class_Object_Name(
                rpdata->object_instance, &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Notification_Class_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;
        case PROP_OBJECT_TYPE:
            apdu_len = encode_application_enumerated(
                &apdu[0], OBJECT_NOTIFICATION_CLASS);
            break;

        case PROP_NOTIFICATION_CLASS:
            apdu_len +=
                encode_application_unsigned(&apdu[0], rpdata->object_instance);
            break;

        case PROP_PRIORITY:
            if (rpdata->array_index == 0)
                apdu_len += encode_application_unsigned(&apdu[0], 3);
            else {
                if (rpdata->array_index == BACNET_ARRAY_ALL) {
                    if (Notification_Class_Priority(rpdata->object_instance, prio)) {
                        apdu_len += encode_application_unsigned(&apdu[apdu_len],
                            prio[TRANSITION_TO_OFFNORMAL]);
                        apdu_len += encode_application_unsigned(&apdu[apdu_len],
                            prio[TRANSITION_TO_FAULT]);
                        apdu_len += encode_application_unsigned(&apdu[apdu_len],
                            prio[TRANSITION_TO_NORMAL]);
                    }
                } else if (rpdata->array_index <= MAX_BACNET_EVENT_TRANSITION) {
                    if (Notification_Class_Priority(rpdata->object_instance, prio)) {
                        apdu_len += encode_application_unsigned(&apdu[apdu_len],
                            prio[rpdata->array_index - 1]);
                    }
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = -1;
                }
            }
            break;

        case PROP_ACK_REQUIRED:
            u8Val = Notification_Class_Ack_Required(rpdata->object_instance);

            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                (u8Val & TRANSITION_TO_OFFNORMAL_MASKED) ? true : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                (u8Val & TRANSITION_TO_FAULT_MASKED) ? true : false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                (u8Val & TRANSITION_TO_NORMAL_MASKED) ? true : false);
            /* encode bitstring */
            apdu_len +=
                encode_application_bitstring(&apdu[apdu_len], &bit_string);
            break;

        case PROP_RECIPIENT_LIST:
            /* encode all entry of Recipient_List */
            for (idx = 0; idx < NC_MAX_RECIPIENTS; idx++) {
                int i = 0;

                /* get pointer of current element for Recipient_List  - easier
                 * for use */
                RecipientEntry = Notification_Class_Recipient_List(rpdata->object_instance, idx);
                if (RecipientEntry->Recipient.RecipientType !=
                    RECIPIENT_TYPE_NOTINITIALIZED) {
                    /* Valid Days - BACnetDaysOfWeek - [bitstring] monday-sunday
                     */
                    u8Val = 0x01;
                    bitstring_init(&bit_string);

                    for (i = 0; i < MAX_BACNET_DAYS_OF_WEEK; i++) {
                        if (RecipientEntry->ValidDays & u8Val)
                            bitstring_set_bit(&bit_string, i, true);
                        else
                            bitstring_set_bit(&bit_string, i, false);
                        u8Val <<= 1; /* next day */
                    }
                    apdu_len += encode_application_bitstring(
                        &apdu[apdu_len], &bit_string);

                    /* From Time */
                    apdu_len += encode_application_time(
                        &apdu[apdu_len], &RecipientEntry->FromTime);

                    /* To Time */
                    apdu_len += encode_application_time(
                        &apdu[apdu_len], &RecipientEntry->ToTime);

                    /*
                       BACnetRecipient ::= CHOICE {
                       device [0] BACnetObjectIdentifier,
                       address [1] BACnetAddress
                       } */

                    /* CHOICE - device [0] BACnetObjectIdentifier */
                    if (RecipientEntry->Recipient.RecipientType ==
                        RECIPIENT_TYPE_DEVICE) {
                        apdu_len += encode_context_object_id(&apdu[apdu_len], 0,
                            OBJECT_DEVICE,
                            RecipientEntry->Recipient._.DeviceIdentifier);
                    }
                    /* CHOICE - address [1] BACnetAddress */
                    else if (RecipientEntry->Recipient.RecipientType ==
                        RECIPIENT_TYPE_ADDRESS) {
                        /* opening tag 1 */
                        apdu_len += encode_opening_tag(&apdu[apdu_len], 1);
                        /* network-number Unsigned16, */
                        apdu_len += encode_application_unsigned(&apdu[apdu_len],
                            RecipientEntry->Recipient._.Address.net);

                        /* mac-address OCTET STRING */
                        if (RecipientEntry->Recipient._.Address.len > 0) {
                            octetstring_init(&octet_string,
                                RecipientEntry->Recipient._.Address.adr,
                                RecipientEntry->Recipient._.Address.len);
                        } else if (RecipientEntry->Recipient._.Address.mac_len > 0) {
                            octetstring_init(&octet_string,
                                RecipientEntry->Recipient._.Address.mac,
                                RecipientEntry->Recipient._.Address.mac_len);
                        } else {
                            octetstring_init(&octet_string, 0, 0);
                        }
                        apdu_len += encode_application_octet_string(
                            &apdu[apdu_len], &octet_string);

                        /* closing tag 1 */
                        apdu_len += encode_closing_tag(&apdu[apdu_len], 1);

                    } else {
                        ;
                    } /* shouldn't happen */

                    /* Process Identifier - Unsigned32 */
                    apdu_len += encode_application_unsigned(
                        &apdu[apdu_len], RecipientEntry->ProcessIdentifier);

                    /* Issue Confirmed Notifications - boolean */
                    apdu_len += encode_application_boolean(
                        &apdu[apdu_len], RecipientEntry->ConfirmedNotify);

                    /* Transitions - BACnet Event Transition Bits [bitstring] */
                    u8Val = RecipientEntry->Transitions;

                    bitstring_init(&bit_string);
                    bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                        (u8Val & TRANSITION_TO_OFFNORMAL_MASKED) ? true
                                                                 : false);
                    bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                        (u8Val & TRANSITION_TO_FAULT_MASKED) ? true : false);
                    bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                        (u8Val & TRANSITION_TO_NORMAL_MASKED) ? true : false);

                    apdu_len += encode_application_bitstring(
                        &apdu[apdu_len], &bit_string);
                }
            }
            break;

        default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = -1;
            break;
    }

    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

bool Notification_Class_Write_Property(BACNET_WRITE_PROPERTY_DATA *wp_data)
{
    struct object_data TmpNotify = { 0 };
    BACNET_APPLICATION_DATA_VALUE value;
    uint8_t TmpPriority[MAX_BACNET_EVENT_TRANSITION]; /* BACnetARRAY[3] of
                                                         Unsigned */
    bool status = false;
    int iOffset;
    uint8_t idx;
    uint8_t u8Val;
    int len = 0;
    struct uci_context *ctxw = NULL;
    char *idx_c = NULL;
    int idx_c_len = 0;


    /* decode some of the request */
    len = bacapp_decode_application_data(
        wp_data->application_data, wp_data->application_data_len, &value);
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }
    if ((wp_data->object_property != PROP_PRIORITY) &&
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
        case PROP_PRIORITY:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_UNSIGNED_INT);
            if (status) {
                if (wp_data->array_index == 0) {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    status = false;
                } else if (wp_data->array_index == BACNET_ARRAY_ALL) {
                    iOffset = 0;
                    for (idx = 0; idx < MAX_BACNET_EVENT_TRANSITION; idx++) {
                        len = bacapp_decode_application_data(
                            &wp_data->application_data[iOffset],
                            wp_data->application_data_len, &value);
                        if ((len == 0) ||
                            (value.tag !=
                                BACNET_APPLICATION_TAG_UNSIGNED_INT)) {
                            /* Bad decode, wrong tag or following required
                             * parameter missing */
                            wp_data->error_class = ERROR_CLASS_PROPERTY;
                            wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                            status = false;
                            break;
                        }
                        if (value.type.Unsigned_Int > 255) {
                            wp_data->error_class = ERROR_CLASS_PROPERTY;
                            wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                            status = false;
                            break;
                        }
                        TmpPriority[idx] = (uint8_t)value.type.Unsigned_Int;
                        iOffset += len;
                    }
                    if (status == true) {
                        if (Notification_Class_Priority_Set(wp_data->object_instance, TmpPriority, MAX_BACNET_EVENT_TRANSITION)) {
                            ucix_add_option_int(ctxw, sec, idx_c, "prio_offnormal", TmpPriority[TRANSITION_TO_OFFNORMAL]);
                            ucix_add_option_int(ctxw, sec, idx_c, "prio_fault", TmpPriority[TRANSITION_TO_FAULT]);
                            ucix_add_option_int(ctxw, sec, idx_c, "prio_normal", TmpPriority[TRANSITION_TO_NORMAL]);
                            ucix_commit(ctxw,sec);
                        }
                    }
                } else if (wp_data->array_index <= 3) {
                    if (value.type.Unsigned_Int > 255) {
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                        status = false;
                    } else {
                        u8Val = wp_data->array_index - 1;
                        if (Notification_Class_Priority_Set(wp_data->object_instance, TmpPriority, u8Val)) {
                            switch ((enum BACnetEventTransitionBits) u8Val) {
                            case TRANSITION_TO_OFFNORMAL:
                                ucix_add_option_int(ctxw, sec, idx_c, "prio_offnormal", TmpPriority[TRANSITION_TO_OFFNORMAL]);
                                ucix_commit(ctxw,sec);
                                break;
                            case TRANSITION_TO_FAULT:
                                ucix_add_option_int(ctxw, sec, idx_c, "prio_fault", TmpPriority[TRANSITION_TO_FAULT]);
                                ucix_commit(ctxw,sec);
                                break;
                            case TRANSITION_TO_NORMAL:
                                ucix_add_option_int(ctxw, sec, idx_c, "prio_normal", TmpPriority[TRANSITION_TO_NORMAL]);
                                ucix_commit(ctxw,sec);
                                break;
                            default:
                                break;
                            }
                        }
                    }
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    status = false;
                }
            }
            break;

        case PROP_ACK_REQUIRED:
            status = write_property_type_valid(
                wp_data, &value, BACNET_APPLICATION_TAG_BIT_STRING);
            if (status) {
                if (value.type.Bit_String.bits_used == 3) {
                    if (Notification_Class_Ack_Required_Set(wp_data->object_instance,
                    value.type.Bit_String.value[0])) {
                        ucix_add_option_int(ctxw, sec, idx_c, "ack_required",
                        Notification_Class_Ack_Required(wp_data->object_instance));
                        ucix_commit(ctxw,sec);
                    }
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            }
            break;

        case PROP_RECIPIENT_LIST:

            idx = 0;
            iOffset = 0;
            /* decode all packed */
            while (iOffset < wp_data->application_data_len) {
                /* Decode Valid Days */
                len = bacapp_decode_application_data(
                    &wp_data->application_data[iOffset],
                    wp_data->application_data_len, &value);

                if ((len == 0) ||
                    (value.tag != BACNET_APPLICATION_TAG_BIT_STRING)) {
                    /* Bad decode, wrong tag or following required parameter
                     * missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }

                if (value.type.Bit_String.bits_used == MAX_BACNET_DAYS_OF_WEEK)
                    /* store value */
                    TmpNotify.Recipient_List[idx].ValidDays =
                        value.type.Bit_String.value[0];
                else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_OTHER;
                    return false;
                }

                iOffset += len;
                /* Decode From Time */
                len = bacapp_decode_application_data(
                    &wp_data->application_data[iOffset],
                    wp_data->application_data_len, &value);

                if ((len == 0) || (value.tag != BACNET_APPLICATION_TAG_TIME)) {
                    /* Bad decode, wrong tag or following required parameter
                     * missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }
                /* store value */
                TmpNotify.Recipient_List[idx].FromTime = value.type.Time;

                iOffset += len;
                /* Decode To Time */
                len = bacapp_decode_application_data(
                    &wp_data->application_data[iOffset],
                    wp_data->application_data_len, &value);

                if ((len == 0) || (value.tag != BACNET_APPLICATION_TAG_TIME)) {
                    /* Bad decode, wrong tag or following required parameter
                     * missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }
                /* store value */
                TmpNotify.Recipient_List[idx].ToTime = value.type.Time;

                iOffset += len;
                /* context tag [0] - Device */
                if (decode_is_context_tag(
                        &wp_data->application_data[iOffset], 0)) {
                    TmpNotify.Recipient_List[idx].Recipient.RecipientType =
                        RECIPIENT_TYPE_DEVICE;
                    /* Decode Network Number */
                    len = bacapp_decode_context_data(
                        &wp_data->application_data[iOffset],
                        wp_data->application_data_len, &value,
                        PROP_RECIPIENT_LIST);

                    if ((len == 0) ||
                        (value.tag != BACNET_APPLICATION_TAG_OBJECT_ID)) {
                        /* Bad decode, wrong tag or following required parameter
                         * missing */
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                        return false;
                    }
                    /* store value */
                    TmpNotify.Recipient_List[idx].Recipient._.DeviceIdentifier =
                        value.type.Object_Id.instance;

                    iOffset += len;
                }
                /* opening tag [1] - Recipient */
                else if (decode_is_opening_tag_number(
                             &wp_data->application_data[iOffset], 1)) {
                    iOffset++;
                    TmpNotify.Recipient_List[idx].Recipient.RecipientType =
                        RECIPIENT_TYPE_ADDRESS;
                    /* Decode Network Number */
                    len = bacapp_decode_application_data(
                        &wp_data->application_data[iOffset],
                        wp_data->application_data_len, &value);

                    if ((len == 0) ||
                        (value.tag != BACNET_APPLICATION_TAG_UNSIGNED_INT)) {
                        /* Bad decode, wrong tag or following required parameter
                         * missing */
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                        return false;
                    }
                    /* store value */
                    TmpNotify.Recipient_List[idx].Recipient._.Address.net =
                        value.type.Unsigned_Int;

                    iOffset += len;
                    /* Decode Address */
                    len = bacapp_decode_application_data(
                        &wp_data->application_data[iOffset],
                        wp_data->application_data_len, &value);

                    if ((len == 0) ||
                        (value.tag != BACNET_APPLICATION_TAG_OCTET_STRING)) {
                        /* Bad decode, wrong tag or following required parameter
                         * missing */
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                        return false;
                    }
                    /* store value */
                    if (TmpNotify.Recipient_List[idx].Recipient._.Address.net ==
                        0) {
                        memcpy(TmpNotify.Recipient_List[idx]
                                   .Recipient._.Address.mac,
                            value.type.Octet_String.value,
                            value.type.Octet_String.length);
                        TmpNotify.Recipient_List[idx]
                            .Recipient._.Address.mac_len =
                            value.type.Octet_String.length;
                    } else {
                        memcpy(TmpNotify.Recipient_List[idx]
                                   .Recipient._.Address.adr,
                            value.type.Octet_String.value,
                            value.type.Octet_String.length);
                        TmpNotify.Recipient_List[idx].Recipient._.Address.len =
                            value.type.Octet_String.length;
                    }

                    iOffset += len;
                    /* closing tag [1] - Recipient */
                    if (decode_is_closing_tag_number(
                            &wp_data->application_data[iOffset], 1))
                        iOffset++;
                    else {
                        /* Bad decode, wrong tag or following required parameter
                         * missing */
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                        return false;
                    }
                } else {
                    /* Bad decode, wrong tag or following required parameter
                     * missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }

                /* Process Identifier */
                len = bacapp_decode_application_data(
                    &wp_data->application_data[iOffset],
                    wp_data->application_data_len, &value);

                if ((len == 0) ||
                    (value.tag != BACNET_APPLICATION_TAG_UNSIGNED_INT)) {
                    /* Bad decode, wrong tag or following required parameter
                     * missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }
                /* store value */
                TmpNotify.Recipient_List[idx].ProcessIdentifier =
                    value.type.Unsigned_Int;

                iOffset += len;
                /* Issue Confirmed Notifications */
                len = bacapp_decode_application_data(
                    &wp_data->application_data[iOffset],
                    wp_data->application_data_len, &value);

                if ((len == 0) ||
                    (value.tag != BACNET_APPLICATION_TAG_BOOLEAN)) {
                    /* Bad decode, wrong tag or following required parameter
                     * missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }
                /* store value */
                TmpNotify.Recipient_List[idx].ConfirmedNotify =
                    value.type.Boolean;

                iOffset += len;
                /* Transitions */
                len = bacapp_decode_application_data(
                    &wp_data->application_data[iOffset],
                    wp_data->application_data_len, &value);

                if ((len == 0) ||
                    (value.tag != BACNET_APPLICATION_TAG_BIT_STRING)) {
                    /* Bad decode, wrong tag or following required parameter
                     * missing */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
                    return false;
                }

                if (value.type.Bit_String.bits_used ==
                    MAX_BACNET_EVENT_TRANSITION)
                    /* store value */
                    TmpNotify.Recipient_List[idx].Transitions =
                        value.type.Bit_String.value[0];
                else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_OTHER;
                    return false;
                }
                iOffset += len;

                /* Increasing element of list */
                if (++idx >= NC_MAX_RECIPIENTS) {
                    wp_data->error_class = ERROR_CLASS_RESOURCES;
                    wp_data->error_code = ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
                    return false;
                }
            }
            len = idx;
            char ucirecp[254][64];
            int ucirecp_n = 0;
            if (Notification_Class_Recipient_List_Set(wp_data->object_instance, TmpNotify.Recipient_List)) {
                for (idx = 0; idx < len; idx++) {
                    unsigned src_port,src_port1,src_port2;
                    if (TmpNotify.Recipient_List[idx].Recipient.RecipientType == RECIPIENT_TYPE_DEVICE) {
                        sprintf(ucirecp[ucirecp_n], "d,%i", TmpNotify.
                            Recipient_List[idx].Recipient._.DeviceIdentifier);
                        ucirecp_n++;
                    } else if ((TmpNotify.Recipient_List[idx].Recipient.RecipientType == RECIPIENT_TYPE_ADDRESS) &&
                    (TmpNotify.Recipient_List[idx].Recipient._.Address.net == 0)) {
                        src_port1 = TmpNotify.Recipient_List[idx].Recipient._.
                            Address.mac[4];
                        src_port2 = TmpNotify.Recipient_List[idx].Recipient._.
                            Address.mac[5];
                        src_port = ( src_port1 * 256 ) + src_port2;
                        sprintf(ucirecp[ucirecp_n], "n,%i,%i.%i.%i.%i:%i", TmpNotify.
                            Recipient_List[idx].Recipient._.Address.net,
                            TmpNotify.Recipient_List[idx].Recipient._.
                                Address.mac[0],
                            TmpNotify.Recipient_List[idx].Recipient._.
                                Address.mac[1],
                            TmpNotify.Recipient_List[idx].Recipient._.
                                Address.mac[2],
                            TmpNotify.Recipient_List[idx].Recipient._.
                                Address.mac[3],
                            src_port);
                        ucirecp_n++;
                    } else if ((TmpNotify.Recipient_List[idx].Recipient.RecipientType == RECIPIENT_TYPE_ADDRESS) &&
                    (TmpNotify.Recipient_List[idx].Recipient._.Address.net != 65535)) {
                        memcpy(TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr, value.type.Octet_String.value,
                            value.type.Octet_String.length);
                        TmpNotify.Recipient_List[idx].Recipient._.Address.len =
                            value.type.Octet_String.length;
                        src_port1 = TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[4];
                        src_port2 = TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[5];
                        src_port = ( src_port1 * 256 ) + src_port2;
                        sprintf(ucirecp[ucirecp_n], "n,%i,%i.%i.%i.%i:%i", TmpNotify.
                            Recipient_List[idx].Recipient._.Address.net,
                            TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[0],
                            TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[1],
                            TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[2],
                            TmpNotify.Recipient_List[idx].Recipient._.
                            Address.adr[3], src_port);
                        ucirecp_n++;
                    } else if ((TmpNotify.Recipient_List[idx].Recipient.RecipientType == RECIPIENT_TYPE_DEVICE) &&
                    (TmpNotify.Recipient_List[idx].Recipient._.Address.net == 65535)) {
                        sprintf(ucirecp[ucirecp_n], "n,%i", TmpNotify.
                            Recipient_List[idx].Recipient._.Address.net);
                        ucirecp_n++;
                    }
                }
            }
            if (ucirecp_n > 0) {
                ucix_set_list(ctxw, sec, idx_c, "recipient",
                ucirecp, ucirecp_n);
                ucix_commit(ctxw,sec);
            }
            status = true;
            break;

        case PROP_OBJECT_NAME:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Notification_Class_Name_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "name",
                        strndup(value.type.Character_String.value,value.type.Character_String.length));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        case PROP_DESCRIPTION:
            status = write_property_type_valid(wp_data, &value,
                BACNET_APPLICATION_TAG_CHARACTER_STRING);
            if (status) {
                if (Notification_Class_Description_Set(
                    wp_data->object_instance, value.type.Character_String.value)) {
                    ucix_add_option(ctxw, sec, idx_c, "description",
                        Notification_Class_Description(wp_data->object_instance));
                    ucix_commit(ctxw,sec);
                }
            }
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }

    return status;
}

/**
 * @brief Get the COV change flag status
 * @param object_instance - object-instance number of the object
 * @param PriorityArray - return PriorityArray
 * @return void
 */
void Notification_Class_Get_Priorities(
    uint32_t object_instance, uint32_t *pPriorityArray)
{
    struct object_data *pObject;
    int i;

    pObject = Keylist_Data(Object_List, object_instance);
    if (pObject) {
        for (i = 0; i < 3; i++)
            pPriorityArray[i] = pObject->Priority[i];
    } else {
        for (i = 0; i < 3; i++)
            pPriorityArray[i] = 255;
    }
}

static bool IsRecipientActive(
    BACNET_DESTINATION *pBacDest, uint8_t EventToState)
{
    BACNET_DATE_TIME DateTime;

    /* valid Transitions */
    switch (EventToState) {
        case EVENT_STATE_OFFNORMAL:
        case EVENT_STATE_HIGH_LIMIT:
        case EVENT_STATE_LOW_LIMIT:
            if (!(pBacDest->Transitions & TRANSITION_TO_OFFNORMAL_MASKED))
                return false;
            break;

        case EVENT_STATE_FAULT:
            if (!(pBacDest->Transitions & TRANSITION_TO_FAULT_MASKED))
                return false;
            break;

        case EVENT_STATE_NORMAL:
            if (!(pBacDest->Transitions & TRANSITION_TO_NORMAL_MASKED))
                return false;
            break;

        default:
            return false; /* shouldn't happen */
    }

    /* get actual date and time */
    Device_getCurrentDateTime(&DateTime);

    /* valid Days */
    if (!((0x01 << (DateTime.date.wday - 1)) & pBacDest->ValidDays))
        return false;

    /* valid FromTime */
    if (datetime_compare_time(&DateTime.time, &pBacDest->FromTime) < 0)
        return false;

    /* valid ToTime */
    if (datetime_compare_time(&pBacDest->ToTime, &DateTime.time) < 0)
        return false;

    return true;
}

void Notification_Class_common_reporting_function(
    BACNET_EVENT_NOTIFICATION_DATA *event_data)
{
    /* Fill the parameters common for all types of events. */

    struct object_data *pObject;
    BACNET_DESTINATION *pBacDest;
    uint8_t index;

    pObject = Keylist_Data(Object_List, event_data->notificationClass);
    if (!pObject)
        return;

    /* Initiating Device Identifier */
    event_data->initiatingObjectIdentifier.type = OBJECT_DEVICE;
    event_data->initiatingObjectIdentifier.instance =
        Device_Object_Instance_Number();

    /* Priority and AckRequired */
    switch (event_data->toState) {
        case EVENT_STATE_NORMAL:
            event_data->priority =
                pObject->Priority[TRANSITION_TO_NORMAL];
            event_data->ackRequired =
                (pObject->Ack_Required & TRANSITION_TO_NORMAL_MASKED)
                ? true
                : false;
            break;

        case EVENT_STATE_FAULT:
            event_data->priority = pObject->Priority[TRANSITION_TO_FAULT];
            event_data->ackRequired =
                (pObject->Ack_Required & TRANSITION_TO_FAULT_MASKED)
                ? true
                : false;
            break;

        case EVENT_STATE_OFFNORMAL:
        case EVENT_STATE_HIGH_LIMIT:
        case EVENT_STATE_LOW_LIMIT:
            event_data->priority =
                pObject->Priority[TRANSITION_TO_OFFNORMAL];
            event_data->ackRequired =
                (pObject->Ack_Required & TRANSITION_TO_OFFNORMAL_MASKED)
                ? true
                : false;
            break;

        default: /* shouldn't happen */
            break;
    }

    /* send notifications for active recipients */
    PRINTF("Notification Class[%u]: send notifications\n",
        event_data->notificationClass);
    /* pointer to first recipient */
    pBacDest = &pObject->Recipient_List[0];
    for (index = 0; index < NC_MAX_RECIPIENTS; index++, pBacDest++) {
        /* check if recipient is defined */
        if (pBacDest->Recipient.RecipientType == RECIPIENT_TYPE_NOTINITIALIZED)
            break; /* recipient doesn't defined - end of list */

        if (IsRecipientActive(pBacDest, event_data->toState)) {
            BACNET_ADDRESS dest;
            uint32_t device_id;
            unsigned max_apdu;

            /* Process Identifier */
            event_data->processIdentifier = pBacDest->ProcessIdentifier;

            /* send notification */
            if (pBacDest->Recipient.RecipientType == RECIPIENT_TYPE_DEVICE) {
                /* send notification to the specified device */
                device_id = pBacDest->Recipient._.DeviceIdentifier;
                PRINTF("Notification Class[%u]: send notification to %u\n",
                    event_data->notificationClass, (unsigned)device_id);
                if (pBacDest->ConfirmedNotify == true)
                    Send_CEvent_Notify(device_id, event_data);
                else if (address_get_by_device(device_id, &max_apdu, &dest))
                    Send_UEvent_Notify(
                        Handler_Transmit_Buffer, event_data, &dest);
            } else if (pBacDest->Recipient.RecipientType ==
                RECIPIENT_TYPE_ADDRESS) {
                PRINTF("Notification Class[%u]: send notification to ADDR\n",
                    event_data->notificationClass);
                /* send notification to the address indicated */
                if (pBacDest->ConfirmedNotify == true) {
                    if (address_get_device_id(&dest, &device_id))
                        Send_CEvent_Notify(device_id, event_data);
                } else {
                    dest = pBacDest->Recipient._.Address;
                    Send_UEvent_Notify(
                        Handler_Transmit_Buffer, event_data, &dest);
                }
            }
        }
    }
}

/* This function tries to find the addresses of the defined devices. */
/* It should be called periodically (example once per minute). */
void Notification_Class_find_recipient(void)
{
    struct object_data *pObject;
    BACNET_DESTINATION *pBacDest;
    BACNET_ADDRESS src = { 0 };
    unsigned max_apdu = 0;
    int notify_index;
    uint32_t DeviceID;
    uint8_t idx;

    for (notify_index = 0; notify_index < Keylist_Count(Object_List);
         notify_index++) {
        /* pointer to current notification */
        pObject = Keylist_Data_Index(Object_List, notify_index);

        /* pointer to first recipient */
        pBacDest = &pObject->Recipient_List[0];
        for (idx = 0; idx < NC_MAX_RECIPIENTS; idx++, pBacDest++) {
            if (pObject->Recipient_List[idx].Recipient.RecipientType ==
                RECIPIENT_TYPE_DEVICE) {
                /* Device ID */
                DeviceID = pObject->Recipient_List[idx]
                               .Recipient._.DeviceIdentifier;
                /* Send who_ is request only when address of device is unknown.
                 */
                if (DeviceID < BACNET_MAX_INSTANCE) {
                    /* note: BACNET_MAX_INSTANCE = wildcard, not valid */
                    if (!address_bind_request(DeviceID, &max_apdu, &src)) {
                        Send_WhoIs(DeviceID, DeviceID);
                    }
                }
            } else if (pObject->Recipient_List[idx]
                           .Recipient.RecipientType == RECIPIENT_TYPE_ADDRESS) {
            }
        }
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
	disable = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx,
	"disable", 0);
	if (strcmp(sec_idx,"default") == 0)
		return;
	if (disable)
		return;
    idx = atoi(sec_idx);
    struct object_data *pObject = NULL;
    int index = 0;
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

    pObject->Ack_Required = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "ack_required", ictx->Object.Ack_Required);
    pObject->Priority[TRANSITION_TO_OFFNORMAL] = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "prio_offnormal", ictx->Object.Priority[TRANSITION_TO_OFFNORMAL]);
    pObject->Priority[TRANSITION_TO_FAULT] = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "prio_fault", ictx->Object.Priority[TRANSITION_TO_FAULT]);
    pObject->Priority[TRANSITION_TO_NORMAL] = ucix_get_option_int(ictx->ctx, ictx->section, sec_idx, "prio_normal", ictx->Object.Priority[TRANSITION_TO_NORMAL]);

    char *ucirecp[254];
    BACNET_DESTINATION recplist[NC_MAX_RECIPIENTS];
    //BACNET_IP6_ADDRESS addr6;
    //BACNET_IP_ADDRESS addr;
    int ucirecp_n = 0;
    int ucirecp_i = 0;
    char *uci_ptr;
    char *uci_ptr_a;
    char *src_ip;
    const char *src_net;
    unsigned net;
    unsigned src_port;

    ucirecp_n = ucix_get_list(ucirecp, ictx->ctx, ictx->section, sec_idx,
        "recipient");

    for (ucirecp_i = 0; ucirecp_i < ucirecp_n; ucirecp_i++) {
        BACNET_ADDRESS src = { 0 };
        recplist[ucirecp_i].ValidDays = 127; //TODO uci bit string 1,1,1,1,1,1,1 Mo,Di,Mi,Do,Fr,Sa,So
        recplist[ucirecp_i].FromTime.hour = 0;
        recplist[ucirecp_i].FromTime.min = 0;
        recplist[ucirecp_i].FromTime.sec = 0;
        recplist[ucirecp_i].FromTime.hundredths = 0;
        recplist[ucirecp_i].ToTime.hour = 23;
        recplist[ucirecp_i].ToTime.min = 59;
        recplist[ucirecp_i].ToTime.sec = 59;
        recplist[ucirecp_i].ToTime.hundredths = 99;
        recplist[ucirecp_i].ConfirmedNotify = false;
        recplist[ucirecp_i].ProcessIdentifier = ucirecp_i;
        recplist[ucirecp_i].Transitions = 7; //bit string 1,1,1 To Alarm,To Fault,To Normal
        uci_ptr = strtok(ucirecp[ucirecp_i], ",");
	    if (strcmp(uci_ptr,"d") == 0) {
            uci_ptr = strtok(NULL, "\0");
            recplist[ucirecp_i].Recipient._.DeviceIdentifier = atoi(uci_ptr);
            recplist[ucirecp_i].Recipient.RecipientType =
                RECIPIENT_TYPE_DEVICE;
        } else if ((strcmp(uci_ptr,"n") == 0)) {
            uci_ptr = strtok(NULL, "\0");
            uci_ptr = strtok(uci_ptr, ",");
            src_net = uci_ptr;
            if (!src_net) {
                net = 0;
            } else {
                net = atoi(src_net);
            }
            if (net == 0) {
                uci_ptr = strtok(NULL, ":");
                if (uci_ptr) {
                    src_ip = uci_ptr;
                    uci_ptr = strtok(NULL, "\0");
                    src_port = atoi(uci_ptr);
                    src.mac[4] = ( src_port / 256 );
                    src.mac[5] = src_port - ( ( src_port / 256 ) * 256 );
                    //bip_get_addr_by_name(src_ip, &addr);
                    //addr.port = src_port;
                    uci_ptr_a = strtok(src_ip, ".");
                    src.mac[0] = atoi(uci_ptr_a);
                    uci_ptr_a = strtok(NULL, ".");
                    src.mac[1] = atoi(uci_ptr_a);
                    uci_ptr_a = strtok(NULL, ".");
                    src.mac[2] = atoi(uci_ptr_a);
                    uci_ptr_a = strtok(NULL, ".");
                    src.mac[3] = atoi(uci_ptr_a);
                    src.mac_len = 7;
                    src.net = net;
                    src.len = 0;
                    recplist[ucirecp_i].Recipient._.Address = src;
                    recplist[ucirecp_i].Recipient.RecipientType =
                        RECIPIENT_TYPE_ADDRESS;
                }
            } else if (net == 65535) {
                recplist[ucirecp_i].Recipient._.Address.net = net;
                recplist[ucirecp_i].Recipient.RecipientType =
                        RECIPIENT_TYPE_ADDRESS;
                recplist[ucirecp_i].Recipient._.Address.len = 0;
                recplist[ucirecp_i].Recipient._.Address.mac_len = 0;
            }
        } else {
            recplist[ucirecp_i].Recipient.RecipientType =
                RECIPIENT_TYPE_NOTINITIALIZED;
        }
    }
    for (ucirecp_i = 0; ucirecp_i < ucirecp_n; ucirecp_i++) {
        BACNET_ADDRESS src = { 0 };
        unsigned max_apdu = 0;
        int32_t DeviceID;

        pObject->Recipient_List[ucirecp_i] =
            recplist[ucirecp_i];

        if (pObject->Recipient_List[ucirecp_i].Recipient.
            RecipientType == RECIPIENT_TYPE_DEVICE) {
            /* copy Device_ID */
            DeviceID =
                pObject->Recipient_List[ucirecp_i].Recipient._.
                DeviceIdentifier;
            address_bind_request(DeviceID, &max_apdu, &src);

        } else if (pObject->Recipient_List[ucirecp_i].Recipient.
            RecipientType == RECIPIENT_TYPE_ADDRESS) {
            /* copy Address */
            src = pObject->Recipient_List[ucirecp_i].Recipient._.Address;
            address_bind_request(BACNET_MAX_INSTANCE, &max_apdu, &src);
        }
    }

    /* add to list */
    index = Keylist_Data_Add(Object_List, idx, pObject);
    if (index >= 0) {
        Device_Inc_Database_Revision();
    }
    return;
}

void Notification_Class_Init(void)
{
    Object_List = Keylist_Create();
    struct uci_context *ctx;
    ctx = ucix_init(sec);
    if (!ctx)
        fprintf(stderr, "Failed to load config file %s\n",sec);
    struct object_data_t tObject;
    const char *option = NULL;
    BACNET_CHARACTER_STRING option_str;

    option = ucix_get_option(ctx, sec, "default", "description");
    if (option)
        if (characterstring_init_ansi(&option_str, option))
            tObject.Description = strndup(option,option_str.length);
    if (!tObject.Description)
        tObject.Description = "Notification Class";
    tObject.Ack_Required = ucix_get_option_int(ctx, sec, "default", "ack_required", 0); // or 3?
    tObject.Priority[TRANSITION_TO_OFFNORMAL] = ucix_get_option_int(ctx, sec, "default", "prio_offnormal", 255);
    tObject.Priority[TRANSITION_TO_FAULT] = ucix_get_option_int(ctx, sec, "default", "prio_fault", 255);
    tObject.Priority[TRANSITION_TO_NORMAL] = ucix_get_option_int(ctx, sec, "default", "prio_normal", 255);
    struct itr_ctx itr_m;
	itr_m.section = sec;
	itr_m.ctx = ctx;
	itr_m.Object = tObject;
    ucix_for_each_section_type(ctx, sec, type,
        (void *)uci_list,&itr_m);
    ucix_cleanup(ctx);
}
#endif /* defined(INTRINSIC_REPORTING) */
