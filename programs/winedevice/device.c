/*
 * Service process to load a kernel driver
 *
 * Copyright 2007 Alexandre Julliard
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

#include "config.h"
#include "wine/port.h"

#include <stdarg.h>

#include "ntstatus.h"
#define WIN32_NO_STATUS
#include "windef.h"
#include "winbase.h"
#include "winternl.h"
#include "winreg.h"
#include "winnls.h"
#include "winsvc.h"
#include "ddk/wdm.h"
#include "wine/unicode.h"
#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(winedevice);
WINE_DECLARE_DEBUG_CHANNEL(relay);

extern NTSTATUS CDECL wine_ntoskrnl_main_loop( HANDLE stop_event );

static WCHAR *driver_name;
static SERVICE_STATUS_HANDLE service_handle;
static HKEY driver_hkey;
static HANDLE stop_event;
static DRIVER_OBJECT driver_obj;
static DRIVER_EXTENSION driver_extension;

/* find the LDR_MODULE corresponding to the driver module */
static LDR_MODULE *find_ldr_module( HMODULE module )
{
    LIST_ENTRY *entry, *list = &NtCurrentTeb()->Peb->LdrData->InMemoryOrderModuleList;

    for (entry = list->Flink; entry != list; entry = entry->Flink)
    {
        LDR_MODULE *ldr = CONTAINING_RECORD(entry, LDR_MODULE, InMemoryOrderModuleList);
        if (ldr->BaseAddress == module) return ldr;
        if (ldr->BaseAddress > (void *)module) break;
    }
    return NULL;
}

/* change permissions of a specific memory range, save original permissions */
static void virtual_protect_save( void *addr, SIZE_T size, ULONG new_prot, ULONG *old_prot )
{
    SYSTEM_BASIC_INFORMATION info;
    UINT i = 0;

    NtQuerySystemInformation( SystemBasicInformation, &info, sizeof(info), NULL );
    while (size)
    {
        SIZE_T block_size = min( size, info.PageSize - ((UINT_PTR)addr & (info.PageSize - 1)) );
        VirtualProtect( addr, block_size, new_prot, &old_prot[i++] );
        addr  = (void *)((char *)addr + block_size);
        size -= block_size;
    }
}

/* restore permissions for a specific memory range */
static void virtual_protect_load( void *addr, SIZE_T size, ULONG *old_prot )
{
    SYSTEM_BASIC_INFORMATION info;
    DWORD dummy;
    UINT i = 0;

    NtQuerySystemInformation( SystemBasicInformation, &info, sizeof(info), NULL );
    while (size)
    {
        SIZE_T block_size = min( size, info.PageSize - ((UINT_PTR)addr & (info.PageSize - 1)) );
        VirtualProtect( addr, block_size, old_prot[i++], &dummy );
        addr  = (void *)((char *)addr + block_size);
        size -= block_size;
    }
}

/* load the driver module file */
static HMODULE load_driver_module( const WCHAR *name )
{
    IMAGE_NT_HEADERS *nt;
    const IMAGE_IMPORT_DESCRIPTOR *imports;
    SYSTEM_BASIC_INFORMATION info;
    int i;
    INT_PTR delta;
    ULONG size;
    HMODULE module = LoadLibraryW( name );

    if (!module) return NULL;
    nt = RtlImageNtHeader( module );

    if (!(delta = (char *)module - (char *)nt->OptionalHeader.ImageBase)) return module;

    /* the loader does not apply relocations to non page-aligned binaries or executables,
     * we have to do it ourselves */

    NtQuerySystemInformation( SystemBasicInformation, &info, sizeof(info), NULL );
    if (nt->OptionalHeader.SectionAlignment < info.PageSize ||
        !(nt->FileHeader.Characteristics & IMAGE_FILE_DLL))
    {
        DWORD old_prot[3];
        IMAGE_BASE_RELOCATION *rel, *end;

        if ((rel = RtlImageDirectoryEntryToData( module, TRUE, IMAGE_DIRECTORY_ENTRY_BASERELOC, &size )))
        {
            WINE_TRACE( "%s: relocating from %p to %p\n",
                        wine_dbgstr_w(name), (char *)module - delta, module );
            end = (IMAGE_BASE_RELOCATION *)((char *)rel + size);
            while (rel < end - 1 && rel->SizeOfBlock)
            {
                void *page = (char *)module + rel->VirtualAddress;
                /* LdrProcessRelocationBlock can access the memory range from page - (page + 0xfff + 8), so
                 * changing permissions of a single page is not sufficient. We assume here that the minimum
                 * page size is 0x1000, so we have to save/restore two or three pages, depending on the
                 * virtual address. */
                virtual_protect_save( page, 0xfff + 8, PAGE_EXECUTE_READWRITE, old_prot );
                rel = LdrProcessRelocationBlock( page, (rel->SizeOfBlock - sizeof(*rel)) / sizeof(USHORT),
                                                 (USHORT *)(rel + 1), delta );
                virtual_protect_load( page, 0xfff + 8, old_prot );
                if (!rel) goto error;
            }
            /* make sure we don't try again */
            size = FIELD_OFFSET( IMAGE_NT_HEADERS, OptionalHeader ) + nt->FileHeader.SizeOfOptionalHeader;
            VirtualProtect( nt, size, PAGE_READWRITE, &old_prot[0] );
            nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC].VirtualAddress = 0;
            VirtualProtect( nt, size, old_prot[0], &old_prot[0] );
        }
    }

    /* make sure imports are relocated too */

    if ((imports = RtlImageDirectoryEntryToData( module, TRUE, IMAGE_DIRECTORY_ENTRY_IMPORT, &size )))
    {
        for (i = 0; imports[i].Name && imports[i].FirstThunk; i++)
        {
            char *name = (char *)module + imports[i].Name;
            WCHAR buffer[32], *p = buffer;

            while (p < buffer + 32) if (!(*p++ = *name++)) break;
            if (p <= buffer + 32) FreeLibrary( load_driver_module( buffer ) );
        }
    }

    return module;

error:
    FreeLibrary( module );
    return NULL;
}

/* call the driver init entry point */
static NTSTATUS init_driver( HMODULE module, UNICODE_STRING *keyname )
{
    unsigned int i;
    NTSTATUS status;
    const IMAGE_NT_HEADERS *nt = RtlImageNtHeader( module );

    if (!nt->OptionalHeader.AddressOfEntryPoint) return STATUS_SUCCESS;

    driver_obj.Size            = sizeof(driver_obj);
    driver_obj.DriverSection   = find_ldr_module( module );
    driver_obj.DriverInit      = (PDRIVER_INITIALIZE)((char *)module + nt->OptionalHeader.AddressOfEntryPoint);
    driver_obj.DriverExtension = &driver_extension;

    driver_extension.DriverObject   = &driver_obj;
    driver_extension.ServiceKeyName = *keyname;

    if (WINE_TRACE_ON(relay))
        WINE_DPRINTF( "%04x:Call driver init %p (obj=%p,str=%s)\n", GetCurrentThreadId(),
                      driver_obj.DriverInit, &driver_obj, wine_dbgstr_w(keyname->Buffer) );

    status = driver_obj.DriverInit( &driver_obj, keyname );

    if (WINE_TRACE_ON(relay))
        WINE_DPRINTF( "%04x:Ret  driver init %p (obj=%p,str=%s) retval=%08x\n", GetCurrentThreadId(),
                      driver_obj.DriverInit, &driver_obj, wine_dbgstr_w(keyname->Buffer), status );

    WINE_TRACE( "init done for %s obj %p\n", wine_dbgstr_w(driver_name), &driver_obj );
    WINE_TRACE( "- DriverInit = %p\n", driver_obj.DriverInit );
    WINE_TRACE( "- DriverStartIo = %p\n", driver_obj.DriverStartIo );
    WINE_TRACE( "- DriverUnload = %p\n", driver_obj.DriverUnload );
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
        WINE_TRACE( "- MajorFunction[%d] = %p\n", i, driver_obj.MajorFunction[i] );

    return status;
}

/* call the driver unload function */
static void unload_driver( HMODULE module, DRIVER_OBJECT *driver_obj )
{
    if (driver_obj->DriverUnload)
    {
        if (WINE_TRACE_ON(relay))
            WINE_DPRINTF( "%04x:Call driver unload %p (obj=%p)\n", GetCurrentThreadId(),
                          driver_obj->DriverUnload, driver_obj );

        driver_obj->DriverUnload( driver_obj );

        if (WINE_TRACE_ON(relay))
            WINE_DPRINTF( "%04x:Ret  driver unload %p (obj=%p)\n", GetCurrentThreadId(),
                          driver_obj->DriverUnload, driver_obj );
    }
    FreeLibrary( module );
}

/* load the .sys module for a device driver */
static HMODULE load_driver(void)
{
    static const WCHAR driversW[] = {'\\','d','r','i','v','e','r','s','\\',0};
    static const WCHAR systemrootW[] = {'\\','S','y','s','t','e','m','R','o','o','t','\\',0};
    static const WCHAR postfixW[] = {'.','s','y','s',0};
    static const WCHAR ntprefixW[] = {'\\','?','?','\\',0};
    static const WCHAR ImagePathW[] = {'I','m','a','g','e','P','a','t','h',0};
    static const WCHAR servicesW[] = {'\\','R','e','g','i','s','t','r','y',
                                      '\\','M','a','c','h','i','n','e',
                                      '\\','S','y','s','t','e','m',
                                      '\\','C','u','r','r','e','n','t','C','o','n','t','r','o','l','S','e','t',
                                      '\\','S','e','r','v','i','c','e','s','\\',0};

    UNICODE_STRING keypath;
    HMODULE module;
    LPWSTR path = NULL, str;
    DWORD type, size;

    str = HeapAlloc( GetProcessHeap(), 0, sizeof(servicesW) + strlenW(driver_name)*sizeof(WCHAR) );
    lstrcpyW( str, servicesW );
    lstrcatW( str, driver_name );

    if (RegOpenKeyW( HKEY_LOCAL_MACHINE, str + 18 /* skip \registry\machine */, &driver_hkey ))
    {
        WINE_ERR( "cannot open key %s, err=%u\n", wine_dbgstr_w(str), GetLastError() );
        HeapFree( GetProcessHeap(), 0, str);
        return FALSE;
    }
    RtlInitUnicodeString( &keypath, str );

    /* read the executable path from memory */
    size = 0;
    if (!RegQueryValueExW( driver_hkey, ImagePathW, NULL, &type, NULL, &size ))
    {
        str = HeapAlloc( GetProcessHeap(), 0, size );
        if (!RegQueryValueExW( driver_hkey, ImagePathW, NULL, &type, (LPBYTE)str, &size ))
        {
            size = ExpandEnvironmentStringsW(str,NULL,0);
            path = HeapAlloc(GetProcessHeap(),0,size*sizeof(WCHAR));
            ExpandEnvironmentStringsW(str,path,size);
        }
        HeapFree( GetProcessHeap(), 0, str );
        if (!path) return FALSE;

        if (!strncmpiW( path, systemrootW, 12 ))
        {
            WCHAR buffer[MAX_PATH];

            GetWindowsDirectoryW(buffer, MAX_PATH);

            str = HeapAlloc(GetProcessHeap(), 0, (size -11 + strlenW(buffer))
                                                        * sizeof(WCHAR));
            lstrcpyW(str, buffer);
            lstrcatW(str, path + 11);
            HeapFree( GetProcessHeap(), 0, path );
            path = str;
        }
        else if (!strncmpW( path, ntprefixW, 4 ))
            str = path + 4;
        else
            str = path;
    }
    else
    {
        /* default is to use the driver name + ".sys" */
        WCHAR buffer[MAX_PATH];
        GetSystemDirectoryW(buffer, MAX_PATH);
        path = HeapAlloc(GetProcessHeap(),0,
          (strlenW(buffer) + strlenW(driversW) + strlenW(driver_name) + strlenW(postfixW) + 1)
          *sizeof(WCHAR));
        lstrcpyW(path, buffer);
        lstrcatW(path, driversW);
        lstrcatW(path, driver_name);
        lstrcatW(path, postfixW);
        str = path;
    }

    WINE_TRACE( "loading driver %s\n", wine_dbgstr_w(str) );

    module = load_driver_module( str );
    HeapFree( GetProcessHeap(), 0, path );
    if (!module) return NULL;

    init_driver( module, &keypath );
    return module;
}

static DWORD WINAPI service_handler( DWORD ctrl, DWORD event_type, LPVOID event_data, LPVOID context )
{
    SERVICE_STATUS status;

    status.dwServiceType             = SERVICE_WIN32;
    status.dwControlsAccepted        = SERVICE_ACCEPT_STOP;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = 0;

    switch(ctrl)
    {
    case SERVICE_CONTROL_STOP:
    case SERVICE_CONTROL_SHUTDOWN:
        WINE_TRACE( "shutting down %s\n", wine_dbgstr_w(driver_name) );
        status.dwCurrentState     = SERVICE_STOP_PENDING;
        status.dwControlsAccepted = 0;
        SetServiceStatus( service_handle, &status );
        SetEvent( stop_event );
        return NO_ERROR;
    default:
        WINE_FIXME( "got service ctrl %x for %s\n", ctrl, wine_dbgstr_w(driver_name) );
        status.dwCurrentState = SERVICE_RUNNING;
        SetServiceStatus( service_handle, &status );
        return NO_ERROR;
    }
}

static void WINAPI ServiceMain( DWORD argc, LPWSTR *argv )
{
    SERVICE_STATUS status;
    HMODULE driver_module;

    WINE_TRACE( "starting service %s\n", wine_dbgstr_w(driver_name) );

    stop_event = CreateEventW( NULL, TRUE, FALSE, NULL );

    service_handle = RegisterServiceCtrlHandlerExW( driver_name, service_handler, NULL );
    if (!service_handle)
        return;

    status.dwServiceType             = SERVICE_WIN32;
    status.dwCurrentState            = SERVICE_START_PENDING;
    status.dwControlsAccepted        = 0;
    status.dwWin32ExitCode           = 0;
    status.dwServiceSpecificExitCode = 0;
    status.dwCheckPoint              = 0;
    status.dwWaitHint                = 10000;
    SetServiceStatus( service_handle, &status );

    driver_module = load_driver();
    if (driver_module)
    {
        status.dwCurrentState     = SERVICE_RUNNING;
        status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
        SetServiceStatus( service_handle, &status );

        wine_ntoskrnl_main_loop( stop_event );
        unload_driver( driver_module, &driver_obj );
    }
    else WINE_ERR( "driver %s failed to load\n", wine_dbgstr_w(driver_name) );

    status.dwCurrentState     = SERVICE_STOPPED;
    status.dwControlsAccepted = 0;
    SetServiceStatus( service_handle, &status );
    WINE_TRACE( "service %s stopped\n", wine_dbgstr_w(driver_name) );
}

int wmain( int argc, WCHAR *argv[] )
{
    SERVICE_TABLE_ENTRYW service_table[2];

    if (!(driver_name = argv[1]))
    {
        WINE_ERR( "missing device name, winedevice isn't supposed to be run manually\n" );
        return 1;
    }

    service_table[0].lpServiceName = argv[1];
    service_table[0].lpServiceProc = ServiceMain;
    service_table[1].lpServiceName = NULL;
    service_table[1].lpServiceProc = NULL;

    StartServiceCtrlDispatcherW( service_table );
    return 0;
}
