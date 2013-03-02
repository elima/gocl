/*
 * gocl-event.c
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

#include <string.h>

#include "gocl-event.h"

#include "gocl-error.h"

struct _GoclEventPrivate
{
  cl_event event;
  GoclQueue *queue;

  GError *error;
  GoclEventResolverFunc resolver_func;

  gboolean already_resolved;
  gboolean waiting_event;

  GList *closure_list;
  GThread *thread;

  gint complete_src_id;
};

typedef struct
{
  GoclEventCallback callback;
  gpointer user_data;
  GMainContext *context;
  GoclEvent *self;
} Closure;

/* properties */
enum
{
  PROP_0,
  PROP_EVENT,
  PROP_QUEUE
};

static void           gocl_event_class_init            (GoclEventClass *class);
static void           gocl_event_init                  (GoclEvent *self);
static void           gocl_event_dispose               (GObject *obj);
static void           gocl_event_finalize              (GObject *obj);

static void           set_property                     (GObject      *obj,
                                                        guint         prop_id,
                                                        const GValue *value,
                                                        GParamSpec   *pspec);
static void           get_property                     (GObject    *obj,
                                                        guint       prop_id,
                                                        GValue     *value,
                                                        GParamSpec *pspec);

static void           free_closure                     (gpointer user_data);

static void           gocl_event_resolve               (GoclEvent *self,
                                                        GError    *error);

static gpointer       wait_event_thread_func           (gpointer user_data);


G_DEFINE_TYPE (GoclEvent, gocl_event, G_TYPE_OBJECT);

#define GOCL_EVENT_GET_PRIVATE(obj)                \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),             \
                                GOCL_TYPE_EVENT,   \
                                GoclEventPrivate)) \

static void
gocl_event_class_init (GoclEventClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->dispose = gocl_event_dispose;
  obj_class->finalize = gocl_event_finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  g_object_class_install_property (obj_class, PROP_EVENT,
                                   g_param_spec_pointer ("event",
                                                         "Event",
                                                         "The native event object",
                                                         G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                         G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (obj_class, PROP_QUEUE,
                                   g_param_spec_object ("queue",
                                                        "Command queue",
                                                        "The command queue associated with this event",
                                                        GOCL_TYPE_QUEUE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GoclEventPrivate));
}

static void
gocl_event_init (GoclEvent *self)
{
  GoclEventPrivate *priv;

  self->priv = priv = GOCL_EVENT_GET_PRIVATE (self);

  priv->event = NULL;

  priv->error = NULL;
  priv->resolver_func = gocl_event_resolve;

  priv->already_resolved = FALSE;
  priv->waiting_event = FALSE;

  priv->closure_list = NULL;
  priv->thread = NULL;
}

static void
gocl_event_dispose (GObject *obj)
{
  GoclEvent *self = GOCL_EVENT (obj);

  if (self->priv->queue != NULL)
    {
      g_object_unref (self->priv->queue);
      self->priv->queue = NULL;
    }

  G_OBJECT_CLASS (gocl_event_parent_class)->dispose (obj);
}

static void
gocl_event_finalize (GObject *obj)
{
  GoclEvent *self = GOCL_EVENT (obj);

  if (self->priv->event != NULL)
    clReleaseEvent (self->priv->event);

  if (self->priv->error != NULL)
    g_error_free (self->priv->error);

  if (self->priv->thread != NULL)
    g_thread_unref (self->priv->thread);

  if (self->priv->closure_list != NULL)
    {
      /* @TODO: should we call any awaiting closure? */
      g_list_free_full (self->priv->closure_list, free_closure);
    }

  G_OBJECT_CLASS (gocl_event_parent_class)->finalize (obj);
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GoclEvent *self;

  self = GOCL_EVENT (obj);

  switch (prop_id)
    {
    case PROP_EVENT:
      self->priv->event = g_value_get_pointer (value);
      break;

    case PROP_QUEUE:
      self->priv->queue = g_value_dup_object (value);
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
  GoclEvent *self;

  self = GOCL_EVENT (obj);

  switch (prop_id)
    {
    case PROP_EVENT:
      g_value_set_pointer (value, self->priv->event);
      break;

    case PROP_QUEUE:
      g_value_set_object (value, self->priv->queue);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

guint
timeout_add (GMainContext *context,
             guint         timeout,
             gint          priority,
             GSourceFunc   callback,
             gpointer      user_data)
{
  guint src_id;
  GSource *src;

  if (context == NULL)
    context = g_main_context_get_thread_default ();

  if (timeout == 0)
    src = g_idle_source_new ();
  else
    src = g_timeout_source_new (timeout);

  g_source_set_priority (src, priority);

  g_source_set_callback (src,
                         callback,
                         user_data,
                         NULL);
  src_id = g_source_attach (src, context);
  g_source_unref (src);

  return src_id;
}

static void
free_closure (gpointer user_data)
{
  Closure *closure = user_data;

  g_object_unref (closure->self);

  g_slice_free (Closure, closure);
}

static void
gocl_event_resolve (GoclEvent *self, GError *error)
{
  g_return_if_fail (GOCL_IS_EVENT (self));
  g_return_if_fail (! self->priv->already_resolved);

  self->priv->already_resolved = TRUE;

  if (error != NULL)
    self->priv->error = g_error_copy (error);
}

static gboolean
notify_event_completed_in_caller_context (gpointer user_data)
{
  Closure *closure = user_data;
  GoclEvent *self = closure->self;

  closure->callback (closure->self,
                     self->priv->error,
                     closure->user_data);

  free_closure (closure);

  return FALSE;
}

static gboolean
event_completed (gpointer user_data)
{
  GoclEvent *self = GOCL_EVENT (user_data);

  self->priv->complete_src_id = 0;

  self->priv->already_resolved = TRUE;
  self->priv->waiting_event = FALSE;

  GList *node = self->priv->closure_list;
  while (node != NULL)
    {
      Closure *closure = node->data;

      timeout_add (closure->context,
                   0,
                   G_PRIORITY_DEFAULT,
                   notify_event_completed_in_caller_context,
                   closure);

      node = g_list_next (node);
    }

  g_list_free (self->priv->closure_list);
  self->priv->closure_list = NULL;

  return FALSE;
}

static gpointer
wait_event_thread_func (gpointer user_data)
{
  GoclEvent *self = GOCL_EVENT (user_data);

  clWaitForEvents (1, &self->priv->event);

  timeout_add (NULL,
               0,
               G_PRIORITY_DEFAULT,
               event_completed,
               self);

  return NULL;
}

/* public */

/**
 * gocl_event_set_event:
 * @event: (type guint64):
 *
 **/
void
gocl_event_set_event (GoclEvent *self, cl_event event)
{
  g_return_if_fail (GOCL_IS_EVENT (self));
  g_return_if_fail (event != NULL);

  self->priv->event = event;
}

/**
 * gocl_event_get_event:
 *
 * Returns: (transfer none) (type guint64):
 **/
cl_event
gocl_event_get_event (GoclEvent *self)
{
  g_return_val_if_fail (GOCL_IS_EVENT (self), NULL);

  return self->priv->event;
}

/**
 * gocl_event_get_queue:
 *
 * Returns: (transfer none):
 **/
GoclQueue *
gocl_event_get_queue (GoclEvent *self)
{
  g_return_val_if_fail (GOCL_IS_EVENT (self), NULL);

  return self->priv->queue;
}

/**
 * gocl_event_steal_resolver_func: (skip)
 *
 * Returns:
 **/
GoclEventResolverFunc
gocl_event_steal_resolver_func (GoclEvent *self)
{
  GoclEventResolverFunc func;

  g_return_val_if_fail (GOCL_IS_EVENT (self), NULL);

  func = self->priv->resolver_func;
  self->priv->resolver_func = NULL;

  return func;
}

/**
 * gocl_event_then:
 * @callback: (scope async):
 *
 **/
void
gocl_event_then (GoclEvent         *self,
                 GoclEventCallback  callback,
                 gpointer           user_data)
{
  Closure *closure;

  g_return_if_fail (GOCL_IS_EVENT (self));
  g_return_if_fail (callback != NULL);

  closure = g_slice_new0 (Closure);
  closure->callback = callback;
  closure->user_data = user_data;
  closure->context = g_main_context_get_thread_default ();
  closure->self = g_object_ref (self);

  if (self->priv->already_resolved)
    {
      timeout_add (closure->context,
                   0,
                   G_PRIORITY_DEFAULT,
                   notify_event_completed_in_caller_context,
                   closure);
    }
  else if (! self->priv->waiting_event)
    {
      g_assert (self->priv->event != NULL);

      self->priv->waiting_event = TRUE;

      self->priv->closure_list = g_list_append (self->priv->closure_list,
                                                closure);

      self->priv->thread = g_thread_new ("gocl-event-thread",
                                         wait_event_thread_func,
                                         self);
    }
}
