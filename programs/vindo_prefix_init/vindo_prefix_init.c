// Custom program that runs after a new prefix is created. It installs
// a huge pile of dlls and other stuff, so that everything works.
//
// This file is part of Vindo, and is therefore under the same license, which is the GPL.

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <wchar.h>

#include "wine/debug.h"
#include "wine/library.h"
#include "wine/unicode.h"

void run_installers();
void run_installer(LPCWSTR installer, LPCWSTR installers_dir);
void install_dll(LPCWSTR dll, LPCWSTR dll_dir);
const WCHAR *get_dll_dir();
const WCHAR *get_installers_dir();
const WCHAR *get_data_dir();

#define TRY(something) if (!(something)) fail()
void fail();

LPWSTR concat(LPCWSTR part1, LPCWSTR part2);

LPVOID alloc(SIZE_T size);
void dealloc(const LPVOID memory);

WINE_DEFAULT_DEBUG_CHANNEL(v_p_i);

int wmain() {
    static const WCHAR star_dot_dll[] = {'*','.','d','l','l',0};
    LPCWSTR dll_dir = get_dll_dir();
    LPWSTR search_pattern = concat(dll_dir, star_dot_dll);
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;

    if ((find_handle = FindFirstFileW(search_pattern, &find_data)) == INVALID_HANDLE_VALUE) fail();

    do {
        *strrchrW(find_data.cFileName, '.') = '\0';
        install_dll(find_data.cFileName, dll_dir);
    } while (FindNextFileW(find_handle, &find_data) != 0);
    
    if (GetLastError() != ERROR_NO_MORE_FILES) fail();

    dealloc(search_pattern);
    dealloc((LPVOID) dll_dir);

    run_installers();

    return 0;
}

void run_installers() {
    static const WCHAR star_dot_exe[] = {'*','.','e','x','e',0};
    LPCWSTR installers_dir = get_installers_dir();
    LPWSTR search_pattern = concat(installers_dir, star_dot_exe);
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;

    if ((find_handle = FindFirstFileW(search_pattern, &find_data)) == INVALID_HANDLE_VALUE) fail();

    do {
        run_installer(find_data.cFileName, installers_dir);
    } while (FindNextFileW(find_handle, &find_data) != 0);
    
    if (GetLastError() != ERROR_NO_MORE_FILES) fail();

    dealloc(search_pattern);
    dealloc((LPVOID) installers_dir);
}

void run_installer(LPCWSTR installer, LPCWSTR installers_dir) {
    WCHAR *installer_file = concat(installers_dir, installer);

    STARTUPINFOW si;
    PROCESS_INFORMATION pi;

    static WCHAR slash_q[] = {' ', '/','q',0};
    WCHAR *argv = concat(installer, slash_q);

    if (!CreateProcessW(installer_file, argv, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) fail();

    WaitForSingleObject(pi.hProcess, INFINITE);

    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);

    dealloc((LPVOID) installer_file);
    dealloc((LPVOID) argv);
}

void install_dll(LPCWSTR dll, LPCWSTR dll_dir) {
    static const WCHAR dot_dll[] = {'.','d','l','l',0};
    static const WCHAR system32[] = {'C',':','\\','w','i','n','d','o','w','s','\\','s','y','s','t','e','m','3','2','\\',0};

    LPCWSTR dll_file = concat(dll, dot_dll);

    LPCWSTR src_file = concat(dll_dir, dll_file);
    LPCWSTR dst_file = concat(system32, dll_file);

    // copy the file
    TRY(CopyFileW(src_file, dst_file, FALSE));

    // make dll override
    static const WCHAR dll_overrides[] = {'S','o','f','t','w','a','r','e','\\','W','i','n','e','\\','D','l','l','O','v','e','r','r','i','d','e','s',0};
    static const WCHAR native[] = {'n','a','t','i','v','e',0};
    HKEY overrides_key;

    TRY(!RegCreateKeyExW(HKEY_CURRENT_USER, dll_overrides, 0, NULL, 0, KEY_ALL_ACCESS, NULL, &overrides_key, NULL));
    TRY(!RegSetValueExW(overrides_key, dll, 0, REG_SZ, (BYTE *) native, sizeof(native)));
    TRY(!RegCloseKey(overrides_key));

    // dllregisterserver
    HMODULE module;
    HRESULT (WINAPI *DllRegisterServer)(void);
    HRESULT result;

    TRACE("loading dll %s\n", debugstr_w(dll));
    TRY(module = LoadLibraryExW(dst_file, 0, LOAD_WITH_ALTERED_SEARCH_PATH));
    if ((DllRegisterServer = (typeof(DllRegisterServer)) GetProcAddress(module, "DllRegisterServer"))) {
        result = DllRegisterServer();
        if (FAILED(result) && result != E_NOTIMPL) {
            ERR("in dll %s: %x\n", debugstr_w(dll), result);
        }
    }
    FreeLibrary(module);

    dealloc((LPVOID) dll_file);
    dealloc((LPVOID) dst_file);
    dealloc((LPVOID) src_file);
}


const WCHAR *get_dll_dir() {
    static const WCHAR slash_dlls[] = {'\\','d','l','l','s','\\',0};
    
    const WCHAR *data_dir = get_data_dir();
    WCHAR *retval = concat(data_dir, slash_dlls);
    dealloc((LPVOID) data_dir);
    return retval;
}

const WCHAR *get_installers_dir() {
    static const WCHAR slash_installers[] = {'\\','i','n','s','t','a','l','l','e','r','s','\\',0};

    const WCHAR *data_dir = get_data_dir();
    WCHAR *retval = concat(data_dir, slash_installers);
    dealloc((LPVOID) data_dir);
    return retval;
}

const WCHAR *get_data_dir() {
    const char *data_dir;

    if (wine_get_data_dir() != NULL)
        data_dir = wine_get_data_dir();
    else {
        if (getenv("WINE_DATA_DIR") != NULL)
            data_dir = getenv("WINE_DATA_DIR");
        else {
            FIXME("can't find data dir, exiting");
            ExitProcess(1);
        }
    }

    return wine_get_dos_file_name(data_dir);
}


void fail() {
    char *msg;

    FormatMessageA(
        FORMAT_MESSAGE_ALLOCATE_BUFFER | 
        FORMAT_MESSAGE_FROM_SYSTEM |
        FORMAT_MESSAGE_IGNORE_INSERTS,
        NULL,
        GetLastError(),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
        (char *) &msg,
        0, NULL );

    ERR("%s", msg);

    ExitProcess(1);
}

LPWSTR concat(LPCWSTR part1, LPCWSTR part2) {
    LPWSTR result = alloc((strlenW(part1) + strlenW(part2) + 1) * 2);
    // Multiply the length by two, because the characters are wide.
    // It took me a half an hour to figure that out. Lesson learned.

    strcpyW(result, part1);
    strcatW(result, part2);

    return result;
}

LPVOID alloc(SIZE_T size) {
    LPVOID memory;

    if (!(memory = HeapAlloc(GetProcessHeap(), 0, size))) {
        ERR("not enough memory");
        ExitProcess(1);
    }

    return memory;
}

void dealloc(const LPVOID memory) {
    HeapFree(GetProcessHeap(), 0, (LPVOID) memory);
}

// Copied from winemenubuilder. Don't know where else to put it.
static WCHAR* utf8_chars_to_wchars(LPCSTR string)
{
    WCHAR *ret;
    INT size = MultiByteToWideChar(CP_UTF8, 0, string, -1, NULL, 0);
    ret = HeapAlloc(GetProcessHeap(), 0, size * sizeof(WCHAR));
    if (ret)
        MultiByteToWideChar(CP_UTF8, 0, string, -1, ret, size);
    return ret;
}


