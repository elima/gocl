/*
 * gocl-context.c
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

/**
 * SECTION:gocl-context
 * @short_description: Object that represents an OpenCL context
 * @stability: Unstable
 *
 * A #GoclContext enables access to OpenCL objects such as devices, command
 * queues, buffers, programs, kernels, etc. Obtaining a #GoclContext is always
 * the first step an OpenCL application performs.
 *
 * A #GoclContext can be created with gocl_context_new_sync(), providing the
 * type of device which is a value from #GoclDeviceType. For convenience, the
 * methods gocl_context_get_default_cpu_sync() and
 * gocl_context_get_default_gpu_sync() are provided to easily retrieve
 * pre-created CPU and GPU contexts, respectively.
 *
 * For GPU devices, it is possible to share object with existing OpenGL contexts
 * if the <i>cl_khr_gl_sharing</i> extension is supported. To enable this
 * support, gocl_context_gpu_new_sync() is used, passing the pointers to the
 * corresponding GL context and display.
 *
 * Once a context is successfully created, devices can be obtained by calling
 * gocl_context_get_device_by_index(), where index must be a value between 0 and
 * the maximum number of devices in the context, minus one. Number of devices can
 * be obtained with gocl_context_get_num_devices().
 *
 * Memory buffers can be created in context's memory to that kernels can access
 * and share them during execution. To create a buffer,
 * gocl_context_create_buffer() method is provided.
 **/

/**
 * GoclContextClass:
 * @parent_class: The parent class
 *
 * The class for #GoclContext objects.
 **/

#include <string.h>
#include <gio/gio.h>

#include "gocl-context.h"

#include "gocl-error-private.h"
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

  gpointer gl_context;
  gpointer gl_display;
};

static cl_platform_id gocl_platforms[MAX_PLATFORMS];
static cl_uint gocl_num_platforms = 0;

static GoclContext *gocl_context_default_gpu = NULL;
static GoclContext *gocl_context_default_cpu = NULL;

/* properties */
enum
{
  PROP_0,
  PROP_DEVICE_TYPE,
  PROP_GL_CONTEXT,
  PROP_GL_DISPLAY
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

#define GOCL_CONTEXT_GET_PRIVATE(obj)                   \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                GOCL_TYPE_CONTEXT,      \
                                GoclContextPrivate))    \

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

  g_object_class_install_property (obj_class, PROP_GL_CONTEXT,
                                   g_param_spec_pointer ("gl-context",
                                                         "GL context",
                                                         "The GL context for data sharing",
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (obj_class, PROP_GL_DISPLAY,
                                   g_param_spec_pointer ("gl-display",
                                                         "GL display",
                                                         "The GL display for data sharing",
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
  cl_context_properties props[7] = {0, };

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

  /* enable GL sharing, if a GL context and display are provided */
  if (self->priv->gl_context != NULL && self->priv->gl_display != NULL)
    {
      props[2] = CL_GL_CONTEXT_KHR;
      props[3] = (cl_context_properties) self->priv->gl_context;

      props[4] = CL_GLX_DISPLAY_KHR;
      props[5] = (cl_context_properties) self->priv->gl_display;
    }

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

  priv->context = NULL;
}

static void
gocl_context_finalize (GObject *obj)
{
  GoclContext *self = GOCL_CONTEXT (obj);

  if (self->priv->context != NULL)
    clReleaseContext (self->priv->context);

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

    case PROP_GL_CONTEXT:
      self->priv->gl_context = g_value_get_pointer (value);
      break;

    case PROP_GL_DISPLAY:
      self->priv->gl_display = g_value_get_pointer (value);
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

    case PROP_GL_CONTEXT:
      g_value_set_pointer (value, self->priv->gl_context);
      break;

    case PROP_GL_DISPLAY:
      g_value_set_pointer (value, self->priv->gl_display);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

/* public */

/**
 * gocl_context_new_sync:
 * @device_type: A value from #GoclDeviceType
 *
 * Attempts to create a #GoclContext of the type specified in @device_type.
 * Upon error, %NULL is returned. On success, a new #GoclContext object is
 * returned.
 *
 * Returns: (transfer full): A newly created #GoclContext
 **/
GoclContext *
gocl_context_new_sync (GoclDeviceType device_type)
{
  GError **error;

  error = gocl_error_prepare ();

  return g_initable_new (GOCL_TYPE_CONTEXT,
                         NULL,
                         error,
                         "device-type", device_type,
                         NULL);
}

/**
 * gocl_context_gpu_new_sync:
 * @gl_context: (allow-none): A GL context, or %NULL
 * @gl_display: (allow-none): A GL display, or %NULL
 *
 * Attemps to create a new GPU context. If @gl_context and @gl_display
 * are provided (not %NULL), then the context will be setup to share
 * objects with an existing OpenGL context. For this to work, the
 * <i>cl_khr_gl_sharing</i> extension should be supported by the OpenCL
 * GPU implementation.
 *
 * Returns: (transfer full): A #GoclContext object, or %NULL on error
 **/
GoclContext *
gocl_context_gpu_new_sync (gpointer gl_context, gpointer gl_display)
{
  GError **error;

  error = gocl_error_prepare ();

  return g_initable_new (GOCL_TYPE_CONTEXT,
                         NULL,
                         error,
                         "device-type", GOCL_DEVICE_TYPE_GPU,
                         "gl-context", gl_context,
                         "gl-display", gl_display,
                         NULL);
}

/**
 * gocl_context_get_default_gpu_sync:
 *
 * Returns platform's default GPU context. The first call to this method will
 * attempt to create a new #GoclContext using a device type of
 * %GOCL_DEVICE_TYPE_GPU. Upon success, the context is cached and subsequent
 * calls will return the same object, increasing its reference count.
 *
 * Returns: (transfer full): A #GoclContext object, or %NULL on error
 **/
GoclContext *
gocl_context_get_default_gpu_sync (void)
{
  if (gocl_context_default_gpu != NULL)
    g_object_ref (gocl_context_default_gpu);
  else
    gocl_context_default_gpu = gocl_context_new_sync (CL_DEVICE_TYPE_GPU);

  return gocl_context_default_gpu;
}

/**
 * gocl_context_get_default_cpu_sync:
 *
 * Returns platform's default CPU context. The first call to this method will
 * attempt to create a new #GoclContext using a device type of
 * %GOCL_DEVICE_TYPE_CPU. Upon success, the context is cached and subsequent calls
 * will return the same object, increasing its reference count.
 *
 * Returns: (transfer full): A #GoclContext object, or %NULL on error
 **/
GoclContext *
gocl_context_get_default_cpu_sync (void)
{
  if (gocl_context_default_cpu != NULL)
    g_object_ref (gocl_context_default_cpu);
  else
    gocl_context_default_cpu = gocl_context_new_sync (CL_DEVICE_TYPE_CPU);

  return gocl_context_default_cpu;
}

/**
 * gocl_context_get_context:
 * @self: The #GoclContext
 *
 * Retrieves the internal OpenCL #cl_context object. This is not normally
 * called by applications. It is rather a low-level, internal API.
 *
 * Returns: (transfer none) (type guint): The internal #cl_context object
 **/
cl_context
gocl_context_get_context (GoclContext *self)
{
  g_return_val_if_fail (GOCL_IS_CONTEXT (self), NULL);

  return self->priv->context;
}

/**
 * gocl_context_get_num_devices:
 * @self: The #GoclContext
 *
 * Obtains the number of devices in this context. Calls to
 * gocl_context_get_device_by_index() must provide a device index between
 * 0 and the number of devices returned by this method, minus one.
 *
 * Returns: The number of devices
 **/
guint
gocl_context_get_num_devices (GoclContext *self)
{
  g_return_val_if_fail (GOCL_IS_CONTEXT (self), 0);

  return (gint) self->priv->num_devices;
}

/**
 * gocl_context_get_device_by_index:
 * @self: The #GoclContext
 * @device_index: The index of the device to retrieve
 *
 * Retrieves the @device_index-th device from the list of context devices.
 * Use gocl_context_get_num_devices() to get the number of devices in the
 * context.
 *
 * Notice that method creates a new #GoclDevice instance every time is called.
 *
 * Returns: (transfer full): A newly created #GoclDevice
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
