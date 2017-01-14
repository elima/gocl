/*
 * gocl-buffer.h
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

#ifndef __GOCL_BUFFER_H__
#define __GOCL_BUFFER_H__

#include <glib-object.h>
#include <CL/opencl.h>

#include "gocl-decls.h"
#include "gocl-queue.h"
#include "gocl-event.h"

G_BEGIN_DECLS

#define GOCL_TYPE_BUFFER              (gocl_buffer_get_type ())
#define GOCL_BUFFER(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOCL_TYPE_BUFFER, GoclBuffer))
#define GOCL_BUFFER_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GOCL_TYPE_BUFFER, GoclBufferClass))
#define GOCL_IS_BUFFER(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOCL_TYPE_BUFFER))
#define GOCL_IS_BUFFER_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GOCL_TYPE_BUFFER))
#define GOCL_BUFFER_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GOCL_TYPE_BUFFER, GoclBufferClass))

typedef struct _GoclBufferClass GoclBufferClass;
typedef struct _GoclBuffer GoclBuffer;
typedef struct _GoclBufferPrivate GoclBufferPrivate;

struct _GoclBuffer
{
  GObject parent_instance;

  GoclBufferPrivate *priv;
};

struct _GoclBufferClass
{
  GObjectClass parent_class;

  /* virtual methods */
  cl_int (* create_cl_mem) (GoclBuffer  *self,
                            cl_context   context,
                            cl_mem      *obj,
                            guint        flags,
                            gsize        size,
                            gpointer     host_ptr);
  cl_int   (* read_all)    (GoclBuffer          *self,
                            cl_mem               buffer,
                            cl_command_queue     queue,
                            gpointer             target_ptr,
                            gsize               *size,
                            gboolean             blocking,
                            cl_event            *event_wait_list,
                            guint                event_wait_list_len,
                            cl_event            *out_event);
  gpointer (* map)         (GoclBuffer       *self,
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
};

GType                  gocl_buffer_get_type                   (void) G_GNUC_CONST;

GoclEvent *            gocl_buffer_read                       (GoclBuffer *self,
                                                               GoclQueue  *queue,
                                                               gpointer    target_ptr,
                                                               gsize       size,
                                                               goffset     offset,
                                                               GList      *event_wait_list);
gboolean               gocl_buffer_read_sync                  (GoclBuffer  *self,
                                                               GoclQueue   *queue,
                                                               gpointer     target_ptr,
                                                               gsize        size,
                                                               goffset      offset,
                                                               GList       *event_wait_list);
GoclEvent *            gocl_buffer_write                      (GoclBuffer     *self,
                                                               GoclQueue      *queue,
                                                               const gpointer  data,
                                                               gsize           size,
                                                               goffset         offset,
                                                               GList          *event_wait_list);
gboolean               gocl_buffer_write_sync                 (GoclBuffer      *self,
                                                               GoclQueue       *queue,
                                                               const gpointer   data,
                                                               gsize            size,
                                                               goffset          offset,
                                                               GList           *event_wait_list);

gboolean               gocl_buffer_read_all_sync              (GoclBuffer  *self,
                                                               GoclQueue   *queue,
                                                               gpointer     target_ptr,
                                                               gsize       *size,
                                                               GList       *event_wait_list);

GoclEvent *            gocl_buffer_map                        (GoclBuffer         *self,
                                                               GoclQueue          *queue,
                                                               GoclBufferMapFlags  map_flags,
                                                               gsize               offset,
                                                               gsize               size,
                                                               GList              *event_wait_list);

gpointer               gocl_buffer_map_sync                   (GoclBuffer         *self,
                                                               GoclQueue          *queue,
                                                               GoclBufferMapFlags  map_flags,
                                                               gsize               offset,
                                                               gsize               size,
                                                               GList              *event_wait_list);

GBytes *               gocl_buffer_map_as_bytes_sync          (GoclBuffer *self,
                                                               GoclQueue          *queue,
                                                               GoclBufferMapFlags  map_flags,
                                                               gsize               offset,
                                                               gsize               size,
                                                               GList              *event_wait_list);

gboolean               gocl_buffer_unmap                      (GoclBuffer   *self,
                                                               GoclQueue    *queue,
                                                               gpointer      mapped_ptr,
                                                               GList        *event_wait_list);

cl_mem *               gocl_buffer_list_to_array              (GList *list,
                                                               guint *len);

G_END_DECLS

#endif /* __GOCL_BUFFER_H__ */
