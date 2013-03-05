/*
 * gocl-kernel.c
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

#include <string.h>
#include <gio/gio.h>

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
static void           gocl_kernel_initable_iface_init   (GInitableIface *iface);
static gboolean       gocl_kernel_initable_init         (GInitable     *initable,
                                                         GCancellable  *cancellable,
                                                         GError       **error);
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

G_DEFINE_TYPE_WITH_CODE (GoclKernel, gocl_kernel, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                gocl_kernel_initable_iface_init));

#define GOCL_KERNEL_GET_PRIVATE(obj)                    \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                GOCL_TYPE_KERNEL,       \
                                GoclKernelPrivate))     \

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
gocl_kernel_initable_iface_init (GInitableIface *iface)
{
  iface->init = gocl_kernel_initable_init;
}

static gboolean
gocl_kernel_initable_init (GInitable     *initable,
                           GCancellable  *cancellable,
                           GError       **error)
{
  GoclKernel *self = GOCL_KERNEL (initable);
  cl_program program;
  cl_int err_code = 0;

  program = gocl_program_get_program (self->priv->program);

  self->priv->kernel = clCreateKernel (program, self->priv->name, &err_code);
  if (gocl_error_check_opencl (err_code, error))
    return FALSE;

  return TRUE;
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

/**
 * gocl_kernel_run_in_device_sync:
 * @event_wait_list: (element-type Gocl.Event) (allow-none):
 *
 **/
gboolean
gocl_kernel_run_in_device_sync (GoclKernel  *self,
                                GoclDevice  *device,
                                gsize        global_work_size,
                                gsize        local_work_size,
                                GList       *event_wait_list,
                                GError     **error)
{
  cl_int err_code;
  cl_event event;
  GoclQueue *queue;
  cl_command_queue _queue;
  cl_event *_event_wait_list = NULL;

  g_return_val_if_fail (GOCL_IS_KERNEL (self), FALSE);
  g_return_val_if_fail (GOCL_IS_DEVICE (device), FALSE);

  queue = gocl_device_get_default_queue (device, error);
  if (queue == NULL)
    return FALSE;

  _event_wait_list = gocl_event_list_to_array (event_wait_list, NULL);

  _queue = gocl_queue_get_queue (queue);

  err_code = clEnqueueNDRangeKernel (_queue,
                                     self->priv->kernel,
                                     1,
                                     NULL,
                                     &global_work_size,
                                     &local_work_size,
                                     g_list_length (event_wait_list),
                                     _event_wait_list,
                                     &event);

  g_free (_event_wait_list);

  if (gocl_error_check_opencl (err_code, error))
    return FALSE;

  clWaitForEvents (1, &event);
  clReleaseEvent (event);

  return TRUE;
}

/**
 * gocl_kernel_run_in_device:
 * @event_wait_list: (element-type Gocl.Event) (allow-none):
 *
 * Returns: (transfer none):
 **/
GoclEvent *
gocl_kernel_run_in_device (GoclKernel  *self,
                           GoclDevice  *device,
                           gsize        global_work_size,
                           gsize        local_work_size,
                           GList       *event_wait_list)
{
  GError *error = NULL;

  cl_int err_code;
  cl_event event;
  GoclQueue *queue;
  cl_command_queue _queue;

  GoclEvent *_event = NULL;
  GoclEventResolverFunc resolver_func;

  cl_event *_event_wait_list = NULL;

  g_return_val_if_fail (GOCL_IS_KERNEL (self), FALSE);
  g_return_val_if_fail (GOCL_IS_DEVICE (device), FALSE);

  queue = gocl_device_get_default_queue (device, &error);

  _event = g_object_new (GOCL_TYPE_EVENT,
                         "queue", queue,
                         NULL);
  resolver_func = gocl_event_steal_resolver_func (_event);

  if (queue == NULL)
    {
      resolver_func (_event, error);
      return _event;
    }

  _event_wait_list = gocl_event_list_to_array (event_wait_list, NULL);

  _queue = gocl_queue_get_queue (queue);

  err_code = clEnqueueNDRangeKernel (_queue,
                                     self->priv->kernel,
                                     1,
                                     NULL,
                                     &global_work_size,
                                     &local_work_size,
                                     g_list_length (event_wait_list),
                                     _event_wait_list,
                                     &event);

  g_free (_event_wait_list);

  if (gocl_error_check_opencl (err_code, &error))
    {
      resolver_func (_event, error);
      return _event;
    }

  gocl_event_set_event (_event, event);

  return _event;
}
