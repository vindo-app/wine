/*
 * Copyright 2015 Erich E. Hoover, Sebastian Lackner, and Michael Müller
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
#ifndef __WINE_THUNKS_H
#define __WINE_THUNKS_H

#define USER_FN(name)   name
#define KERNEL_FN(name) NTDLL_##name

#define THUNK_LIST \
    T(NtOpenFile, 6) \
    T(NtCreateFile, 11) \
    T(NtSetInformationFile, 5) \
    T(NtSetInformationThread, 4) \
    T(NtQueryAttributesFile, 2) \
    T(NtQueryFullAttributesFile, 2) \
    T(NtOpenProcess, 3) \
    T(NtOpenProcessToken, 3) \
    T(NtOpenProcessTokenEx, 4) \
    T(NtOpenThread, 4) \
    T(NtOpenThreadToken, 4) \
    T(NtOpenThreadTokenEx, 5) \
    T(NtOpenKey, 3) \
    T(NtCreateKey, 7) \
    T(NtMapViewOfSection, 10) \
    T(NtUnmapViewOfSection, 2) \
/* THUNK_LIST */

#ifndef THUNK_DEFINE

#if defined(__i386__)
# define T(name, param_count) extern typeof(name) USER_FN(name);
THUNK_LIST
# undef T
#endif

#if defined(__i386__) && defined(NTDLL_KERNELSPACE)
# define T(name, param_count) extern typeof(name) KERNEL_FN(name) DECLSPEC_HIDDEN;
  THUNK_LIST
# undef T
# define THUNK(name) KERNEL_FN(name)
#else
# define THUNK(name) USER_FN(name)
#endif

#define NtOpenFile THUNK(NtOpenFile)
#define NtCreateFile THUNK(NtCreateFile)
#define NtSetInformationFile THUNK(NtSetInformationFile)
#define NtSetInformationThread THUNK(NtSetInformationThread)
#define NtQueryAttributesFile THUNK(NtQueryAttributesFile)
#define NtQueryFullAttributesFile THUNK(NtQueryFullAttributesFile)
#define NtOpenProcess THUNK(NtOpenProcess)
#define NtOpenProcessToken THUNK(NtOpenProcessToken)
#define NtOpenProcessTokenEx THUNK(NtOpenProcessTokenEx)
#define NtOpenThread THUNK(NtOpenThread)
#define NtOpenThreadToken THUNK(NtOpenThreadToken)
#define NtOpenThreadTokenEx THUNK(NtOpenThreadTokenEx)
#define NtOpenKey THUNK(NtOpenKey)
#define NtCreateKey THUNK(NtCreateKey)
#define NtMapViewOfSection THUNK(NtMapViewOfSection)
#define NtUnmapViewOfSection THUNK(NtUnmapViewOfSection)

#endif /* THUNK_DEFINE */

#endif /* __WINE_THUNKS_H */
