// Custom program that runs after a new prefix is created. It installs
// a huge pile of dlls and other stuff, so that everything works.
//
// This file is part of Vindo, and is therefore under the same license, which is the GPL.

#include <stdio.h>
#include <stdlib.h>

#include <windows.h>

#include "wine/debug.h"
#include "wine/library.h"
#include "wine/unicode.h"

const WCHAR *get_dll_dir();

WINE_DEFAULT_DEBUG_CHANNEL(v_p_i);

#define DLL_SRC_TEMPLATE "%s\\dlls\\%s.dll"
#define DLL_DST_TEMPLATE "C:\\windows\\system32\\%s.dll"

int wmain() {
    const WCHAR *dll_dir = get_dll_dir();


    MESSAGE("dll_dir: %s\n", wine_dbgstr_w(dll_dir));

    HeapFree(GetProcessHeap(), 0, dll_dir);
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

    return wine_get_dos_file_name(dll_dir);
}

