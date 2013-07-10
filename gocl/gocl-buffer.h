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

#include <gocl-decls.h>
#include <gocl-queue.h>
#include <gocl-event.h>

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
  gboolean (* create_cl_mem) (GoclBuffer  *self,
                              cl_context   context,
                              cl_mem      *obj,
                              guint        flags,
                              gsize        size,
                              GError     **error);
};

GType                  gocl_buffer_get_type                   (void) G_GNUC_CONST;

cl_mem                 gocl_buffer_get_buffer                 (GoclBuffer *self);

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
                                                               GList       *event_wait_list,
                                                               GError     **error);
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
                                                               GList           *event_wait_list,
                                                               GError         **error);

G_END_DECLS

#endif /* __GOCL_BUFFER_H__ */
