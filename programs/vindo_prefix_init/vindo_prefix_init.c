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

void install_dlls();
const WCHAR *get_dll_dir();
void print_error();

LPWSTR concat(LPCWSTR part1, LPCWSTR part2);

LPVOID alloc(SIZE_T size);
void dealloc(LPVOID memory);

WINE_DEFAULT_DEBUG_CHANNEL(v_p_i);

int wmain() {
    install_dlls();

    return 0;
}

void install_dlls() {
    static const WCHAR star_dot_dll[] = {'*','.','d','l','l',0};
    static const WCHAR system32[] = {'C',':','\\','w','i','n','d','o','w','s','\\','s','y','s','t','e','m','3','2','\\',0};

    static const WCHAR dll_overrides_path[] = {'S','o','f','t','w','a','r','e','\\','W','i','n','e','\\','D','l','l','O','v','e','r','r','i','d','e','s',0};

    LPCWSTR dll_dir = get_dll_dir();
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;

    // search for dlls in share/wine/dlls

    LPWSTR search_pattern = concat(dll_dir, star_dot_dll);

    if ((find_handle = FindFirstFileW(search_pattern, &find_data)) == INVALID_HANDLE_VALUE) {
        print_error();
        goto stop;
    }

    do {
        LPWSTR dll = find_data.cFileName;
        LPWSTR src = concat(dll_dir, dll);
        LPWSTR dest = concat(system32, dll);

        // actually copy the dll to c:\windows\system32

        if (!CopyFileW(src, dest, FALSE)) {
            print_error();
            ERR("src: %s", wine_dbgstr_w(src));
            goto stop;
        }
        
        // set up a dll override
        HKEY dll_overrides;

        if (!RegOpenKeyExW(HKEY_CURRENT_USER, dll_overrides_path, 0, KEY_SET_VALUE, &dll_overrides)) {
            print_error();
            goto stop;
        }

        if (!RegCloseKey(dll_overrides)) {
            print_error();
            goto stop;
        }

        dealloc(src);
        dealloc(dest);
    } while (FindNextFileW(find_handle, &find_data) != 0);

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        print_error();
        goto stop;
    }

    dealloc(search_pattern);
    FindClose(find_handle);

stop:
    dealloc((LPVOID) dll_dir);
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

void print_error() {
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

void dealloc(LPVOID memory) {
    HeapFree(GetProcessHeap(), 0, memory);
}
