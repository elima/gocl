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

/**
 * SECTION:gocl-event
 * @short_description: Object that represents the future completion of an
 * asynchronous operation
 * @stability: Unstable
 *
 * A #GoclEvent is created as the result of requesting an asynchronous
 * operation on a #GoclQueue (the command queue of a device). Examples
 * of these operations are the execution of a kernel with
 * gocl_kernel_run_in_device() and reading from / writing to #GoclBuffer
 * objects in a non-blocking fashion (not yet implemented).
 *
 * A #GoclEvent is used by applications to get a notification when the
 * corresponding operation completes, by calling gocl_event_then().
 *
 * #GoclEvent's are also the building blocks of synchronization
 * in OpenCL. The application developer will notice that most operations
 * include a @event_wait_list argument, which asks OpenCL to wait for
 * the completion of all #GoclEvent's in the list, before starting the
 * operation in question. This pattern greatly simplifies the application
 * logic and allows for complex algorithms to be splitted in smaller,
 * synchronized routines.
 *
 * A #GoclEvent is always associated with a #GoclQueue, which represents
 * the command queue where the operation represented by the event was
 * originally queued. The #GoclQueue can be retrieved using
 * gocl_event_get_queue().
 **/

/**
 * GoclEventClass:
 * @parent_class: The parent class
 *
 * The class for #GoclEvent objects.
 **/

/**
 * GoclEventCallback:
 * @self: The #GoclEvent
 * @error: A #GError if an error ocurred, %NULL otherwise
 * @user_data: The arbitrary pointer passed in gocl_event_then(),
 * or %NULL
 *
 * Prototype of the @callback argument of gocl_event_then().
 **/

/**
 * GoclEventResolverFunc:
 * @self: The #GoclEvent
 * @error: A #GError if an error ocurred, %NULL otherwise
 *
 * Prototype of the function that notifies the completion of an event.
 * This function is not normally called directly by applications. The object
 * that creates the event is responsible for stealing the resolver function
 * with gocl_event_steal_resolver_func(), and then resolve the event with
 * success or error by calling this method, when the related operation
 * completes.
 **/

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
  gint unref_src_id;
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
                                                         "The internal OpenCL cl_event object",
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

  priv->complete_src_id = 0;
  priv->unref_src_id = 0;
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

  g_object_unref (self);

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

static gboolean
unref_in_idle (gpointer user_data)
{
  GoclEvent *self = GOCL_EVENT (user_data);

  g_object_unref (self);

  return FALSE;
}

/* public */

/**
 * gocl_event_get_event:
 * @self: The #GoclEvent
 *
 * Retrieves the OpenCL internal #cl_event object. This is a rather
 * low-level method that should not normally be called by applications.
 *
 * Returns: (transfer none) (type guint64): The internal #cl_event object
 **/
cl_event
gocl_event_get_event (GoclEvent *self)
{
  g_return_val_if_fail (GOCL_IS_EVENT (self), NULL);

  return self->priv->event;
}

/**
 * gocl_event_get_queue:
 * @self: The #GoclEvent
 *
 * Retrieves the #GoclQueue object where the operation that created this
 * event was queued.
 *
 * Returns: (transfer none): The #GoclQueue associated with the event
 **/
GoclQueue *
gocl_event_get_queue (GoclEvent *self)
{
  g_return_val_if_fail (GOCL_IS_EVENT (self), NULL);

  return self->priv->queue;
}

/**
 * gocl_event_steal_resolver_func: (skip)
 * @self: The #GoclEvent
 *
 * Steals the internal resolver function from this event and sets it to %NULL,
 * so that only the first caller is able to resolve this event. This is a rather
 * low-level method that should not normally be called by applications.
 *
 * This method is intended to secure the #GoclEvent, to guarantee that the
 * creator of the event has exclusive control over the triggering of the
 * event.
 *
 * Returns: (transfer none): The internal resolver function
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
 * @self: The #GoclEvent
 * @callback: (scope async): A callback with a #GoclEventCallback signature
 * @user_data: (allow-none): Arbitrary data to pass in @callback, or %NULL
 *
 * Requests for a notification when the operation represented by this event
 * has finished. When this event triggers, @callback will be called
 * passing @user_data as argument, if provided.
 *
 * If the event already triggered when this method is called, the notification
 * is immediately scheduled as an idle call.
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

  g_object_ref (self);

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

/**
 * gocl_event_list_to_array:
 * @event_list: (element-type Gocl.Event) (allow-none): A #GList containing
 * #GoclEvent objects
 * @len: (out) (allow-none): A pointer to a value to retrieve list length
 *
 * A convenient method to retrieve a #GList of #GoclEvent's as an array of
 * #cl_event's corresponding to the internal objects of each #GoclEvent in
 * the #GList. This is a rather low-level method and should not normally be
 * called by applications.
 *
 * Returns: (transfer container) (array length=len): An array of #cl_event
 *   objects. Free with g_free().
 **/
cl_event *
gocl_event_list_to_array (GList *event_list, gsize *len)
{
  cl_event *event_arr = NULL;
  gint i;
  gsize _len;
  GList *node;

  _len = g_list_length (event_list);

  if (len != NULL)
    *len = _len;

  if (_len == 0)
    return event_arr;

  event_arr = g_new (cl_event, g_list_length (event_list));

  node = event_list;
  i = 0;
  while (node != NULL)
    {
      g_return_val_if_fail (GOCL_IS_EVENT (node->data), NULL);

      event_arr[i] = gocl_event_get_event (GOCL_EVENT (node->data));

      i++;
      node = g_list_next (node);
    }

  return event_arr;
}

/**
 * gocl_event_idle_unref:
 * @self: The #GoclEvent
 *
 * Schedules an object de-reference in an idle call.
 * This is a rather low-level method and should not normally be called by
 * applications.
 **/
void
gocl_event_idle_unref (GoclEvent *self)
{
  g_return_if_fail (GOCL_IS_EVENT (self));

  if (self->priv->unref_src_id != 0)
    return;

  self->priv->unref_src_id = timeout_add (NULL,
                                          0,
                                          G_PRIORITY_LOW,
                                          unref_in_idle,
                                          self);
}
