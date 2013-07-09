/*
 * gocl-kernel.h
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

#ifndef __GOCL_KERNEL_H__
#define __GOCL_KERNEL_H__

#include <glib-object.h>

#include <gocl-device.h>
#include <gocl-event.h>

G_BEGIN_DECLS

#define GOCL_TYPE_KERNEL              (gocl_kernel_get_type ())
#define GOCL_KERNEL(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOCL_TYPE_KERNEL, GoclKernel))
#define GOCL_KERNEL_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GOCL_TYPE_KERNEL, GoclKernelClass))
#define GOCL_IS_KERNEL(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOCL_TYPE_KERNEL))
#define GOCL_IS_KERNEL_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GOCL_TYPE_KERNEL))
#define GOCL_KERNEL_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GOCL_TYPE_KERNEL, GoclKernelClass))

typedef struct _GoclKernelClass GoclKernelClass;
typedef struct _GoclKernel GoclKernel;
typedef struct _GoclKernelPrivate GoclKernelPrivate;

struct _GoclKernel
{
  GObject parent_instance;

  GoclKernelPrivate *priv;
};

struct _GoclKernelClass
{
  GObjectClass parent_class;
};

GType                  gocl_kernel_get_type                   (void) G_GNUC_CONST;

cl_kernel              gocl_kernel_get_kernel                 (GoclKernel *self);

gboolean               gocl_kernel_set_argument               (GoclKernel      *self,
                                                               guint            index,
                                                               gsize            size,
                                                               const gpointer  *buffer,
                                                               GError         **error);
gboolean               gocl_kernel_set_argument_int32         (GoclKernel  *self,
                                                               guint        index,
                                                               gsize        num_elements,
                                                               gint32      *buffer,
                                                               GError     **error);
gboolean               gocl_kernel_set_argument_buffer        (GoclKernel  *self,
                                                               guint        index,
                                                               GoclBuffer  *buffer,
                                                               GError     **error);

gboolean               gocl_kernel_run_in_device_sync         (GoclKernel  *self,
                                                               GoclDevice  *device,
                                                               gsize        global_work_size,
                                                               gsize        local_work_size,
                                                               GList       *event_wait_list,
                                                               GError     **error);
GoclEvent *            gocl_kernel_run_in_device              (GoclKernel  *self,
                                                               GoclDevice  *device,
                                                               gsize        global_work_size,
                                                               gsize        local_work_size,
                                                               GList       *event_wait_list);

void                   gocl_kernel_set_work_dimension         (GoclKernel *self,
                                                               guint8      work_dim);

G_END_DECLS

#endif /* __GOCL_KERNEL_H__ */
