/*
 * NTDLL thunks
 *
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

#include "config.h"
#include "wine/port.h"

#define THUNK_DEFINE
#include "ntdll_misc.h"

#define _STR(name) #name
#define STR(name)  _STR(name)

/*
 * Several browsers (Chrome/Opera/Steam) use a "sandbox" mechanism that copies the first
 * 16 bytes of an ntdll function and replaces it with a jump to their own trampoline.
 * This thunking mechanism provides a wrapper compatible with such a technique, but that also
 * mimics the normal NT behavior to avoid upsetting any security software that checks for
 * applications tampering/hooking ntdll routines.
 */

#if defined(__i386__)
#define DEFINE_NTDLL_THUNK(name, param_count) \
__ASM_STDCALL_FUNC(USER_FN(name), param_count*4, \
    "movl $" __ASM_NAME(STR(KERNEL_FN(name))) ", %eax\n\t" \
    "movl $" __ASM_NAME("KiFastSystemCall") ", %edx\n\t" \
    "call *%edx\n\t" \
    "ret $(" #param_count "*4)\n\t" \
    "nop" \
)
#else
/* FIXME: no thunk support for this architecture yet */
#define DEFINE_NTDLL_THUNK(name, param_count)
#endif

#if defined(__i386__)
/* TODO: Expose this function, but we want to wait until we have done more testing. */
__ASM_STDCALL_FUNC(KiFastSystemCall, 0,
    "add $4, %esp\n\t"
    "jmp *%eax\n\t"
    "ret $0"
)
VOID KiFastSystemCall(VOID) DECLSPEC_HIDDEN;
#endif

#define T(name, param_count) DEFINE_NTDLL_THUNK(name, param_count)
THUNK_LIST
#undef T
