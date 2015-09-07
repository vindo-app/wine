/*
 * Copyright 2002-2005 Jason Edmeades
 * Copyright 2002-2005 Raphael Junqueira
 * Copyright 2005 Oliver Stieber
 * Copyright 2009-2011 Henri Verbeet for CodeWeavers
 * Copyright 2013 Stefan Dösinger for CodeWeavers
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
#include "wined3d_private.h"

WINE_DEFAULT_DEBUG_CHANNEL(d3d_surface);
WINE_DECLARE_DEBUG_CHANNEL(d3d_perf);

BOOL volume_prepare_system_memory(struct wined3d_volume *volume)
{
    if (volume->resource.heap_memory)
        return TRUE;

    if (!wined3d_resource_allocate_sysmem(&volume->resource))
    {
        ERR("Failed to allocate system memory.\n");
        return FALSE;
    }
    return TRUE;
}

/* This call just uploads data, the caller is responsible for binding the
 * correct texture. */
/* Context activation is done by the caller. */
void wined3d_volume_upload_data(struct wined3d_volume *volume, const struct wined3d_context *context,
        const struct wined3d_const_bo_address *data)
{
    const struct wined3d_gl_info *gl_info = context->gl_info;
    const struct wined3d_format *format = volume->resource.format;
    UINT width = volume->resource.width;
    UINT height = volume->resource.height;
    UINT depth = volume->resource.depth;
    const void *mem = data->addr;
    void *converted_mem = NULL;

    TRACE("volume %p, context %p, level %u, format %s (%#x).\n",
            volume, context, volume->texture_level, debug_d3dformat(format->id),
            format->id);

    if (format->convert)
    {
        UINT dst_row_pitch, dst_slice_pitch;
        UINT src_row_pitch, src_slice_pitch;

        if (data->buffer_object)
            ERR("Loading a converted volume from a PBO.\n");
        if (volume->container->resource.format_flags & WINED3DFMT_FLAG_BLOCKS)
            ERR("Converting a block-based format.\n");

        dst_row_pitch = width * format->conv_byte_count;
        dst_slice_pitch = dst_row_pitch * height;

        wined3d_resource_get_pitch(&volume->resource, &src_row_pitch, &src_slice_pitch);

        converted_mem = HeapAlloc(GetProcessHeap(), 0, dst_slice_pitch * depth);
        format->convert(data->addr, converted_mem, src_row_pitch, src_slice_pitch,
                dst_row_pitch, dst_slice_pitch, width, height, depth);
        mem = converted_mem;
    }

    if (data->buffer_object)
    {
        GL_EXTCALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, data->buffer_object));
        checkGLcall("glBindBuffer");
    }

    GL_EXTCALL(glTexSubImage3D(GL_TEXTURE_3D, volume->texture_level, 0, 0, 0,
            width, height, depth,
            format->glFormat, format->glType, mem));
    checkGLcall("glTexSubImage3D");

    if (data->buffer_object)
    {
        GL_EXTCALL(glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0));
        checkGLcall("glBindBuffer");
    }

    HeapFree(GetProcessHeap(), 0, converted_mem);
}

/* Context activation is done by the caller. */
static void wined3d_volume_download_data(struct wined3d_volume *volume,
        const struct wined3d_context *context, const struct wined3d_bo_address *data)
{
    const struct wined3d_gl_info *gl_info = context->gl_info;
    const struct wined3d_format *format = volume->resource.format;

    if (format->convert)
    {
        FIXME("Attempting to download a converted volume, format %s.\n",
                debug_d3dformat(format->id));
        return;
    }

    if (data->buffer_object)
    {
        GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, data->buffer_object));
        checkGLcall("glBindBuffer");
    }

    gl_info->gl_ops.gl.p_glGetTexImage(GL_TEXTURE_3D, volume->texture_level,
            format->glFormat, format->glType, data->addr);
    checkGLcall("glGetTexImage");

    if (data->buffer_object)
    {
        GL_EXTCALL(glBindBuffer(GL_PIXEL_PACK_BUFFER, 0));
        checkGLcall("glBindBuffer");
    }

}

static void wined3d_volume_evict_sysmem(struct wined3d_volume *volume)
{
    wined3d_resource_free_sysmem(&volume->resource);
    volume->resource.map_heap_memory = NULL;
    wined3d_resource_invalidate_location(&volume->resource, WINED3D_LOCATION_SYSMEM);
}

/* Context activation is done by the caller. */
static void wined3d_volume_srgb_transfer(struct wined3d_volume *volume,
        struct wined3d_context *context, BOOL dest_is_srgb)
{
    struct wined3d_bo_address data;
    /* Optimizations are possible, but the effort should be put into either
     * implementing EXT_SRGB_DECODE in the driver or finding out why we
     * picked the wrong copy for the original upload and fixing that.
     *
     * Also keep in mind that we want to avoid using resource.heap_memory
     * for DEFAULT pool surfaces. */

    WARN_(d3d_perf)("Performing slow rgb/srgb volume transfer.\n");
    data.buffer_object = 0;
    data.addr = HeapAlloc(GetProcessHeap(), 0, volume->resource.size);
    if (!data.addr)
        return;

    wined3d_texture_bind_and_dirtify(volume->container, context, !dest_is_srgb);
    wined3d_volume_download_data(volume, context, &data);
    wined3d_texture_bind_and_dirtify(volume->container, context, dest_is_srgb);
    wined3d_volume_upload_data(volume, context, wined3d_const_bo_address(&data));

    HeapFree(GetProcessHeap(), 0, data.addr);
}

static BOOL wined3d_volume_can_evict(const struct wined3d_volume *volume)
{
    if (volume->resource.pool != WINED3D_POOL_MANAGED)
        return FALSE;
    if (volume->download_count >= 10)
        return FALSE;
    if (volume->resource.format->convert)
        return FALSE;
    if (volume->flags & WINED3D_VFLAG_CLIENT_STORAGE)
        return FALSE;

    return TRUE;
}

/* Context activation is done by the caller. */
static void wined3d_volume_load_location(struct wined3d_resource *resource,
        struct wined3d_context *context, DWORD location)
{
    struct wined3d_volume *volume = volume_from_resource(resource);
    DWORD required_access = wined3d_resource_access_from_location(location);

    TRACE("Volume %p, loading %s, have %s.\n", volume, wined3d_debug_location(location),
        wined3d_debug_location(volume->resource.locations));

    if ((volume->resource.access_flags & required_access) != required_access)
    {
        ERR("Operation requires %#x access, but volume only has %#x.\n",
                required_access, volume->resource.access_flags);
        return;
    }

    switch (location)
    {
        case WINED3D_LOCATION_TEXTURE_RGB:
        case WINED3D_LOCATION_TEXTURE_SRGB:
            if ((location == WINED3D_LOCATION_TEXTURE_RGB
                    && !(volume->container->flags & WINED3D_TEXTURE_RGB_ALLOCATED))
                    || (location == WINED3D_LOCATION_TEXTURE_SRGB
                    && !(volume->container->flags & WINED3D_TEXTURE_SRGB_ALLOCATED)))
                ERR("Trying to load (s)RGB texture without prior allocation.\n");

            if (volume->resource.locations & WINED3D_LOCATION_DISCARDED)
            {
                TRACE("Volume previously discarded, nothing to do.\n");
                wined3d_resource_invalidate_location(&volume->resource, WINED3D_LOCATION_DISCARDED);
            }
            else if (volume->resource.locations & WINED3D_LOCATION_SYSMEM)
            {
                struct wined3d_const_bo_address data = {0, volume->resource.heap_memory};
                wined3d_texture_bind_and_dirtify(volume->container, context,
                        location == WINED3D_LOCATION_TEXTURE_SRGB);
                wined3d_volume_upload_data(volume, context, &data);
            }
            else if (volume->resource.locations & WINED3D_LOCATION_BUFFER)
            {
                struct wined3d_const_bo_address data = {volume->resource.buffer->name, NULL};
                wined3d_texture_bind_and_dirtify(volume->container, context,
                        location == WINED3D_LOCATION_TEXTURE_SRGB);
                wined3d_volume_upload_data(volume, context, &data);
            }
            else if (volume->resource.locations & WINED3D_LOCATION_TEXTURE_RGB)
            {
                wined3d_volume_srgb_transfer(volume, context, TRUE);
            }
            else if (volume->resource.locations & WINED3D_LOCATION_TEXTURE_SRGB)
            {
                wined3d_volume_srgb_transfer(volume, context, FALSE);
            }
            else
            {
                FIXME("Implement texture loading from %s.\n", wined3d_debug_location(volume->resource.locations));
                return;
            }
            wined3d_resource_validate_location(&volume->resource, location);

            if (wined3d_volume_can_evict(volume))
                wined3d_volume_evict_sysmem(volume);

            break;

        case WINED3D_LOCATION_SYSMEM:
            if (!volume->resource.heap_memory)
                ERR("Trying to load WINED3D_LOCATION_SYSMEM without setting it up first.\n");

            if (volume->resource.locations & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB))
            {
                struct wined3d_bo_address data = {0, volume->resource.heap_memory};

                if (volume->resource.locations & WINED3D_LOCATION_TEXTURE_RGB)
                    wined3d_texture_bind_and_dirtify(volume->container, context, FALSE);
                else
                    wined3d_texture_bind_and_dirtify(volume->container, context, TRUE);

                volume->download_count++;
                wined3d_volume_download_data(volume, context, &data);
            }
            else
            {
                FIXME("Implement WINED3D_LOCATION_SYSMEM loading from %s.\n",
                        wined3d_debug_location(volume->resource.locations));
                return;
            }
            wined3d_resource_validate_location(&volume->resource, WINED3D_LOCATION_SYSMEM);
            break;

        case WINED3D_LOCATION_BUFFER:
            if (!volume->resource.buffer)
                ERR("Trying to load WINED3D_LOCATION_BUFFER without setting it up first.\n");

            if (volume->resource.locations & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB))
            {
                struct wined3d_bo_address data = {volume->resource.buffer->name, NULL};

                if (volume->resource.locations & WINED3D_LOCATION_TEXTURE_RGB)
                    wined3d_texture_bind_and_dirtify(volume->container, context, FALSE);
                else
                    wined3d_texture_bind_and_dirtify(volume->container, context, TRUE);

                wined3d_volume_download_data(volume, context, &data);
            }
            else
            {
                FIXME("Implement WINED3D_LOCATION_BUFFER loading from %s.\n",
                        wined3d_debug_location(volume->resource.locations));
                return;
            }
            wined3d_resource_validate_location(&volume->resource, WINED3D_LOCATION_BUFFER);
            break;

        default:
            FIXME("Implement %s loading from %s.\n", wined3d_debug_location(location),
                    wined3d_debug_location(volume->resource.locations));
    }
}

/* Context activation is done by the caller. */
void wined3d_volume_load(struct wined3d_volume *volume, struct wined3d_context *context, BOOL srgb_mode)
{
    wined3d_texture_prepare_texture(volume->container, context, srgb_mode);
    wined3d_resource_load_location(&volume->resource, context,
            srgb_mode ? WINED3D_LOCATION_TEXTURE_SRGB : WINED3D_LOCATION_TEXTURE_RGB);
}

void wined3d_volume_destroy(struct wined3d_volume *volume)
{
    struct wined3d_device *device = volume->resource.device;
    TRACE("volume %p.\n", volume);

    resource_cleanup(&volume->resource);
    if (wined3d_settings.cs_multithreaded)
        device->cs->ops->finish(device->cs);
    volume->resource.parent_ops->wined3d_object_destroyed(volume->resource.parent);
    HeapFree(GetProcessHeap(), 0, volume);
}

static void volume_unload(struct wined3d_resource *resource)
{
    struct wined3d_volume *volume = volume_from_resource(resource);
    struct wined3d_device *device = volume->resource.device;
    struct wined3d_context *context;

    if (volume->resource.pool == WINED3D_POOL_DEFAULT)
        ERR("Unloading DEFAULT pool volume.\n");

    TRACE("texture %p.\n", resource);

    if (wined3d_resource_prepare_system_memory(&volume->resource))
    {
        context = context_acquire(device, NULL);
        wined3d_resource_load_location(&volume->resource, context, WINED3D_LOCATION_SYSMEM);
        context_release(context);
        wined3d_resource_invalidate_location(&volume->resource, ~WINED3D_LOCATION_SYSMEM);
    }
    else
    {
        ERR("Out of memory when unloading volume %p.\n", volume);
        wined3d_resource_validate_location(&volume->resource, WINED3D_LOCATION_DISCARDED);
        wined3d_resource_invalidate_location(&volume->resource, ~WINED3D_LOCATION_DISCARDED);
    }

    /* The texture name is managed by the container. */
    volume->flags &= ~WINED3D_VFLAG_CLIENT_STORAGE;

    resource_unload(resource);
}

ULONG CDECL wined3d_volume_incref(struct wined3d_volume *volume)
{
    TRACE("Forwarding to container %p.\n", volume->container);

    return wined3d_texture_incref(volume->container);
}

ULONG CDECL wined3d_volume_decref(struct wined3d_volume *volume)
{
    TRACE("Forwarding to container %p.\n", volume->container);

    return wined3d_texture_decref(volume->container);
}

void * CDECL wined3d_volume_get_parent(const struct wined3d_volume *volume)
{
    TRACE("volume %p.\n", volume);

    return volume->resource.parent;
}

void CDECL wined3d_volume_preload(struct wined3d_volume *volume)
{
    FIXME("volume %p stub!\n", volume);
}

struct wined3d_resource * CDECL wined3d_volume_get_resource(struct wined3d_volume *volume)
{
    TRACE("volume %p.\n", volume);

    return &volume->resource;
}

static BOOL wined3d_volume_check_box_dimensions(const struct wined3d_volume *volume,
        const struct wined3d_box *box)
{
    if (!box)
        return TRUE;

    if (box->left >= box->right)
        return FALSE;
    if (box->top >= box->bottom)
        return FALSE;
    if (box->front >= box->back)
        return FALSE;
    if (box->right > volume->resource.width)
        return FALSE;
    if (box->bottom > volume->resource.height)
        return FALSE;
    if (box->back > volume->resource.depth)
        return FALSE;

    return TRUE;
}

HRESULT CDECL wined3d_volume_map(struct wined3d_volume *volume,
        struct wined3d_map_desc *map_desc, const struct wined3d_box *box, DWORD flags)
{
    HRESULT hr;
    const struct wined3d_format *format = volume->resource.format;
    const unsigned int fmt_flags = volume->container->resource.format_flags;

    map_desc->data = NULL;
    if (!(volume->resource.access_flags & WINED3D_RESOURCE_ACCESS_CPU))
    {
        WARN("Volume %p is not CPU accessible.\n", volume);
        return WINED3DERR_INVALIDCALL;
    }
    if (!wined3d_volume_check_box_dimensions(volume, box))
    {
        WARN("Map box is invalid.\n");
        return WINED3DERR_INVALIDCALL;
    }
    if ((fmt_flags & WINED3DFMT_FLAG_BLOCKS) && !wined3d_resource_check_block_align(&volume->resource, box))
    {
        WARN("Map box is misaligned for %ux%u blocks.\n",
                format->block_width, format->block_height);
        return WINED3DERR_INVALIDCALL;
    }

    hr = wined3d_resource_map(&volume->resource, map_desc, box, flags);
    if (FAILED(hr))
        return hr;

    return hr;
}

struct wined3d_volume * CDECL wined3d_volume_from_resource(struct wined3d_resource *resource)
{
    return volume_from_resource(resource);
}

HRESULT CDECL wined3d_volume_unmap(struct wined3d_volume *volume)
{
    HRESULT hr;

    if (volume->resource.unmap_dirtify)
        wined3d_texture_set_dirty(volume->container);

    hr = wined3d_resource_unmap(&volume->resource);
    if (hr == WINEDDERR_NOTLOCKED)
        return WINED3DERR_INVALIDCALL;
    return hr;
}

static ULONG volume_resource_incref(struct wined3d_resource *resource)
{
    return wined3d_volume_incref(volume_from_resource(resource));
}

static ULONG volume_resource_decref(struct wined3d_resource *resource)
{
    return wined3d_volume_decref(volume_from_resource(resource));
}

static void wined3d_volume_location_invalidated(struct wined3d_resource *resource, DWORD location)
{
    struct wined3d_volume *volume = volume_from_resource(resource);

    if (location & (WINED3D_LOCATION_TEXTURE_RGB | WINED3D_LOCATION_TEXTURE_SRGB))
        wined3d_texture_set_dirty(volume->container);
}

static const struct wined3d_resource_ops volume_resource_ops =
{
    volume_resource_incref,
    volume_resource_decref,
    volume_unload,
    wined3d_volume_location_invalidated,
    wined3d_volume_load_location,
};

static HRESULT volume_init(struct wined3d_volume *volume, struct wined3d_texture *container,
        const struct wined3d_resource_desc *desc, UINT level)
{
    struct wined3d_device *device = container->resource.device;
    const struct wined3d_gl_info *gl_info = &device->adapter->gl_info;
    const struct wined3d_format *format = wined3d_get_format(gl_info, desc->format);
    HRESULT hr;
    UINT size;

    /* TODO: Write tests for other resources and move this check
     * to resource_init, if applicable. */
    if (desc->usage & WINED3DUSAGE_DYNAMIC
            && (desc->pool == WINED3D_POOL_MANAGED || desc->pool == WINED3D_POOL_SCRATCH))
    {
        WARN("Attempted to create a DYNAMIC texture in pool %s.\n", debug_d3dpool(desc->pool));
        return WINED3DERR_INVALIDCALL;
    }

    size = wined3d_format_calculate_size(format, device->surface_alignment, desc->width, desc->height, desc->depth);

    if (FAILED(hr = resource_init(&volume->resource, device, WINED3D_RTYPE_VOLUME, format,
            WINED3D_MULTISAMPLE_NONE, 0, desc->usage, desc->pool, desc->width, desc->height, desc->depth,
            size, NULL, &wined3d_null_parent_ops, &volume_resource_ops)))
    {
        WARN("Failed to initialize resource, returning %#x.\n", hr);
        return hr;
    }

    volume->texture_level = level;
    volume->resource.locations = WINED3D_LOCATION_DISCARDED;

    if (desc->pool == WINED3D_POOL_DEFAULT && desc->usage & WINED3DUSAGE_DYNAMIC
            && gl_info->supported[ARB_PIXEL_BUFFER_OBJECT]
            && !format->convert)
    {
        wined3d_resource_free_sysmem(&volume->resource);
        volume->resource.map_binding = WINED3D_LOCATION_BUFFER;
        volume->resource.map_heap_memory = NULL;
    }

    volume->container = container;

    return WINED3D_OK;
}

HRESULT wined3d_volume_create(struct wined3d_texture *container, const struct wined3d_resource_desc *desc,
        unsigned int level, struct wined3d_volume **volume)
{
    struct wined3d_device_parent *device_parent = container->resource.device->device_parent;
    const struct wined3d_parent_ops *parent_ops;
    struct wined3d_volume *object;
    void *parent;
    HRESULT hr;

    TRACE("container %p, width %u, height %u, depth %u, level %u, format %s, "
            "usage %#x, pool %s, volume %p.\n",
            container, desc->width, desc->height, desc->depth, level, debug_d3dformat(desc->format),
            desc->usage, debug_d3dpool(desc->pool), volume);

    if (!(object = HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(*object))))
        return E_OUTOFMEMORY;

    if (FAILED(hr = volume_init(object, container, desc, level)))
    {
        WARN("Failed to initialize volume, returning %#x.\n", hr);
        HeapFree(GetProcessHeap(), 0, object);
        return hr;
    }

    if (FAILED(hr = device_parent->ops->volume_created(device_parent,
            wined3d_texture_get_parent(container), object, &parent, &parent_ops)))
    {
        WARN("Failed to create volume parent, hr %#x.\n", hr);
        wined3d_volume_destroy(object);
        return hr;
    }

    TRACE("Created volume %p, parent %p, parent_ops %p.\n", object, parent, parent_ops);

    object->resource.parent = parent;
    object->resource.parent_ops = parent_ops;
    *volume = object;

    return WINED3D_OK;
}
