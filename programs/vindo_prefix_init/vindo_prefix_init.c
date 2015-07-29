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

const WCHAR *get_dll_dir();
void print_error();

WINE_DEFAULT_DEBUG_CHANNEL(v_p_i);


int wmain() {
    static const WCHAR dot_dll[] = {'.','d','l','l',0};
    static const WCHAR slash_star[] = {'/','*',0};
    static const WCHAR dest_template[] = {'C','\\','w','i','n','d','o','w','s','\\','s','y','s','t','e','m','3','2','\\','%','s',0};
    static const WCHAR src_template[] = {'%','s','\\','%','s',0};

    const WCHAR *dll_dir = get_dll_dir();
    WIN32_FIND_DATAW find_data;
    HANDLE find_handle;

    WCHAR search_pattern[MAX_PATH];
    strcpyW(search_pattern, dll_dir);
    strcatW(search_pattern, slash_star);
    
    if ((find_handle = FindFirstFileW(search_pattern, &find_data)) == INVALID_HANDLE_VALUE) {
        print_error();
        goto stop;
    }

    do {
        const WCHAR *dll = find_data.cFileName;
        if (strlenW(dll) > 4 && !strcmpW(dll + strlenW(dll) - 4, dot_dll)) { // extension of .dll
            WCHAR dest[MAX_PATH];
            WCHAR src[MAX_PATH];

            sprintfW(dest, dest_template, dll);
            sprintfW(src, src_template, dll_dir, dll);

            if (!CopyFileW(src, dest, FALSE)) {
                print_error();
                ERR("src: %s", wine_dbgstr_w(src));
                goto stop;
            }
        }
    } while (FindNextFileW(find_handle, &find_data) != 0);

    if (GetLastError() != ERROR_NO_MORE_FILES) {
        print_error();
        goto stop;
    }

    FindClose(find_handle);

stop:
    HeapFree(GetProcessHeap(), 0, (LPVOID) dll_dir);
    return 0;
}

const WCHAR *get_dll_dir() {
    const char *data_dir;
    char *dll_dir;

    if (wine_get_data_dir() != NULL)
        data_dir = wine_get_data_dir();
    else {
        FIXME("wine_get_data_dir returning NULL, using ~/test_data_dir\n");
        data_dir = "~/test_data_dir";
    }

    dll_dir = HeapAlloc(GetProcessHeap(), 0, strlen(data_dir) + strlen("/dlls") + 1);
    strcpy(dll_dir, data_dir);
    strcat(dll_dir, "/dlls");

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
