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
 * gocl_context_create_buffer() method. It is possible to initialize the
 * contents of the buffer upon creating, by specifying a block of host memory
 * to copy data from, and the appropriate flag from #GoclBufferFlags.
 * Also, buffers can be initialized at any time by calling
 * gocl_buffer_write_sync().
 *
 * To read data from a buffer into host memory, gocl_buffer_read_sync() method
 * is provided. This is normally used after the execution of a kernel that
 * affected the contents of the buffer.
 *
 * Both gocl_buffer_write_sync() and gocl_buffer_read_sync() are block program
 * execution. Asynchronous function will shortly be provided as well.
 **/

/**
 * GoclBufferClass:
 * @parent_class: The parent class
 *
 * The class for #GoclBuffer objects.
 **/

#include <string.h>
#include <gio/gio.h>

#include "gocl-buffer.h"

#include "gocl-error.h"
#include "gocl-decls.h"
#include "gocl-context.h"
#include "gocl-event.h"

struct _GoclBufferPrivate
{
  GoclContext *context;
  cl_mem buf;

  guint flags;
  gsize size;
};

/* properties */
enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_FLAGS,
  PROP_SIZE
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
  cl_int err_code = 0;
  cl_context ctx;

  ctx = gocl_context_get_context (self->priv->context);

  self->priv->buf = clCreateBuffer (ctx,
                                    self->priv->flags,
                                    self->priv->size,
                                    NULL,
                                    &err_code);
  if (gocl_error_check_opencl (err_code, error))
    return FALSE;

  return TRUE;
}

static void
gocl_buffer_init (GoclBuffer *self)
{
  GoclBufferPrivate *priv;

  self->priv = priv = GOCL_BUFFER_GET_PRIVATE (self);
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

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

/* public */

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
 * gocl_buffer_read_sync:
 * @self: The #GoclBuffer
 * @queue: A #GoclQueue where the operation will be enqueued
 * @target_ptr: (array length=size) (element-type guint8): The pointer to copy
 * the data to
 * @size: The size of the data to be read
 * @offset: The offset to start reading from
 * @event_wait_list: (element-type Gocl.Event) (allow-none): List or #GoclEvent
 * object to wait for, or %NULL
 * @error: (out) (allow-none): A pointer to a #GError, or %NULL
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
gocl_buffer_read_sync (GoclBuffer  *self,
                       GoclQueue   *queue,
                       gpointer     target_ptr,
                       gsize        size,
                       goffset      offset,
                       GList       *event_wait_list,
                       GError     **error)
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

  return ! gocl_error_check_opencl (err_code, error);
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
 * @error: (out) (allow-none): A pointer to a #GError, or %NULL
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
gocl_buffer_write_sync (GoclBuffer      *self,
                        GoclQueue       *queue,
                        const gpointer   data,
                        gsize            size,
                        goffset          offset,
                        GList           *event_wait_list,
                        GError         **error)
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

  return ! gocl_error_check_opencl (err_code, error);
}
