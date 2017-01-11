/*
 * gocl-buffer.c
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
 * SECTION:gocl-buffer
 * @short_description: Object that represents a block of memory in an OpenCL
 * context
 * @stability: Unstable
 *
 * A #GoclBuffer represents a buffer object in an OpenCL context. These objects
 * are directly accessible from OpenCL programs.
 *
 * Buffers are created from a #GoclContext, by calling
 * gocl_buffer_new() method. It is possible to initialize the
 * contents of the buffer upon creating, by specifying a block of host memory
 * to copy data from, and the appropriate flag from #GoclBufferFlags.
 * Also, buffers can be initialized at any time by calling
 * gocl_buffer_write() or gocl_buffer_write_sync().
 *
 * To read data from a buffer into host memory, gocl_buffer_read() and
 * gocl_buffer_read_sync() methods are provided. These are normally used after
 * the execution of a kernel that affected the contents of the buffer.
 *
 * Both gocl_buffer_write_sync() and gocl_buffer_read_sync() block program
 * execution, while gocl_buffer_write() and gocl_buffer_read() are asynchronous
 * versions and safe to call from the application's main loop.
 **/

/**
 * GoclBufferClass:
 * @parent_class: The parent class
 * @create_cl_mem: Virtual method used by deriving classes to create the
 *                 internal #cl_mem object
 * @read_all: Virtual method used by deriving classes to read all the
 *            content of the buffer into host memory
 *
 * The class for #GoclBuffer objects.
 **/

#include <string.h>
#include <gio/gio.h>

#include "gocl-buffer.h"

#include "gocl-private.h"
#include "gocl-decls.h"
#include "gocl-context.h"

struct _GoclBufferPrivate
{
  GoclContext *context;
  cl_mem buf;

  guint flags;
  gsize size;
  gpointer host_ptr;
};

/* properties */
enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_FLAGS,
  PROP_SIZE,
  PROP_HOST_PTR
};

static void           gocl_buffer_class_init            (GoclBufferClass *class);
static void           gocl_buffer_initable_iface_init   (GInitableIface *iface);
static gboolean       gocl_buffer_initable_init         (GInitable     *initable,
                                                         GCancellable  *cancellable,
                                                         GError       **error);
static void           gocl_buffer_init                  (GoclBuffer *self);
static void           gocl_buffer_finalize              (GObject *obj);

static void           set_property                      (GObject      *obj,
                                                         guint         prop_id,
                                                         const GValue *value,
                                                         GParamSpec   *pspec);
static void           get_property                      (GObject    *obj,
                                                         guint       prop_id,
                                                         GValue     *value,
                                                         GParamSpec *pspec);

static cl_int         create_cl_mem                     (GoclBuffer  *self,
                                                         cl_context   context,
                                                         cl_mem      *obj,
                                                         guint        flags,
                                                         gsize        size,
                                                         gpointer     host_ptr);
static cl_int         read_all                          (GoclBuffer          *self,
                                                         cl_mem               buffer,
                                                         cl_command_queue     queue,
                                                         gpointer             target_ptr,
                                                         gsize               *size,
                                                         gboolean             blocking,
                                                         cl_event            *event_wait_list,
                                                         guint                event_wait_list_len,
                                                         cl_event            *out_event);
static gpointer
map                                                     (GoclBuffer       *self,
                                                         cl_mem            buffer,
                                                         cl_command_queue  queue,
                                                         cl_map_flags      map_flags,
                                                         gboolean          blocking,
                                                         gsize             offset,
                                                         gsize             cb,
                                                         cl_event         *event_wait_list,
                                                         guint             event_wait_list_len,
                                                         cl_event         *out_event,
                                                         cl_int           *out_errcode);

G_DEFINE_TYPE_WITH_CODE (GoclBuffer, gocl_buffer, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                gocl_buffer_initable_iface_init));

#define GOCL_BUFFER_GET_PRIVATE(obj)                    \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                GOCL_TYPE_BUFFER,       \
                                GoclBufferPrivate))     \

static void
gocl_buffer_class_init (GoclBufferClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->finalize = gocl_buffer_finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  class->create_cl_mem = create_cl_mem;
  class->read_all = read_all;
  class->map = map;

  g_object_class_install_property (obj_class, PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Buffer's context",
                                                        "The context where this buffer was created",
                                                        GOCL_TYPE_CONTEXT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_FLAGS,
                                   g_param_spec_uint ("flags",
                                                      "Buffer flags",
                                                      "The flags used when creating this buffer",
                                                      GOCL_BUFFER_FLAGS_READ_WRITE,
                                                      GOCL_BUFFER_FLAGS_COPY_HOST_PTR,
                                                      GOCL_BUFFER_FLAGS_READ_WRITE,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_SIZE,
                                   g_param_spec_uint64 ("size",
                                                        "Buffer size",
                                                        "The size of this buffer",
                                                        0,
                                                        G_MAXUINT64,
                                                        0,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_HOST_PTR,
                                   g_param_spec_pointer ("host-ptr",
                                                         "Host pointer",
                                                         "The host pointer used by this buffer",
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GoclBufferPrivate));
}

static void
gocl_buffer_initable_iface_init (GInitableIface *iface)
{
  iface->init = gocl_buffer_initable_init;
}

static gboolean
gocl_buffer_initable_init (GInitable     *initable,
                            GCancellable  *cancellable,
                            GError       **error)
{
  GoclBuffer *self = GOCL_BUFFER (initable);
  cl_context ctx;
  cl_int err_code;

  ctx = gocl_context_get_context (self->priv->context);

  err_code = GOCL_BUFFER_GET_CLASS (self)->create_cl_mem (self,
                                                          ctx,
                                                          &self->priv->buf,
                                                          self->priv->flags,
                                                          self->priv->size,
                                                          self->priv->host_ptr);
  return ! gocl_error_check_opencl (err_code, error);
}

static void
gocl_buffer_init (GoclBuffer *self)
{
  GoclBufferPrivate *priv;

  self->priv = priv = GOCL_BUFFER_GET_PRIVATE (self);

  priv->host_ptr = NULL;
}

static void
gocl_buffer_finalize (GObject *obj)
{
  GoclBuffer *self = GOCL_BUFFER (obj);

  clReleaseMemObject (self->priv->buf);

  G_OBJECT_CLASS (gocl_buffer_parent_class)->finalize (obj);
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GoclBuffer *self;

  self = GOCL_BUFFER (obj);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->priv->context = g_value_dup_object (value);
      break;

    case PROP_FLAGS:
      self->priv->flags = g_value_get_uint (value);
      break;

    case PROP_SIZE:
      self->priv->size = g_value_get_uint64 (value);
      break;

    case PROP_HOST_PTR:
      self->priv->host_ptr = g_value_get_pointer (value);
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
  GoclBuffer *self;

  self = GOCL_BUFFER (obj);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->priv->context);
      break;

    case PROP_FLAGS:
      g_value_set_uint (value, self->priv->flags);
      break;

    case PROP_SIZE:
      g_value_set_uint64 (value, self->priv->size);
      break;

    case PROP_HOST_PTR:
      g_value_set_pointer (value, self->priv->host_ptr);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static cl_int
create_cl_mem (GoclBuffer  *self,
               cl_context   context,
               cl_mem      *obj,
               guint        flags,
               gsize        size,
               gpointer     host_ptr)
{
  cl_int err_code;

  *obj = clCreateBuffer (context,
                         flags,
                         size,
                         host_ptr,
                         &err_code);

  return err_code;
}

static cl_int
read_all (GoclBuffer          *self,
          cl_mem               buffer,
          cl_command_queue     queue,
          gpointer             target_ptr,
          gsize               *size,
          gboolean             blocking,
          cl_event            *event_wait_list,
          guint                event_wait_list_len,
          cl_event            *out_event)
{
  if (size != NULL)
    *size = self->priv->size;

  return clEnqueueReadBuffer (queue,
                              buffer,
                              blocking,
                              0,
                              self->priv->size,
                              target_ptr,
                              event_wait_list_len,
                              event_wait_list,
                              out_event);
}

static gpointer
map (GoclBuffer       *self,
     cl_mem            buffer,
     cl_command_queue  queue,
     cl_map_flags      map_flags,
     gboolean          blocking,
     gsize             offset,
     gsize             cb,
     cl_event         *event_wait_list,
     guint             event_wait_list_len,
     cl_event         *out_event,
     cl_int           *out_errcode)
{
  return clEnqueueMapBuffer (queue,
                             buffer,
                             blocking,
                             map_flags,
                             offset,
                             cb,
                             event_wait_list_len,
                             event_wait_list,
                             out_event,
                             out_errcode);
}

/* public */

/**
 * gocl_buffer_new:
 * @context: A #GoclContext to attach the buffer to
 * @flags: An OR'ed combination of values from #GoclBufferFlags
 * @size: The size of the buffer, in bytes
 * @host_ptr: (allow-none) (type guint64): A pointer to memory in the host system, or %NULL
 *
 * Creates a new buffer on context's memory. Depending on flags, the @host_ptr pointer can be
 * used to initialize the contents of the buffer from a block of memory in the host.
 *
 * Returns: (transfer full): A newly created #GoclBuffer, or %NULL on error
 **/
GoclBuffer *
gocl_buffer_new (GoclContext  *context,
                 guint         flags,
                 gsize         size,
                 gpointer      host_ptr)
{
  GError **error;

  g_return_val_if_fail (GOCL_IS_CONTEXT (context), NULL);

  error = gocl_error_prepare ();

  return g_initable_new (GOCL_TYPE_BUFFER,
                         NULL,
                         error,
                         "context", context,
                         "flags", flags,
                         "size", size,
                         "host-ptr", host_ptr,
                         NULL);
}

/**
 * gocl_buffer_get_buffer:
 * @self: The #GoclBuffer
 *
 * Retrieves the internal OpenCL #cl_mem object. This is not normally
 * called by applications. It is rather a low-level, internal API.
 *
 * Returns: (type guint64) (transfer none): The internal #cl_mem object
 **/
cl_mem
gocl_buffer_get_buffer (GoclBuffer *self)
{
  g_return_val_if_fail (GOCL_IS_BUFFER (self), NULL);

  return self->priv->buf;
}

/**
 * gocl_buffer_get_context:
 * @buffer: The #GoclBuffer
 *
 * Retrieves the #GoclContext the buffer belongs to.
 *
 * Returns: (transfer none): A #GoclContext object
 **/
GoclContext *
gocl_buffer_get_context (GoclBuffer *buffer)
{
  g_return_val_if_fail (GOCL_IS_BUFFER (buffer), NULL);

  return buffer->priv->context;
}

/**
 * gocl_buffer_read:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @target_ptr: (array length=size) (element-type guint8): The pointer to copy
 * the data to
 * @size: The size of the data to be read
 * @offset: The offset to start reading from
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 *
 * Asynchronously reads a block of data of @size bytes from remote context
 * into host memory, starting at @offset. The operation is enqueued in
 * @queue, and the program execution continues without blocking. For a
 * synchronous version of this methid, see gocl_buffer_read_sync().
 *
 * A #GoclEvent is returned so that the application can get notified when the
 * operation finishes, by calling gocl_event_then(). Also, the returned event
 * can be added to the @event_wait_list argument of other operations, to
 * synchronize their execution with the completion of this operation.
 *
 * If @event_wait_list is provided, the read operation will start only when
 * all the #GoclEvent in the list have triggered.
 *
 * Returns: (transfer none): A #GoclEvent to get notified when the read
 * operation finishes
 **/
GoclEvent *
gocl_buffer_read (GoclBuffer *self,
                  GoclQueue  *queue,
                  gpointer    target_ptr,
                  gsize       size,
                  goffset     offset,
                  GList      *event_wait_list)
{
  GError *error = NULL;
  cl_int err_code;
  cl_event event;
  cl_command_queue _queue;

  GoclEvent *_event = NULL;
  GoclEventResolverFunc resolver_func;

  cl_event *_event_wait_list = NULL;

  g_return_val_if_fail (GOCL_IS_BUFFER (self), NULL);
  g_return_val_if_fail (GOCL_IS_QUEUE (queue), NULL);

  _event_wait_list = gocl_event_list_to_array (event_wait_list, NULL);

  _queue = gocl_queue_get_queue (queue);

  err_code = clEnqueueReadBuffer (_queue,
                                  self->priv->buf,
                                  CL_FALSE,
                                  offset,
                                  size,
                                  target_ptr,
                                  g_list_length (event_wait_list),
                                  _event_wait_list,
                                  &event);
  g_free (_event_wait_list);

  if (gocl_error_check_opencl (err_code, &error))
    {
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

  gocl_event_idle_unref (_event);

  return _event;
}

/**
 * gocl_buffer_read_sync:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @target_ptr: (array length=size) (element-type guint8): The pointer to copy
 * the data to
 * @size: The size of the data to be read
 * @offset: The offset to start reading from
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 *
 * Reads a block of data of @size bytes from remote context into host memory,
 * starting at @offset. The operation is actually enqueued in @queue, and
 * the program execution blocks until the read finishes. If @event_wait_list
 * is provided, the read operation will start only when all the #GoclEvent in
 * the list have triggered.
 *
 * Returns: %TRUE on success, %FALSE on error
 **/
gboolean
gocl_buffer_read_sync (GoclBuffer *self,
                       GoclQueue  *queue,
                       gpointer    target_ptr,
                       gsize       size,
                       goffset     offset,
                       GList      *event_wait_list)
{
  cl_command_queue _queue;
  cl_int err_code;
  cl_event *_event_wait_list = NULL;

  g_return_val_if_fail (GOCL_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (GOCL_IS_QUEUE (queue), FALSE);

  _event_wait_list = gocl_event_list_to_array (event_wait_list, NULL);

  _queue = gocl_queue_get_queue (queue);

  err_code = clEnqueueReadBuffer (_queue,
                                  self->priv->buf,
                                  CL_TRUE,
                                  offset,
                                  size,
                                  target_ptr,
                                  g_list_length (event_wait_list),
                                  _event_wait_list,
                                  NULL);

  g_free (_event_wait_list);

  return ! gocl_error_check_opencl_internal (err_code);
}

/**
 * gocl_buffer_write:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @data: A pointer to write data from
 * @size: The size of the data to be written
 * @offset: The offset to start writing data to
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 *
 * Asynchronously writes a block of data of @size bytes from host memory into
 * remote context, starting at @offset. The operation is enqueued in
 * @queue, and the program execution continues without blocking. For a
 * synchronous version of this methid, see gocl_buffer_write_sync().
 *
 * A #GoclEvent is returned so that the application can get notified when the
 * operation finishes, by calling gocl_event_then(). Also, the returned event
 * can be added to the @event_wait_list argument of other operations, to
 * synchronize their execution with the completion of this operation.
 *
 * If @event_wait_list is provided, the write operation will start only when
 * all the #GoclEvent in the list have triggered.
 *
 * Returns: (transfer none): A #GoclEvent to get notified when the write
 * operation finishes
 **/
GoclEvent *
gocl_buffer_write (GoclBuffer     *self,
                   GoclQueue      *queue,
                   const gpointer  data,
                   gsize           size,
                   goffset         offset,
                   GList          *event_wait_list)
{
  GError *error = NULL;
  cl_int err_code;
  cl_event event;
  cl_command_queue _queue;

  GoclEvent *_event = NULL;
  GoclEventResolverFunc resolver_func;

  cl_event *_event_wait_list = NULL;

  g_return_val_if_fail (GOCL_IS_BUFFER (self), NULL);
  g_return_val_if_fail (GOCL_IS_QUEUE (queue), NULL);

  _event_wait_list = gocl_event_list_to_array (event_wait_list, NULL);

  _queue = gocl_queue_get_queue (queue);

  err_code = clEnqueueWriteBuffer (_queue,
                                   self->priv->buf,
                                   CL_FALSE,
                                   offset,
                                   size,
                                   data,
                                   g_list_length (event_wait_list),
                                   _event_wait_list,
                                   &event);
  g_free (_event_wait_list);

  if (gocl_error_check_opencl (err_code, &error))
    {
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

  gocl_event_idle_unref (_event);

  return _event;
}

/**
 * gocl_buffer_write_sync:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @data: A pointer to write data from
 * @size: The size of the data to be written
 * @offset: The offset to start writing data to
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 *
 * Writes a block of data of @size bytes from host memory into remote context
 * memory, starting at @offset. The operation is actually enqueued in @queue, and
 * the program execution blocks until the read finishes. If @event_wait_list
 * is provided, the read operation will start only when all the #GoclEvent in
 * the list have triggered.
 *
 * Returns: %TRUE on success, %FALSE on error
 **/
gboolean
gocl_buffer_write_sync (GoclBuffer     *self,
                        GoclQueue      *queue,
                        const gpointer  data,
                        gsize           size,
                        goffset         offset,
                        GList          *event_wait_list)
{
  cl_command_queue _queue;
  cl_int err_code;
  cl_event *_event_wait_list = NULL;

  g_return_val_if_fail (GOCL_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (GOCL_IS_QUEUE (queue), FALSE);

  _event_wait_list = gocl_event_list_to_array (event_wait_list, NULL);

  _queue = gocl_queue_get_queue (queue);

  err_code = clEnqueueWriteBuffer (_queue,
                                   self->priv->buf,
                                   CL_TRUE,
                                   offset,
                                   size,
                                   data,
                                   g_list_length (event_wait_list),
                                   _event_wait_list,
                                   NULL);

  g_free (_event_wait_list);

  return ! gocl_error_check_opencl_internal (err_code);
}

/**
 * gocl_buffer_list_to_array:
 * @list: (element-type Gocl.Buffer) (allow-none): A #GList containing
 * #GoclBuffer objects
 * @len: (out) (allow-none): A pointer to a value to retrieve list length
 *
 * A convenient method to retrieve a #GList of #GoclBuffer's as an array of
 * #cl_mem's corresponding to the internal objects of each #GoclBuffer in
 * the #GList. This is a rather low-level method and should not normally be
 * called by applications.
 *
 * Returns: (transfer container) (array length=len): An array of #cl_mem
 *   objects. Free with g_free().
 **/
cl_mem *
gocl_buffer_list_to_array (GList *list, guint *len)
{
  cl_mem *arr = NULL;
  gint i;
  guint _len;
  GList *node;

  _len = g_list_length (list);

  if (len != NULL)
    *len = _len;

  if (_len == 0)
    return arr;

  arr = g_new (cl_mem, g_list_length (list));

  node = list;
  i = 0;
  while (node != NULL)
    {
      g_return_val_if_fail (GOCL_IS_BUFFER (node->data), NULL);

      arr[i] = gocl_buffer_get_buffer (GOCL_BUFFER (node->data));

      i++;
      node = g_list_next (node);
    }

  return arr;
}

/**
 * gocl_buffer_read_all_sync:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @target_ptr: (array length=size) (element-type guint8) (allow-none): The
 * pointer to copy the data to
 * @size: (out) (allow-none): A pointer to retrieve the size of the buffer,
 * or %NULL
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 *
 * Reads all the data in buffer from remote context into the host memory
 * referenced by @target_ptr. The operation is enqueued in @queue, and the
 * program execution blocks until the read finishes.
 *
 * If @size is not %NULL, it will store the total size read.
 *
 * If @event_wait_list is provided, the read operation will
 * start only when all the #GoclEvent in the list have triggered.
 *
 * Returns: %TRUE on success, %FALSE on error
 **/
gboolean
gocl_buffer_read_all_sync (GoclBuffer *self,
                           GoclQueue  *queue,
                           gpointer    target_ptr,
                           gsize      *size,
                           GList      *event_wait_list)
{
  GoclBufferClass *class;
  cl_command_queue _queue;
  cl_int err_code;
  cl_event *_event_wait_list = NULL;
  guint event_wait_list_len;
  cl_mem buffer;

  g_return_val_if_fail (GOCL_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (GOCL_IS_QUEUE (queue), FALSE);
  g_return_val_if_fail (target_ptr != NULL, FALSE);

  _event_wait_list = gocl_event_list_to_array (event_wait_list,
                                               &event_wait_list_len);

  _queue = gocl_queue_get_queue (queue);

  buffer = gocl_buffer_get_buffer (GOCL_BUFFER (self));

  class = GOCL_BUFFER_GET_CLASS (self);
  g_assert (class->read_all != NULL);

  err_code = class->read_all (self,
                              buffer,
                              _queue,
                              target_ptr,
                              size,
                              TRUE,
                              _event_wait_list,
                              event_wait_list_len,
                              NULL);
  g_free (_event_wait_list);

  return ! gocl_error_check_opencl_internal (err_code);
}

/**
 * gocl_buffer_map:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @offset: The offset in bytes in the buffer
 * @size: The size in bytes of the buffer
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 *
 * Returns: (transfer none): A #GoclEvent to get notified when the map
 * operation finishes
 */
GoclEvent *
gocl_buffer_map (GoclBuffer         *self,
                 GoclQueue          *queue,
                 GoclBufferMapFlags map_flags,
                 gsize              offset,
                 gsize              size,
                 GList              *event_wait_list)
{
  GoclBufferClass *class;
  cl_command_queue _queue;
  cl_int err_code;
  cl_event *_event_wait_list = NULL;
  guint event_wait_list_len;
  cl_mem buffer;
  cl_event event = NULL;
  GoclEventResolverFunc resolver_func;
  GoclEvent *_event;
  GError *error = NULL;

  g_return_val_if_fail (GOCL_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (GOCL_IS_QUEUE (queue), FALSE);

  _event_wait_list = gocl_event_list_to_array (event_wait_list,
                                               &event_wait_list_len);

  _queue = gocl_queue_get_queue (queue);

  buffer = gocl_buffer_get_buffer (GOCL_BUFFER (self));

  class = GOCL_BUFFER_GET_CLASS (self);
  g_assert (class->read_all != NULL);

  class->map (self,
              buffer,
              _queue,
              map_flags,
              FALSE,
              offset,
              size,
              _event_wait_list,
              event_wait_list_len,
              &event,
              &err_code);

  g_free (_event_wait_list);

  if (gocl_error_check_opencl (err_code, &error))
    {
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

  gocl_event_idle_unref (_event);

  return _event;
}

/**
 * gocl_buffer_map_sync:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @offset: The offset in bytes in the buffer
 * @size: The size in bytes of the buffer
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 *
 * Returns: A pointer to the mapped buffer
 */
gpointer
gocl_buffer_map_sync (GoclBuffer         *self,
                      GoclQueue          *queue,
                      GoclBufferMapFlags map_flags,
                      gsize              offset,
                      gsize              size,
                      GList              *event_wait_list)
{
  GoclBufferClass *class;
  cl_command_queue _queue;
  cl_int err_code;
  cl_event *_event_wait_list = NULL;
  guint event_wait_list_len;
  cl_mem buffer;
  cl_event event = NULL;
  gpointer ret;

  g_return_val_if_fail (GOCL_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (GOCL_IS_QUEUE (queue), FALSE);

  _event_wait_list = gocl_event_list_to_array (event_wait_list,
                                               &event_wait_list_len);

  _queue = gocl_queue_get_queue (queue);

  buffer = gocl_buffer_get_buffer (GOCL_BUFFER (self));

  class = GOCL_BUFFER_GET_CLASS (self);
  g_assert (class->read_all != NULL);

  ret = class->map (self,
                    buffer,
                    _queue,
                    map_flags,
                    FALSE,
                    offset,
                    size,
                    _event_wait_list,
                    event_wait_list_len,
                    &event,
                    &err_code);

  g_free (_event_wait_list);

  return ret;
}

/**
 * gocl_buffer_unmap:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @mapped_ptr: The pointer where the buffer was previously mapped with #gocl_buffer_map
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 *
 * Returns: %TRUE if the buffer was successfully unmapped, otherwise %FALSE
 */
gboolean
gocl_buffer_unmap (GoclBuffer   *self,
                   GoclQueue    *queue,
                   gpointer      mapped_ptr,
                   GList        *event_wait_list)
{
  gint err_code;
  cl_command_queue _queue;
  cl_mem buffer;
  cl_event *_event_wait_list = NULL;
  guint event_wait_list_len;
  cl_event event = NULL;

  g_return_val_if_fail (GOCL_IS_BUFFER (self), FALSE);
  g_return_val_if_fail (GOCL_IS_QUEUE (queue), FALSE);
  g_return_val_if_fail (mapped_ptr != NULL, FALSE);

  _event_wait_list = gocl_event_list_to_array (event_wait_list,
                                               &event_wait_list_len);

  _queue = gocl_queue_get_queue (queue);

  buffer = gocl_buffer_get_buffer (GOCL_BUFFER (self));

  err_code = clEnqueueUnmapMemObject (_queue, buffer, mapped_ptr, event_wait_list_len, _event_wait_list, &event);

  g_free (_event_wait_list);

  return ! gocl_error_check_opencl_internal (err_code);
}
