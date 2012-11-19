/*
 * gocl-queue.c
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

#include <gio/gio.h>

#include "gocl-queue.h"

#include "gocl-error.h"
#include "gocl-decls.h"
#include "gocl-context.h"
#include "gocl-device.h"

struct _GoclQueuePrivate
{
  cl_command_queue queue;

  GoclContext *context;
  GoclDevice *device;
};

/* properties */
enum
{
  PROP_0,
  PROP_CONTEXT,
  PROP_DEVICE
};

static void           gocl_queue_class_init            (GoclQueueClass *class);
static void           gocl_queue_initable_iface_init   (GInitableIface *iface);
static gboolean       gocl_queue_initable_init         (GInitable     *initable,
                                                        GCancellable  *cancellable,
                                                        GError       **error);
static void           gocl_queue_init                  (GoclQueue *self);
static void           gocl_queue_dispose               (GObject *obj);
static void           gocl_queue_finalize              (GObject *obj);

static void           set_property                     (GObject      *obj,
                                                        guint         prop_id,
                                                        const GValue *value,
                                                        GParamSpec   *pspec);
static void           get_property                     (GObject    *obj,
                                                        guint       prop_id,
                                                        GValue     *value,
                                                        GParamSpec *pspec);

G_DEFINE_TYPE_WITH_CODE (GoclQueue, gocl_queue, G_TYPE_OBJECT,
                         G_IMPLEMENT_INTERFACE (G_TYPE_INITABLE,
                                                gocl_queue_initable_iface_init));

#define GOCL_QUEUE_GET_PRIVATE(obj)  \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj), \
                                GOCL_TYPE_QUEUE, \
                                GoclQueuePrivate)) \

static void
gocl_queue_class_init (GoclQueueClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->dispose = gocl_queue_dispose;
  obj_class->finalize = gocl_queue_finalize;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  g_object_class_install_property (obj_class, PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Queue context",
                                                        "The context this queue belongs to",
                                                        GOCL_TYPE_CONTEXT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_DEVICE,
                                   g_param_spec_object ("device",
                                                        "Queue device",
                                                        "The device this queue belongs to",
                                                        GOCL_TYPE_DEVICE,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GoclQueuePrivate));
}

static void
gocl_queue_initable_iface_init (GInitableIface *iface)
{
  iface->init = gocl_queue_initable_init;
}

static gboolean
gocl_queue_initable_init (GInitable     *initable,
                          GCancellable  *cancellable,
                          GError       **error)
{
  GoclQueue *self = GOCL_QUEUE (initable);
  cl_int err_code = 0;
  cl_context ctx;
  cl_device_id device_id;

  ctx = gocl_context_get_context (self->priv->context);
  device_id = (cl_device_id) gocl_device_get_id (self->priv->device);

  self->priv->queue = clCreateCommandQueue (ctx,
                                            device_id,
                                            0,
                                            &err_code);

  if (gocl_error_check_opencl (err_code, error))
    return FALSE;
  else
    return TRUE;
}

static void
gocl_queue_init (GoclQueue *self)
{
  GoclQueuePrivate *priv;

  self->priv = priv = GOCL_QUEUE_GET_PRIVATE (self);

  priv->queue = NULL;
}

static void
gocl_queue_dispose (GObject *obj)
{
  GoclQueue *self = GOCL_QUEUE (obj);

  if (self->priv->context != NULL)
    {
      g_object_unref (self->priv->context);
      self->priv->context = NULL;
    }

  if (self->priv->device != NULL)
    {
      g_object_unref (self->priv->device);
      self->priv->device = NULL;
    }

  G_OBJECT_CLASS (gocl_queue_parent_class)->dispose (obj);
}

static void
gocl_queue_finalize (GObject *obj)
{
  GoclQueue *self = GOCL_QUEUE (obj);

  if (self->priv->queue != NULL)
    clReleaseCommandQueue (self->priv->queue);

  G_OBJECT_CLASS (gocl_queue_parent_class)->finalize (obj);
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GoclQueue *self;

  self = GOCL_QUEUE (obj);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->priv->context = g_value_dup_object (value);
      break;

    case PROP_DEVICE:
      self->priv->device = g_value_dup_object (value);
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
  GoclQueue *self;

  self = GOCL_QUEUE (obj);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->priv->context);
      break;

    case PROP_DEVICE:
      g_value_set_object (value, self->priv->device);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

/* public */

/**
 * gocl_queue_get_queue:
 *
 * Returns: (transfer none):
 **/
cl_command_queue
gocl_queue_get_queue (GoclQueue *self)
{
  g_return_val_if_fail (GOCL_IS_QUEUE (self), NULL);

  return self->priv->queue;
}
