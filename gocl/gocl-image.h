/*
 * gocl-image.h
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

#ifndef __GOCL_IMAGE_H__
#define __GOCL_IMAGE_H__

#include <glib-object.h>
#include <CL/opencl.h>

#include "gocl-decls.h"
#include "gocl-buffer.h"

G_BEGIN_DECLS

#define GOCL_TYPE_IMAGE              (gocl_image_get_type ())
#define GOCL_IMAGE(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOCL_TYPE_IMAGE, GoclImage))
#define GOCL_IMAGE_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GOCL_TYPE_IMAGE, GoclImageClass))
#define GOCL_IS_IMAGE(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOCL_TYPE_IMAGE))
#define GOCL_IS_IMAGE_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GOCL_TYPE_IMAGE))
#define GOCL_IMAGE_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GOCL_TYPE_IMAGE, GoclImageClass))

typedef struct _GoclImageClass GoclImageClass;
typedef struct _GoclImage GoclImage;
typedef struct _GoclImagePrivate GoclImagePrivate;

struct _GoclImage
{
  GoclBuffer parent_instance;

  GoclImagePrivate *priv;
};

struct _GoclImageClass
{
  GoclBufferClass parent_class;
};

GType                  gocl_image_get_type                   (void) G_GNUC_CONST;

G_END_DECLS

#endif /* __GOCL_IMAGE_H__ */
