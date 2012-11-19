/*
 * gocl-context.c
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
#include <gio/gio.h>

#include "gocl-context.h"

#include "gocl-error.h"
#include "gocl-decls.h"

#define MAX_PLATFORMS 8
#define MAX_DEVICES   8

#define DEFAULT_PLATFORM_INDEX 0

struct _GoclContextPrivate
{
  cl_platform_id platform_id;

  cl_context context;
  GoclDeviceType device_type;

  cl_device_id devices[MAX_DEVICES];
  cl_uint num_devices;

  cl_command_queue queues[MAX_DEVICES];
};

static cl_platform_id gocl_platforms[MAX_PLATFORMS];
static cl_uint gocl_num_platforms = 0;

static GoclContext *gocl_context_default_gpu = NULL;
static GoclContext *gocl_context_default_cpu = NULL;

/* properties */
enum
{
  PROP_0,
  PROP_DEVICE_TYPE
};

static void           gocl_context_class_init            (GoclContextClass *class);
static void           gocl_context_initable_iface_init   (GInitableIface *iface);
static gboolean       gocl_context_initable_init         (GInitable     *initable,
                                                          GCancellable  *cancellable,
                                                          GError       **error);
static void           gocl_context_init                  (GoclContext *self);
static void           gocl_context_finalize              (GObject *obj);

static void           set_property                       (GObject      *obj,
                                                          guint         prop_id,
                                                          const GValue *value,
                                                          GParamSpec   *pspec);
static void           get_property                       (GObject    *obj,
                                                          guint       prop_id,
                                                          GValue     *value,
                                                          GParamSpec *pspec);

G_DEFINE_TYPE_WITH_CODE (GoclContext, gocl_context, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                gocl_context_initable_iface_init));

#define GOCL_CONTEXT_GET_PRIVATE(obj)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                GOCL_TYPE_CONTEXT, \
                                GoclContextPrivate)) \

static void
gocl_context_class_init (GoclContextClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->finalize = gocl_context_finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  g_object_class_install_property (obj_class, PROP_DEVICE_TYPE,
                                   g_param_spec_uint ("device-type",
                                                      "Device type",
                                                      "The device type for this context",
                                                      GOCL_DEVICE_TYPE_DEFAULT,
                                                      GOCL_DEVICE_TYPE_ALL,
                                                      GOCL_DEVICE_TYPE_DEFAULT,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GoclContextPrivate));
}

static void
gocl_context_initable_iface_init (GInitableIface *iface)
{
  iface->init = gocl_context_initable_init;
}

static gboolean
gocl_context_initable_init (GInitable     *initable,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GoclContext *self = GOCL_CONTEXT (initable);
  cl_int err_code = 0;
  cl_context_properties props[3] = {0, };

  /* get platform ids */
  if (gocl_num_platforms == 0)
    {
      err_code = clGetPlatformIDs (MAX_PLATFORMS,
                                   gocl_platforms,
                                   &gocl_num_platforms);
      if (gocl_error_check_opencl (err_code, error))
        return FALSE;
    }

  /* @TODO: currently using platform 0 only */
  self->priv->platform_id = gocl_platforms[DEFAULT_PLATFORM_INDEX];

  /* get devices */
  err_code = clGetDeviceIDs (self->priv->platform_id,
                             self->priv->device_type,
                             MAX_DEVICES,
                             self->priv->devices,
                             &self->priv->num_devices);

  /* setup context properties */
  props[0] = CL_CONTEXT_PLATFORM;
  props[1] = (cl_context_properties) self->priv->platform_id;

  /* create the context */
  self->priv->context = clCreateContextFromType (props,
                                                 self->priv->device_type,
                                                 NULL,
                                                 NULL,
                                                 &err_code);
  if (gocl_error_check_opencl (err_code, error))
    return FALSE;

  return TRUE;
}

static void
gocl_context_init (GoclContext *self)
{
  GoclContextPrivate *priv;

  self->priv = priv = GOCL_CONTEXT_GET_PRIVATE (self);

  memset (priv->queues, 0, sizeof (cl_command_queue) * MAX_DEVICES);
}

static void
gocl_context_finalize (GObject *obj)
{
  GoclContext *self = GOCL_CONTEXT (obj);
  gint i;

  clReleaseContext (self->priv->context);

  for (i=0; i<self->priv->num_devices; i++)
    if (self->priv->queues[i] != NULL)
      clReleaseCommandQueue (self->priv->queues[i]);

  G_OBJECT_CLASS (gocl_context_parent_class)->finalize (obj);

  if (self == gocl_context_default_cpu)
    gocl_context_default_cpu = NULL;
  else if (self == gocl_context_default_gpu)
    gocl_context_default_cpu = NULL;
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GoclContext *self;

  self = GOCL_CONTEXT (obj);

  switch (prop_id)
    {
    case PROP_DEVICE_TYPE:
      self->priv->device_type = g_value_get_uint (value);
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
  GoclContext *self;

  self = GOCL_CONTEXT (obj);

  switch (prop_id)
    {
    case PROP_DEVICE_TYPE:
      g_value_set_uint (value, self->priv->device_type);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

/* public */

/**
 * gocl_context_new:
 *
 * Returns: (transfer full):
 **/
GoclContext *
gocl_context_new (GoclDeviceType device_type, GError **error)
{
  return g_initable_new (GOCL_TYPE_CONTEXT,
                         NULL,
                         error,
                         "device-type", device_type,
                         NULL);
}

/**
 * gocl_context_get_default_gpu:
 *
 * Returns: (transfer full):
 **/
GoclContext *
gocl_context_get_default_gpu (GError **error)
{
  if (gocl_context_default_gpu != NULL)
    g_object_ref (gocl_context_default_gpu);
  else
    gocl_context_default_gpu = gocl_context_new (CL_DEVICE_TYPE_GPU, error);

  return gocl_context_default_gpu;
}

/**
 * gocl_context_get_default_cpu:
 *
 * Returns: (transfer full):
 **/
GoclContext *
gocl_context_get_default_cpu (GError **error)
{
  if (gocl_context_default_cpu != NULL)
    g_object_ref (gocl_context_default_cpu);
  else
    gocl_context_default_cpu = gocl_context_new (CL_DEVICE_TYPE_CPU, error);

  return gocl_context_default_cpu;
}

/**
 * gocl_context_get_context:
 *
 * Returns: (transfer none) (type guint):
 **/
cl_context
gocl_context_get_context (GoclContext *self)
{
  g_return_val_if_fail (GOCL_IS_CONTEXT (self), NULL);

  return self->priv->context;
}

/**
 * gocl_context_count_devices:
 *
 **/
guint
gocl_context_get_num_devices (GoclContext *self)
{
  g_return_val_if_fail (GOCL_IS_CONTEXT (self), 0);

  return (gint) self->priv->num_devices;
}

/**
 * gocl_context_get_device_by_index:
 *
 * Returns: (transfer full):
 **/
GoclDevice *
gocl_context_get_device_by_index (GoclContext *self, guint device_index)
{
  GoclDevice *device;

  g_return_val_if_fail (GOCL_IS_CONTEXT (self), NULL);
  g_return_val_if_fail (device_index < self->priv->num_devices, NULL);

  device = g_object_new (GOCL_TYPE_DEVICE,
                         "context", self,
                         "id", self->priv->devices[device_index],
                         NULL);

  return device;
}

/**
 * gocl_context_create_buffer:
 *
 * Returns: (transfer full):
 **/
GoclBuffer *
gocl_context_create_buffer (GoclContext  *self,
                            guint         flags,
                            gsize         size,
                            gpointer      host_ptr,
                            GError      **error)
{
  GoclBuffer *buffer;

  g_return_val_if_fail (GOCL_IS_CONTEXT (self), NULL);

  buffer = g_initable_new (GOCL_TYPE_BUFFER,
                           NULL,
                           error,
                           "context", self,
                           "flags", flags,
                           "size", size,
                           NULL);

  return buffer;
}
