/*
 * gocl-kernel.c
 *
 * Gocl - GLib/GObject wrapper for OpenCL
 * Copyright (C) 2012 Igalia S.L.
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

#include <string.h>

#include "gocl-kernel.h"

#include "gocl-error.h"
#include "gocl-program.h"

struct _GoclKernelPrivate
{
  cl_kernel kernel;
  gchar *name;

  GoclProgram *program;
};

/* properties */
enum
{
  PROP_0,
  PROP_PROGRAM,
  PROP_NAME
};

static void           gocl_kernel_class_init            (GoclKernelClass *class);
static void           gocl_kernel_init                  (GoclKernel *self);
static void           gocl_kernel_finalize              (GObject *obj);

static void           set_property                       (GObject      *obj,
                                                          guint         prop_id,
                                                          const GValue *value,
                                                          GParamSpec   *pspec);
static void           get_property                       (GObject    *obj,
                                                          guint       prop_id,
                                                          GValue     *value,
                                                          GParamSpec *pspec);

G_DEFINE_TYPE (GoclKernel, gocl_kernel, G_TYPE_OBJECT);

#define GOCL_KERNEL_GET_PRIVATE(obj) \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                GOCL_TYPE_KERNEL, \
                                GoclKernelPrivate)) \

static void
gocl_kernel_class_init (GoclKernelClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->finalize = gocl_kernel_finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  g_object_class_install_property (obj_class, PROP_PROGRAM,
                                   g_param_spec_object ("program",
                                                        "Program",
                                                        "The program owning this kernel",
                                                        GOCL_TYPE_PROGRAM,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_NAME,
                                   g_param_spec_string ("name",
                                                        "Kernel name",
                                                        "The name of the kernel function",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GoclKernelPrivate));
}

static void
gocl_kernel_init (GoclKernel *self)
{
  GoclKernelPrivate *priv;

  self->priv = priv = GOCL_KERNEL_GET_PRIVATE (self);
}

static void
gocl_kernel_finalize (GObject *obj)
{
  GoclKernel *self = GOCL_KERNEL (obj);

  g_free (self->priv->name);

  g_object_unref (self->priv->program);

  clReleaseKernel (self->priv->kernel);

  G_OBJECT_CLASS (gocl_kernel_parent_class)->finalize (obj);
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GoclKernel *self;

  self = GOCL_KERNEL (obj);

  switch (prop_id)
    {
    case PROP_PROGRAM:
      self->priv->program = g_value_dup_object (value);
      break;

    case PROP_NAME:
      self->priv->name = g_value_dup_string (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
get_property (GObject    *obj,
              guint       prop_id,
              GValue     *value,
              GParamSpec *pspec)
{
  GoclKernel *self;

  self = GOCL_KERNEL (obj);

  switch (prop_id)
    {
    case PROP_PROGRAM:
      g_value_set_object (value, self->priv->program);
      break;

    case PROP_NAME:
      g_value_set_string (value, self->priv->name);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

/* public */

GoclKernel *
gocl_kernel_new (GObject      *program,
                 const gchar  *name,
                 GError      **error)
{
  GoclKernel *self;
  cl_int err_code;

  g_return_val_if_fail (GOCL_IS_PROGRAM (program), NULL);
  g_return_val_if_fail (name != NULL, NULL);

  self = g_object_new (GOCL_TYPE_KERNEL,
                       "program", GOCL_PROGRAM (program),
                       "name", name,
                       NULL);

  self->priv->kernel =
    clCreateKernel (gocl_program_get_program (GOCL_PROGRAM (program)),
                    name,
                    &err_code);
  if (gocl_error_check_opencl (err_code, error))
    return NULL;

  return self;
}

/**
 * gocl_kernel_get_kernel:
 *
 * Returns: (transfer none) (type guint64):
 **/
cl_kernel
gocl_kernel_get_kernel (GoclKernel *self)
{
  g_return_val_if_fail (GOCL_IS_KERNEL (self), NULL);

  return self->priv->kernel;
}

/**
 * gocl_kernel_set_argument_int32:
 * @buf: (array length=num_elements) (element-type guint32):
 *
 **/
gboolean
gocl_kernel_set_argument_int32 (GoclKernel  *self,
                                guint        index,
                                gsize        num_elements,
                                gint32      *buf,
                                GError     **error)
{
  cl_int err_code;

  g_return_val_if_fail (GOCL_IS_KERNEL (self), FALSE);

  err_code = clSetKernelArg (self->priv->kernel,
                             index,
                             sizeof (gint32) * num_elements,
                             buf);

  return ! gocl_error_check_opencl (err_code, error);
}

/**
 * gocl_kernel_set_argument_int32:
 * @buf: (array length=num_elements) (element-type guint32):
 *
 **/
gboolean
gocl_kernel_set_argument_buffer (GoclKernel  *self,
                                 guint        index,
                                 GoclBuffer  *buffer,
                                 GError     **error)
{
  cl_int err_code;
  cl_mem buf;

  g_return_val_if_fail (GOCL_IS_KERNEL (self), FALSE);

  buf = gocl_buffer_get_buffer (buffer);

  err_code = clSetKernelArg (self->priv->kernel,
                             index,
                             sizeof (cl_mem),
                             &buf);

  return ! gocl_error_check_opencl (err_code, error);
}

gboolean
gocl_kernel_run_in_device_sync (GoclKernel  *self,
                                GoclDevice  *device,
                                gsize        global_work_size,
                                gsize        local_work_size,
                                GError     **error)
{
  cl_int err_code;
  cl_event event;
  GoclQueue *queue;
  cl_command_queue _queue;

  g_return_val_if_fail (GOCL_IS_KERNEL (self), FALSE);
  g_return_val_if_fail (GOCL_IS_DEVICE (device), FALSE);

  queue = gocl_device_get_queue (device, error);
  if (queue == NULL)
    return FALSE;

  _queue = gocl_queue_get_queue (queue);

  err_code = clEnqueueNDRangeKernel (_queue,
                                     self->priv->kernel,
                                     1,
                                     NULL,
                                     &global_work_size,
                                     &local_work_size,
                                     0,
                                     NULL,
                                     &event);
  if (gocl_error_check_opencl (err_code, error))
    return FALSE;

  clWaitForEvents (1, &event);
  clReleaseEvent (event);

  return TRUE;
}
