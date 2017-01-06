/*
 * gocl-queue.h
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

#ifndef __GOCL_QUEUE_H__
#define __GOCL_QUEUE_H__

#include <glib-object.h>
#include <CL/opencl.h>

G_BEGIN_DECLS

#define GOCL_TYPE_QUEUE              (gocl_queue_get_type ())
#define GOCL_QUEUE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOCL_TYPE_QUEUE, GoclQueue))
#define GOCL_QUEUE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GOCL_TYPE_QUEUE, GoclQueueClass))
#define GOCL_IS_QUEUE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOCL_TYPE_QUEUE))
#define GOCL_IS_QUEUE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GOCL_TYPE_QUEUE))
#define GOCL_QUEUE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GOCL_TYPE_QUEUE, GoclQueueClass))

typedef struct _GoclQueueClass GoclQueueClass;
typedef struct _GoclQueue GoclQueue;
typedef struct _GoclQueuePrivate GoclQueuePrivate;

struct _GoclQueue
{
  GObject parent_instance;

  GoclQueuePrivate *priv;
};

struct _GoclQueueClass
{
  GObjectClass parent_class;
};

GType                  gocl_queue_get_type                   (void) G_GNUC_CONST;

guint                  gocl_queue_get_flags                  (GoclQueue *self);

gboolean               gocl_queue_flush                      (GoclQueue *self);

gboolean               gocl_queue_finish                     (GoclQueue *self);

G_END_DECLS

#endif /* __GOCL_QUEUE_H__ */
