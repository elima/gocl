/*
 * gocl-device.c
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
 * SECTION:gocl-device
 * @short_description: Object that represents an OpenCL capable device
 * @stability: Unstable
 *
 * A #GoclDevice object is not normally created directly. Instead, it is
 * obtained from a #GoclContext by calling any of gocl_context_get_device_by_index(),
 * gocl_context_get_default_gpu_sync() or gocl_context_get_default_cpu_sync().
 *
 * To obtain the maximum work group size of a device,
 * gocl_device_get_max_work_group_size() is used. The number of compute units can be
 * retrieved with gocl_device_get_max_compute_units().
 *
 * To enqueue operations on this device, a #GoclQueue provides a default command queue
 * which is obtained by calling gocl_device_get_default_queue(). More device queues can
 * be created by passing this object as 'device' property in the #GoclQueue constructor.
 **/

/**
 * GoclDeviceClass:
 * @parent_class: The parent class
 *
 * The class for #GoclDevice objects.
 **/

#include <gio/gio.h>

#include "gocl-device.h"

#include "gocl-error-private.h"
#include "gocl-error.h"
#include "gocl-decls.h"
#include "gocl-context.h"

struct _GoclDevicePrivate
{
  GoclContext *context;
  cl_device_id device_id;

  gsize max_work_group_size;

  GoclQueue *queue;

  gchar *extensions;
};

/* properties */
enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_ID
};

static void           gocl_device_class_init            (GoclDeviceClass *class);
static void           gocl_device_init                  (GoclDevice *self);
static void           gocl_device_dispose               (GObject *obj);
static void           gocl_device_finalize              (GObject *obj);

static void           set_property                      (GObject      *obj,
                                                         guint         prop_id,
                                                         const GValue *value,
                                                         GParamSpec   *pspec);
static void           get_property                      (GObject    *obj,
                                                         guint       prop_id,
                                                         GValue     *value,
                                                         GParamSpec *pspec);

G_DEFINE_TYPE (GoclDevice, gocl_device, G_TYPE_OBJECT);

#define GOCL_DEVICE_GET_PRIVATE(obj)                \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),              \
                                GOCL_TYPE_DEVICE,   \
                                GoclDevicePrivate)) \

static void
gocl_device_class_init (GoclDeviceClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->dispose = gocl_device_dispose;
  obj_class->finalize = gocl_device_finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  g_object_class_install_property (obj_class, PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Device context",
                                                        "The context of this device",
                                                        GOCL_TYPE_CONTEXT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_ID,
                                   g_param_spec_pointer ("id",
                                                         "Device id",
                                                         "The id of this device",
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GoclDevicePrivate));
}

static void
gocl_device_init (GoclDevice *self)
{
  GoclDevicePrivate *priv;

  self->priv = priv = GOCL_DEVICE_GET_PRIVATE (self);

  priv->max_work_group_size = 0;
  priv->queue = NULL;

  priv->extensions = NULL;
}

static void
gocl_device_dispose (GObject *obj)
{
  GoclDevice *self = GOCL_DEVICE (obj);

  if (self->priv->context != NULL)
    {
      g_object_unref (self->priv->context);
      self->priv->context = NULL;
    }

  if (self->priv->queue != NULL)
    {
      g_object_unref (self->priv->queue);
      self->priv->queue = NULL;
    }

  G_OBJECT_CLASS (gocl_device_parent_class)->dispose (obj);
}

static void
gocl_device_finalize (GObject *obj)
{
  GoclDevice *self = GOCL_DEVICE (obj);

  g_free (self->priv->extensions);

  G_OBJECT_CLASS (gocl_device_parent_class)->finalize (obj);
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GoclDevice *self;

  self = GOCL_DEVICE (obj);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->priv->context = g_value_dup_object (value);
      break;

    case PROP_ID:
      self->priv->device_id = (cl_device_id) g_value_get_pointer (value);
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
  GoclDevice *self;

  self = GOCL_DEVICE (obj);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->priv->context);
      break;

    case PROP_ID:
      g_value_set_pointer (value, (gpointer) self->priv->device_id);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static gboolean
acquire_or_release_gl_objects (GoclDevice  *self,
                               gboolean     acquire,
                               GList       *object_list,
                               GList       *event_wait_list,
                               cl_event    *out_event)
{
  cl_int err_code;
  GoclQueue *queue;
  cl_command_queue _queue;

  cl_event *_event_wait_list = NULL;
  guint event_wait_list_len;

  cl_mem *_object_list;
  guint object_list_len;

  g_return_val_if_fail (GOCL_IS_DEVICE (self), FALSE);

  if (object_list == NULL)
    return TRUE;

  queue = gocl_device_get_default_queue (self);
  if (queue == NULL)
    return FALSE;

  _queue = gocl_queue_get_queue (queue);

  _event_wait_list = gocl_event_list_to_array (event_wait_list,
                                               &event_wait_list_len);
  _object_list = gocl_buffer_list_to_array (object_list,
                                            &object_list_len);

  if (acquire)
    err_code = clEnqueueAcquireGLObjects (_queue,
                                          object_list_len,
                                          _object_list,
                                          event_wait_list_len,
                                          _event_wait_list,
                                          out_event);
  else
    err_code = clEnqueueReleaseGLObjects (_queue,
                                          object_list_len,
                                          _object_list,
                                          event_wait_list_len,
                                          _event_wait_list,
                                          out_event);

  g_free (_event_wait_list);
  g_free (_object_list);

  if (gocl_error_check_opencl_internal (err_code))
    return FALSE;

  return TRUE;
}

/* public */

/**
 * gocl_device_get_id:
 * @self: The #GoclDevice
 *
 * Returns the internal #cl_device_id.
 *
 * Returns: (transfer none) (type guint64): The device id
 **/
cl_device_id
gocl_device_get_id (GoclDevice *self)
{
  g_return_val_if_fail (GOCL_IS_DEVICE (self), NULL);

  return self->priv->device_id;
}

/**
 * gocl_device_get_context:
 * @device: The #GoclDevice
 *
 * Obtains the #GoclContext the device belongs to.
 *
 * Returns: (transfer none): A #GoclContext. The returned object is owned by
 *   the device, do not free.
 **/
GoclContext *
gocl_device_get_context (GoclDevice *device)
{
  g_return_val_if_fail (GOCL_IS_DEVICE (device), NULL);

  return device->priv->context;
}

/**
 * gocl_device_get_max_work_group_size:
 * @self: The #GoclDevice
 *
 * Retrieves the maximum work group size for the device,
 * by querying the @CL_DEVICE_MAX_WORK_GROUP_SIZE info key through
 * clGetDeviceInfo().
 * Upon success a value greater than zero is returned, otherwise zero
 * is returned.
 *
 * Returns: The maximum size of the work group for this device.
 **/
gsize
gocl_device_get_max_work_group_size (GoclDevice *self)
{
  cl_int err_code;

  g_return_val_if_fail (GOCL_IS_DEVICE (self), 0);

  if (self->priv->max_work_group_size > 0)
    return self->priv->max_work_group_size;

  err_code = clGetDeviceInfo (self->priv->device_id,
                              CL_DEVICE_MAX_WORK_GROUP_SIZE,
                              sizeof (gsize),
                              &self->priv->max_work_group_size,
                              NULL);
  if (gocl_error_check_opencl_internal (err_code))
    return 0;

  return self->priv->max_work_group_size;
}

/**
 * gocl_device_get_default_queue:
 * @self: The #GoclDevice
 *
 * Returns a #GoclQueue command queue associated with this device, or %NULL upon
 * error.
 *
 * Returns: (transfer none): A #GoclQueue object, which is owned by the device
 *   and should not be freed.
 **/
GoclQueue *
gocl_device_get_default_queue (GoclDevice *self)
{
  g_return_val_if_fail (GOCL_IS_DEVICE (self), NULL);

  if (self->priv->queue == NULL)
    {
      GError **error;

      error = gocl_error_prepare ();
      self->priv->queue = g_initable_new (GOCL_TYPE_QUEUE,
                                          NULL,
                                          error,
                                          "device", self,
                                          NULL);
    }

  return self->priv->queue;
}

/**
 * gocl_device_has_extension:
 * @self: The #GoclDevice
 * @extension_name: The OpenCL extension name, as string
 *
 * Tells whether the device supports a given OpenCL extension, described by
 * @extension_name.
 *
 * Returns: %TRUE if the device supports the extension, %FALSE otherwise
 **/
gboolean
gocl_device_has_extension (GoclDevice *self, const gchar *extension_name)
{
  g_return_val_if_fail (GOCL_IS_DEVICE (self), FALSE);
  g_return_val_if_fail (extension_name != NULL, FALSE);

  if (self->priv->extensions == NULL)
    {
      cl_int err_code;
      gchar value[2049] = {0, };
      gsize value_size;
      GError *error = NULL;

      err_code = clGetDeviceInfo (self->priv->device_id,
                                  CL_DEVICE_EXTENSIONS,
                                  2048,
                                  value,
                                  &value_size);
      if (gocl_error_check_opencl (err_code, &error))
        {
          g_warning ("Error getting device info for CL_DEVICE_EXTENSIONS: %s",
                     error->message);
          g_error_free (error);
          return FALSE;
        }

      value[value_size] = '\0';
      self->priv->extensions = g_strdup (value);
    }

  return
    g_strstr_len (self->priv->extensions, -1, extension_name) != NULL;
}

/**
 * gocl_device_get_max_compute_units:
 * @self: The #GoclDevice
 *
 * Retrieves the number of compute units in an OpenCL device, by querying
 * CL_DEVICE_MAX_COMPUTE_UNITS in device info.
 *
 * Returns: The number of compute units in the device
 **/
guint
gocl_device_get_max_compute_units (GoclDevice *self)
{
  cl_int err_code;
  cl_uint max_compute_units;
  GError *error = NULL;

  g_return_val_if_fail (GOCL_IS_DEVICE (self), FALSE);

  err_code = clGetDeviceInfo (self->priv->device_id,
                              CL_DEVICE_MAX_COMPUTE_UNITS,
                              sizeof (guint),
                              &max_compute_units,
                              NULL);
  if (gocl_error_check_opencl (err_code, &error))
    {
      g_warning ("Error getting device info for CL_DEVICE_MAX_COMPUTE_UNITS: %s",
                 error->message);
      g_error_free (error);
      return 0;
    }

  return (guint) max_compute_units;
}

/**
 * gocl_device_acquire_gl_objects_sync:
 * @self: The #GoclDevice
 * @object_list: (element-type Gocl.Buffer) (allow-none): A #GList of
 * #GoclBuffer objects, or %NULL
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * objects to wait for, or %NULL
 *
 * Enqueues a request for acquiring the #GoclBuffer (or deriving) objects
 * contained in @object_list, which were created from OpenGL objects, blocking
 * the program execution until the operation finishes.
 *
 * This method works only if the <i>cl_khr_gl_sharing</i> OpenCL extension is
 * supported.
 *
 * Upon success, %TRUE is returned, otherwise %FALSE is returned.
 *
 * Returns: %TRUE on success, %FALSE on error
 **/
gboolean
gocl_device_acquire_gl_objects_sync (GoclDevice  *self,
                                     GList       *object_list,
                                     GList       *event_wait_list)
{
  cl_event event;

  if (! acquire_or_release_gl_objects (self,
                                       TRUE,
                                       object_list,
                                       event_wait_list,
                                       &event))
    {
      return FALSE;
    }

  clWaitForEvents (1, &event);
  clReleaseEvent (event);

  return TRUE;
}

/**
 * gocl_device_acquire_gl_objects:
 * @self: The #GoclDevice
 * @object_list: (element-type Gocl.Buffer) (allow-none): A #GList of
 * #GoclBuffer objects, or %NULL
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * objects to wait for, or %NULL
 *
 * Enqueues an asynchronous request for acquiring the #GoclBuffer (or deriving)
 * objects contained in @object_list, which were created from OpenGL objects.
 * For a blocking version of this method, see
 * gocl_device_acquire_gl_objects_sync().
 *
 * This method works only if the <i>cl_khr_gl_sharing</i> OpenCL extension is
 * supported.
 *
 * Returns: (transfer none): A #GoclEvent to get notified when the operation
 * finishes
 **/
GoclEvent *
gocl_device_acquire_gl_objects (GoclDevice  *self,
                                GList       *object_list,
                                GList       *event_wait_list)
{
  GoclEvent *_event = NULL;
  cl_event event;
  GoclQueue *queue;

  queue = gocl_device_get_default_queue (self);

  if (! acquire_or_release_gl_objects (self,
                                       TRUE,
                                       object_list,
                                       event_wait_list,
                                       &event))
    {
      GError *error;
      GoclEventResolverFunc resolver_func;

      error = gocl_error_get_last ();

      _event = g_object_new (GOCL_TYPE_EVENT,
                             "queue", queue,
                             NULL);
      resolver_func = gocl_event_steal_resolver_func (_event);
      resolver_func (_event, error);
      g_error_free (error);
    }
  else
    {
      _event = g_object_new (GOCL_TYPE_EVENT,
                             "queue", queue,
                             "event", event,
                             NULL);
      gocl_event_set_event_wait_list (_event, event_wait_list);
      gocl_event_steal_resolver_func (_event);
    }

  return _event;
}

/**
 * gocl_device_release_gl_objects_sync:
 * @self: The #GoclDevice
 * @object_list: (element-type Gocl.Buffer) (allow-none): A #GList of
 * #GoclBuffer objects, or %NULL
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * objects to wait for, or %NULL
 *
 * Enqueues a request for releasing the #GoclBuffer (or deriving) objects
 * contained in @object_list, which were previously acquired by a call to
 * gocl_device_acquire_gl_objects_sync().
 *
 * Upon success, %TRUE is returned, otherwise %FALSE is returned.
 *
 * Returns: %TRUE on success, %FALSE on error
 **/
gboolean
gocl_device_release_gl_objects_sync (GoclDevice  *self,
                                     GList       *object_list,
                                     GList       *event_wait_list)
{
  cl_event event;

  if (! acquire_or_release_gl_objects (self,
                                       FALSE,
                                       object_list,
                                       event_wait_list,
                                       &event))
    {
      return FALSE;
    }

  clWaitForEvents (1, &event);
  clReleaseEvent (event);

  return TRUE;
}

/**
 * gocl_device_release_gl_objects:
 * @self: The #GoclDevice
 * @object_list: (element-type Gocl.Buffer) (allow-none): A #GList of
 * #GoclBuffer objects, or %NULL
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * objects to wait for, or %NULL
 *
 * Enqueues an asynchronous request for releasing the #GoclBuffer (or deriving)
 * objects contained in @object_list, which were created from OpenGL objects.
 * For a blocking version of this method, see
 * gocl_device_release_gl_objects_sync().
 *
 * This method works only if the <i>cl_khr_gl_sharing</i> OpenCL extension is
 * supported.
 *
 * Returns: (transfer none): A #GoclEvent to get notified when the operation
 * finishes
 **/
GoclEvent *
gocl_device_release_gl_objects (GoclDevice  *self,
                                GList       *object_list,
                                GList       *event_wait_list)
{
  GoclEvent *_event = NULL;
  cl_event event;
  GoclQueue *queue;

  queue = gocl_device_get_default_queue (self);

  if (! acquire_or_release_gl_objects (self,
                                       FALSE,
                                       object_list,
                                       event_wait_list,
                                       &event))
    {
      GError *error;
      GoclEventResolverFunc resolver_func;

      error = gocl_error_get_last ();

      _event = g_object_new (GOCL_TYPE_EVENT,
                             "queue", queue,
                             NULL);
      resolver_func = gocl_event_steal_resolver_func (_event);
      resolver_func (_event, error);
      g_error_free (error);
    }
  else
    {
      _event = g_object_new (GOCL_TYPE_EVENT,
                             "queue", queue,
                             "event", event,
                             NULL);
      gocl_event_set_event_wait_list (_event, event_wait_list);
      gocl_event_steal_resolver_func (_event);
    }

  return _event;
}
