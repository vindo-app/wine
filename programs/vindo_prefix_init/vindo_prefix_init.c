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

void install_dll(LPCWSTR dll, LPCWSTR dll_dir);
const WCHAR *get_dll_dir();

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

    return 0;
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

    TRY(module = LoadLibraryExW(dst_file, 0, LOAD_WITH_ALTERED_SEARCH_PATH));
    if (!(DllRegisterServer = (typeof(DllRegisterServer)) GetProcAddress(module, "DllRegisterServer"))) goto dont_register; // this is fine
    result = DllRegisterServer();
    if (FAILED(result)) fail();
    FreeLibrary(module);

dont_register:
    dealloc((LPVOID) dll_dir);
    dealloc((LPVOID) dll_file);
    dealloc((LPVOID) dst_file);
    dealloc((LPVOID) src_file);
}


const WCHAR *get_dll_dir() {
    const char *data_dir;
    char *dll_dir;

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

    dll_dir = alloc(strlen(data_dir) + strlen("/dlls") + 1);
    strcpy(dll_dir, data_dir);
    strcat(dll_dir, "/dlls/");

    const WCHAR *dos_dll_dir = wine_get_dos_file_name(dll_dir);
    HeapFree(GetProcessHeap(), 0, dll_dir);
    return dos_dll_dir;
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
