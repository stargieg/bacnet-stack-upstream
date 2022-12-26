/**************************************************************************
*
* Copyright (C) 2015 Nikola Jelic <nikola.jelic@euroicc.com>
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

#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <stdbool.h>
#include <stdint.h>
#include "bacnet/bacnet_stack_exports.h"
#include "bacnet/bacdef.h"
#include "bacnet/bacapp.h"
#include "bacnet/datetime.h"
#include "bacnet/bacerror.h"
#include "bacnet/wp.h"
#include "bacnet/rp.h"
#include "bacnet/bacdevobjpropref.h"
#include "bacnet/bactimevalue.h"

#ifndef BACNET_WEEKLY_SCHEDULE_SIZE
#define BACNET_WEEKLY_SCHEDULE_SIZE 8   /* maximum number of data points for each day */
#endif

#ifndef BACNET_SCHEDULE_OBJ_PROP_REF_SIZE
#define BACNET_SCHEDULE_OBJ_PROP_REF_SIZE 4     /* maximum number of obj prop references */
#endif


#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

    /*
     * Note:
     * This is a different struct from BACNET_DAILY_SCHEDULE used in prop value encoding!
     * The number of entries is different.
     */
    typedef struct bacnet_obj_daily_schedule {
        BACNET_TIME_VALUE Time_Values[BACNET_WEEKLY_SCHEDULE_SIZE];
        uint16_t TV_Count;      /* the number of time values actually used */
    } BACNET_OBJ_DAILY_SCHEDULE;

    BACNET_STACK_EXPORT
    void Schedule_Property_Lists(const int **pRequired,
        const int **pOptional,
        const int **pProprietary);

    BACNET_STACK_EXPORT
    bool Schedule_Valid_Instance(uint32_t object_instance);
    BACNET_STACK_EXPORT
    unsigned Schedule_Count(void);
    BACNET_STACK_EXPORT
    uint32_t Schedule_Index_To_Instance(unsigned index);
    BACNET_STACK_EXPORT
    unsigned Schedule_Instance_To_Index(uint32_t instance);
    BACNET_STACK_EXPORT
    void Schedule_Init(void);

    BACNET_STACK_EXPORT
    void Schedule_Out_Of_Service_Set(
        uint32_t object_instance,
        bool value);
    BACNET_STACK_EXPORT
    bool Schedule_Out_Of_Service(
        uint32_t object_instance);


    BACNET_STACK_EXPORT
    bool Schedule_Object_Name(uint32_t object_instance,
        BACNET_CHARACTER_STRING * object_name);

    BACNET_STACK_EXPORT
    int Schedule_Read_Property(BACNET_READ_PROPERTY_DATA * rpdata);
    BACNET_STACK_EXPORT
    bool Schedule_Write_Property(BACNET_WRITE_PROPERTY_DATA * wp_data);

    /* utility functions for calculating current Present Value
     * if Exception Schedule is to be added, these functions must take that into account */
    BACNET_STACK_EXPORT
    void schedule_timer(uint16_t uSeconds);
#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif
