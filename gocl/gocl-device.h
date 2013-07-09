/*
 * gocl-device.h
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

#ifndef __GOCL_DEVICE_H__
#define __GOCL_DEVICE_H__

#include <glib-object.h>
#include <CL/opencl.h>

#include <gocl-buffer.h>
#include <gocl-queue.h>

G_BEGIN_DECLS

#define GOCL_TYPE_DEVICE              (gocl_device_get_type ())
#define GOCL_DEVICE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOCL_TYPE_DEVICE, GoclDevice))
#define GOCL_DEVICE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GOCL_TYPE_DEVICE, GoclDeviceClass))
#define GOCL_IS_DEVICE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOCL_TYPE_DEVICE))
#define GOCL_IS_DEVICE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GOCL_TYPE_DEVICE))
#define GOCL_DEVICE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GOCL_TYPE_DEVICE, GoclDeviceClass))

typedef struct _GoclDeviceClass GoclDeviceClass;
typedef struct _GoclDevice GoclDevice;
typedef struct _GoclDevicePrivate GoclDevicePrivate;

struct _GoclDevice
{
  GObject parent_instance;

  GoclDevicePrivate *priv;
};

struct _GoclDeviceClass
{
  GObjectClass parent_class;
};

GType                  gocl_device_get_type                   (void) G_GNUC_CONST;

cl_device_id           gocl_device_get_id                     (GoclDevice *self);

gsize                  gocl_device_get_max_work_group_size    (GoclDevice  *self,
                                                               GError     **error);

GoclQueue *            gocl_device_get_default_queue          (GoclDevice  *self,
                                                               GError     **error);

gboolean               gocl_device_has_extension              (GoclDevice   *self,
                                                               const gchar  *extension_name);

guint                  gocl_device_get_max_compute_units      (GoclDevice *self);

/* GoclQueue headers */
GoclDevice *           gocl_queue_get_device                  (GoclQueue *self);

G_END_DECLS

#endif /* __GOCL_DEVICE_H__ */
