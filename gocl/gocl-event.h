/*
 * gocl-event.h
 *
 * Gocl - GLib/GObject wrapper for OpenCL
 * Copyright (C) 2013 Igalia S.L.
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

#ifndef __GOCL_EVENT_H__
#define __GOCL_EVENT_H__

#include <glib-object.h>
#include <CL/opencl.h>

#include <gocl-queue.h>

G_BEGIN_DECLS

#define GOCL_TYPE_EVENT              (gocl_event_get_type ())
#define GOCL_EVENT(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOCL_TYPE_EVENT, GoclEvent))
#define GOCL_EVENT_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GOCL_TYPE_EVENT, GoclEventClass))
#define GOCL_IS_EVENT(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOCL_TYPE_EVENT))
#define GOCL_IS_EVENT_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GOCL_TYPE_EVENT))
#define GOCL_EVENT_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GOCL_TYPE_EVENT, GoclEventClass))

typedef struct _GoclEventClass GoclEventClass;
typedef struct _GoclEvent GoclEvent;
typedef struct _GoclEventPrivate GoclEventPrivate;

typedef void (* GoclEventResolverFunc) (GoclEvent *self, GError *error);

typedef void (* GoclEventCallback)     (GoclEvent *self,
                                        GError    *error,
                                        gpointer   user_data);

struct _GoclEvent
{
  GObject parent_instance;

  GoclEventPrivate *priv;
};

struct _GoclEventClass
{
  GObjectClass parent_class;
};

GType                  gocl_event_get_type                   (void) G_GNUC_CONST;

void                   gocl_event_set_event                  (GoclEvent *self,
                                                              cl_event   event);
cl_event               gocl_event_get_event                  (GoclEvent *self);

GoclQueue *            gocl_event_get_queue                  (GoclEvent *self);

GoclEventResolverFunc  gocl_event_steal_resolver_func        (GoclEvent *self);

void                   gocl_event_then                       (GoclEvent         *self,
                                                              GoclEventCallback  callback,
                                                              gpointer           user_data);

cl_event *             gocl_event_list_to_array              (GList *event_list,
                                                              gsize *len);

void                   gocl_event_idle_unref                 (GoclEvent *self);

G_END_DECLS

#endif /* __GOCL_EVENT_H__ */
