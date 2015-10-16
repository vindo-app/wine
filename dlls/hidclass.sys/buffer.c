/*  Implementation of a ring buffer for reports
 *
 * Copyright 2015 CodeWeavers, Aric Stewart
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
#include <stdarg.h>
#define NONAMELESSUNION
#include "hid.h"

#include "wine/debug.h"

WINE_DEFAULT_DEBUG_CHANNEL(hid);

#define BASE_BUFFER_SIZE 32

struct ReportRingBuffer
{
    UINT start, end, size;

    int *pointers;
    UINT pointer_alloc;
    UINT buffer_size;

    CRITICAL_SECTION lock;

    BYTE *buffer;
};

struct ReportRingBuffer* RingBuffer_Create(UINT buffer_size)
{
    struct ReportRingBuffer *ring;
    TRACE("Create Ring Buffer with buffer size %i\n",buffer_size);
    ring = HeapAlloc(GetProcessHeap(), 0, sizeof(*ring));
    if (!ring)
        return NULL;
    ring->start = ring->end = 0;
    ring->size = BASE_BUFFER_SIZE;
    ring->buffer_size = buffer_size;
    ring->pointer_alloc = 2;
    ring->pointers = HeapAlloc(GetProcessHeap(), 0, sizeof(int) * ring->pointer_alloc);
    if (!ring->pointers)
    {
        HeapFree(GetProcessHeap(), 0, ring);
        return NULL;
    }
    memset(ring->pointers, 0xff, sizeof(int) * ring->pointer_alloc);
    ring->buffer = HeapAlloc(GetProcessHeap(), 0, buffer_size * ring->size);
    if (!ring->buffer)
    {
        HeapFree(GetProcessHeap(), 0, ring->pointers);
        HeapFree(GetProcessHeap(), 0, ring);
        return NULL;
    }
    InitializeCriticalSection(&ring->lock);
    ring->lock.DebugInfo->Spare[0] = (DWORD_PTR)(__FILE__ ": RingBuffer.lock");
    return ring;
}

void RingBuffer_Destroy(struct ReportRingBuffer *ring)
{
    HeapFree(GetProcessHeap(), 0, ring->buffer);
    HeapFree(GetProcessHeap(), 0, ring->pointers);
    ring->lock.DebugInfo->Spare[0] = 0;
    DeleteCriticalSection(&ring->lock);
    HeapFree(GetProcessHeap(), 0, ring);
}

UINT RingBuffer_GetBufferSize(struct ReportRingBuffer *ring)
{
    return ring->buffer_size;
}

void RingBuffer_Read(struct ReportRingBuffer *ring, UINT index, void *output, UINT *size)
{
    void *ret = NULL;

    EnterCriticalSection(&ring->lock);
    if (index >= ring->pointer_alloc || ring->pointers[index] == 0xffffffff)
    {
        LeaveCriticalSection(&ring->lock);
        *size = 0;
        return;
    }
    if (ring->pointers[index] == ring->end)
    {
        LeaveCriticalSection(&ring->lock);
        *size = 0;
    }
    else
    {
        ret = &ring->buffer[ring->pointers[index] * ring->buffer_size];
        memcpy(output, ret, ring->buffer_size);
        ring->pointers[index]++;
        if (ring->pointers[index] == ring->size)
            ring->pointers[index] = 0;
        LeaveCriticalSection(&ring->lock);
        *size = ring->buffer_size;
    }
}

UINT RingBuffer_AddPointer(struct ReportRingBuffer *ring)
{
    UINT idx;
    EnterCriticalSection(&ring->lock);
    for (idx = 0; idx < ring->pointer_alloc; idx++)
        if (ring->pointers[idx] == -1)
            break;
    if (idx >= ring->pointer_alloc)
    {
        int count = idx = ring->pointer_alloc;
        ring->pointer_alloc *= 2;
        ring->pointers = HeapReAlloc(GetProcessHeap(), 0, ring->pointers, sizeof(int) * ring->pointer_alloc);
        for( ;count < ring->pointer_alloc; count++)
            ring->pointers[count] = -1;
    }
    ring->pointers[idx] = ring->start;
    LeaveCriticalSection(&ring->lock);
    return idx;
}

void RingBuffer_RemovePointer(struct ReportRingBuffer *ring, UINT index)
{
    EnterCriticalSection(&ring->lock);
    if (index < ring->pointer_alloc)
        ring->pointers[index] = 0xffffffff;
    LeaveCriticalSection(&ring->lock);
}
