/*
 * Copyright 2015 Aric Stewart
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301, USA
 */

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winioctl.h"
#include "ddk/wdm.h"
#include "hidusage.h"
#include "ddk/hidport.h"
#include "ddk/hidclass.h"
#include "ddk/hidpi.h"
#include "wine/list.h"
#include "parse.h"

#define DEFAULT_POLL_INTERVAL 200
#define MAX_POLL_INTERVAL_MSEC 10000

typedef struct _BASE_DEVICE_EXTENSTION {
    HID_DEVICE_EXTENSION deviceExtension;

    HID_COLLECTION_INFORMATION information;
    WINE_HIDP_PREPARSED_DATA *preparseData;

    ULONG poll_interval;
    WCHAR *device_name;
    WCHAR *link_name;
    HANDLE halt_event;
    HANDLE thread;

    LIST_ENTRY irp_queue;

    /* Minidriver Specific stuff will end up here */
} BASE_DEVICE_EXTENSION;

typedef struct _minidriver
{
    struct list entry;

    HID_MINIDRIVER_REGISTRATION minidriver;

    PDRIVER_UNLOAD DriverUnload;
} minidriver;

/* Internal device functions */
NTSTATUS HID_CreateDevice(DEVICE_OBJECT *native_device, HID_MINIDRIVER_REGISTRATION *driver, DEVICE_OBJECT **device) DECLSPEC_HIDDEN;
NTSTATUS HID_LinkDevice(DEVICE_OBJECT *device, LPCWSTR serial, LPCWSTR index) DECLSPEC_HIDDEN;
void HID_DeleteDevice(HID_MINIDRIVER_REGISTRATION *driver, DEVICE_OBJECT *device) DECLSPEC_HIDDEN;
