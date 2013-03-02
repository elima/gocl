/*
 * gocl-decls.h
 *
 * Gocl - GLib/GObject wrapper for OpenCL
 * Copyright (C) 2012-2013 Igalia S.L.
 *
 * Authors:
 *  Eduardo Lima Mitev <elima@igalia.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 3 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License at http://www.gnu.org/licenses/lgpl-3.0.txt
 * for more details.
 */

#ifndef __GOCL_DECLS_H__
#define __GOCL_DECLS_H__

#include <glib.h>
#include <CL/opencl.h>

G_BEGIN_DECLS

/**
 * GoclDeviceType:
 * @GOCL_DEVICE_TYPE_DEFAULT:     Default device
 * @GOCL_DEVICE_TYPE_CPU:         CPU device
 * @GOCL_DEVICE_TYPE_GPU:         GPU device
 * @GOCL_DEVICE_TYPE_ACCELERATOR: Accelerator device
 * @GOCL_DEVICE_TYPE_ALL:         Any device
 **/
typedef enum
{
  GOCL_DEVICE_TYPE_DEFAULT     = CL_DEVICE_TYPE_DEFAULT,
  GOCL_DEVICE_TYPE_CPU         = CL_DEVICE_TYPE_CPU,
  GOCL_DEVICE_TYPE_GPU         = CL_DEVICE_TYPE_GPU,
  GOCL_DEVICE_TYPE_ACCELERATOR = CL_DEVICE_TYPE_ACCELERATOR,
  GOCL_DEVICE_TYPE_ALL         = CL_DEVICE_TYPE_ALL
} GoclDeviceType;

typedef enum
{
  GOCL_BUFFER_FLAGS_READ_WRITE     = CL_MEM_READ_WRITE,
  GOCL_BUFFER_FLAGS_WRITE_ONLY     = CL_MEM_WRITE_ONLY,
  GOCL_BUFFER_FLAGS_USE_HOST_PTR   = CL_MEM_USE_HOST_PTR,
  GOCL_BUFFER_FLAGS_ALLOC_HOST_PTR = CL_MEM_ALLOC_HOST_PTR,
  GOCL_BUFFER_FLAGS_COPY_HOST_PTR  = CL_MEM_COPY_HOST_PTR
} GoclBufferFlags;

typedef enum
{
  GOCL_QUEUE_FLAGS_OUT_OF_ORDER = CL_QUEUE_OUT_OF_ORDER_EXEC_MODE_ENABLE,
  GOCL_QUEUE_FLAGS_PROFILING    = CL_QUEUE_PROFILING_ENABLE
} GoclQueueFlags;

G_END_DECLS

#endif /* __GOCL_DECLS_H__ */
