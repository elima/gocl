/*
 * gocl-image.c
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

/**
 * SECTION:gocl-image
 * @short_description: Object that represents an image buffer in an OpenCL
 * context
 * @stability: Unstable
 *
 * A #GoclImage represents a image object in an OpenCL context. An image is a
 * special type of buffer that holds pixel information in a convenient way.
 * As buffers, images are also directly accessible from OpenCL programs.
 *
 * Iamges are created using gocl_image_new() and
 * gocl_image_new_from_gl_texture().
 *
 * Reading from and writing to images is done using the provided
 * #GoclBuffer APIs.
 **/

/**
 * GoclImageClass:
 * @parent_class: The parent class
 *
 * The class for #GoclImage objects.
 **/

#include <string.h>
#include <gio/gio.h>

#include "gocl-image.h"

#include "gocl-error-private.h"
#include "gocl-context.h"

struct _GoclImagePrivate
{
  cl_image_desc props;

  guint gl_texture;
};

/* properties */
enum
{
  PROP_0,
  PROP_TYPE,
  PROP_WIDTH,
  PROP_HEIGHT,
  PROP_DEPTH,
  PROP_GL_TEXTURE
};

static void           gocl_image_class_init               (GoclImageClass *class);
static void           gocl_image_init                     (GoclImage *self);

static void           set_property                        (GObject      *obj,
                                                           guint         prop_id,
                                                           const GValue *value,
                                                           GParamSpec   *pspec);
static void           get_property                        (GObject    *obj,
                                                           guint       prop_id,
                                                           GValue     *value,
                                                           GParamSpec *pspec);

static cl_int         create_cl_mem                       (GoclBuffer  *buffer,
                                                           cl_context   context,
                                                           cl_mem      *obj,
                                                           guint        flags,
                                                           gsize        size,
                                                           gpointer     host_ptr);
static cl_int         read_all                            (GoclBuffer          *buffer,
                                                           cl_mem               image,
                                                           cl_command_queue     queue,
                                                           gpointer             target_ptr,
                                                           gsize               *size,
                                                           gboolean             blocking,
                                                           cl_event            *event_wait_list,
                                                           guint                event_wait_list_len,
                                                           cl_event            *out_event);

G_DEFINE_TYPE (GoclImage, gocl_image, GOCL_TYPE_BUFFER)

#define GOCL_IMAGE_GET_PRIVATE(obj)                       \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                    \
                                GOCL_TYPE_IMAGE,          \
                                GoclImagePrivate))        \

static void
gocl_image_class_init (GoclImageClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);
  GoclBufferClass *gocl_buf_class = GOCL_BUFFER_CLASS (class);

  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  gocl_buf_class->create_cl_mem = create_cl_mem;
  gocl_buf_class->read_all = read_all;

  g_object_class_install_property (obj_class, PROP_TYPE,
                                   g_param_spec_uint ("type",
                                                      "Image type",
                                                      "The type of the image (1D, 2D, 3D, etc)",
                                                      0,
                                                      G_MAXUINT,
                                                      0,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (obj_class, PROP_WIDTH,
                                   g_param_spec_uint64 ("width",
                                                        "Width",
                                                        "The width of the image, in pixels",
                                                        0,
                                                        G_MAXUINT64,
                                                        0,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (obj_class, PROP_HEIGHT,
                                   g_param_spec_uint64 ("height",
                                                        "Height",
                                                        "The height of the image, in pixels",
                                                        0,
                                                        G_MAXUINT64,
                                                        0,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));
  g_object_class_install_property (obj_class, PROP_DEPTH,
                                   g_param_spec_uint64 ("depth",
                                                        "Depth",
                                                        "The depth of the image, in pixels",
                                                        0,
                                                        G_MAXUINT64,
                                                        0,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (obj_class, PROP_GL_TEXTURE,
                                   g_param_spec_uint ("gl-texture",
                                                      "GL Texture",
                                                      "The GL texture associated with this image",
                                                      0,
                                                      G_MAXUINT,
                                                      0,
                                                      G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                      G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GoclImagePrivate));
}

static void
gocl_image_init (GoclImage *self)
{
  GoclImagePrivate *priv;

  self->priv = priv = GOCL_IMAGE_GET_PRIVATE (self);

  memset (&priv->props, 0, sizeof (cl_image_desc));
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GoclImage *self;

  self = GOCL_IMAGE (obj);

  switch (prop_id)
    {
    case PROP_TYPE:
      self->priv->props.image_type = g_value_get_uint (value);
      break;

    case PROP_WIDTH:
      self->priv->props.image_width = g_value_get_uint64 (value);
      break;

    case PROP_HEIGHT:
      self->priv->props.image_height = g_value_get_uint64 (value);
      break;

    case PROP_DEPTH:
      self->priv->props.image_depth = g_value_get_uint64 (value);
      break;

    case PROP_GL_TEXTURE:
      self->priv->gl_texture = g_value_get_uint (value);
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
  GoclImage *self;

  self = GOCL_IMAGE (obj);

  switch (prop_id)
    {
    case PROP_TYPE:
      g_value_set_uint (value, self->priv->props.image_type);
      break;

    case PROP_WIDTH:
      g_value_set_uint64 (value, self->priv->props.image_width);
      break;

    case PROP_HEIGHT:
      g_value_set_uint64 (value, self->priv->props.image_height);
      break;

    case PROP_DEPTH:
      g_value_set_uint64 (value, self->priv->props.image_depth);
      break;

    case PROP_GL_TEXTURE:
      g_value_set_uint (value, self->priv->gl_texture);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
get_image_info (GoclImage *self, cl_mem obj, cl_image_info param)
{
  gsize value;
  cl_int err_code = 0;
  GError *error = NULL;

  err_code = clGetImageInfo (obj,
                             param,
                             sizeof (gsize),
                             &value,
                             NULL);
  if (gocl_error_check_opencl (err_code, &error))
    {
      g_warning ("Failed to obtain OpenCL image property: %s\n", error->message);
      g_error_free (error);
      return;
    }

  switch (param)
    {
    case CL_IMAGE_WIDTH:
      self->priv->props.image_width = value;
      break;

    case CL_IMAGE_HEIGHT:
      self->priv->props.image_height = value;
      break;

    case CL_IMAGE_DEPTH:
      self->priv->props.image_depth = value;
      break;

    default:
      break;
    }
}

static cl_int
create_cl_mem (GoclBuffer  *buffer,
               cl_context   context,
               cl_mem      *obj,
               guint        flags,
               gsize        size,
               gpointer     host_ptr)
{
  GoclImage *self = GOCL_IMAGE (buffer);
  cl_int err_code = 0;

  if (self->priv->gl_texture > 0)
    {
      *obj = clCreateFromGLTexture (context,
                                    flags,  /* OR'ed from GoclBufferFlags */
                                    0x0DE1, /* GL_TEXTURE_2D from gl.h */
                                    0,      /* miplevel = 0 for now */
                                    self->priv->gl_texture,
                                    &err_code);

      if (err_code == CL_SUCCESS)
        {
          /* the only we currently support from GL */
          self->priv->props.image_type = GOCL_IMAGE_TYPE_2D;

          get_image_info (self, *obj, CL_IMAGE_WIDTH);
          get_image_info (self, *obj, CL_IMAGE_HEIGHT);
          get_image_info (self, *obj, CL_IMAGE_DEPTH);
        }
    }
  else
    {
      /* by now, only RGBA as 8-bit unsigned integers is supported */
      cl_image_format format = { CL_RGBA, CL_UNORM_INT8 };

      *obj = clCreateImage (context,
                            flags,
                            &format,
                            &self->priv->props,
                            host_ptr,
                            &err_code);
    }

  return err_code;
}

static cl_int
read_all (GoclBuffer          *buffer,
          cl_mem               image,
          cl_command_queue     queue,
          gpointer             target_ptr,
          gsize               *size,
          gboolean             blocking,
          cl_event            *event_wait_list,
          guint                event_wait_list_len,
          cl_event            *out_event)
{
  GoclImage *self = GOCL_IMAGE (buffer);

  gsize origin[3] = {0, };
  gsize region[3];

  region[0] = self->priv->props.image_width;
  region[1] = self->priv->props.image_height;
  region[2] = self->priv->props.image_type == GOCL_IMAGE_TYPE_2D ?
    1 : self->priv->props.image_depth;

  if (size != NULL)
    *size = region[0] * region[1] * region[2] * 4 /* assuming RGBA */;

  return clEnqueueReadImage (queue,
                             image,
                             blocking,
                             origin,
                             region,
                             0,
                             0,
                             target_ptr,
                             event_wait_list_len,
                             event_wait_list,
                             out_event);
}

/* public */

/**
 * gocl_image_new:
 * @context: The #GoclContext to create the image in
 * @flags: An OR'ed combination of values from #GoclBufferFlags
 * @type: Image type value from #GoclImageType
 * @width: Image width in pixels
 * @height: Image height in pixels, zero if image type is 1D
 * @depth: Image depth in pixels, or zero if image is not 3D
 *
 * Creates a new image buffer. Currently only 8-bits unsigned integer format is
 * supported, with RGBA channels.
 * Other image properties like row pitch, slice pitch, etc. are assumed to be
 * zero by now.
 *
 * Returns: (transfer full): A newly created #GoclImage, or %NULL on error
 **/
GoclImage *
gocl_image_new (GoclContext   *context,
                guint          flags,
                gpointer       host_ptr,
                GoclImageType  type,
                gsize          width,
                gsize          height,
                gsize          depth)
{
  GError **error;

  g_return_val_if_fail (GOCL_IS_CONTEXT (context), NULL);

  error = gocl_error_prepare ();

  return g_initable_new (GOCL_TYPE_IMAGE,
                         NULL,
                         error,
                         "context", context,
                         "flags", flags,
                         "host-ptr", host_ptr,
                         "type", type,
                         "width", width,
                         "height", height,
                         "depth", depth,
                         NULL);
}

/**
 * gocl_image_new_from_gl_texture:
 * @context: The #GoclContext to create the image in
 * @flags: An OR'ed combination of values from #GoclBufferFlags
 * @texture: The GL texture handle
 *
 * Creates a new image buffer from a GL texture handle. This only works if the
 * OpenCL platform supports the <i>cl_khr_gl_sharing</i> extension, and the
 * @context has been created for sharing with OpenGL, using
 * gocl_context_gpu_new_sync().
 *
 * Returns: (transfer full): A newly created #GoclImage, or %NULL on error
 **/
GoclImage *
gocl_image_new_from_gl_texture (GoclContext *context,
                                guint        flags,
                                guint        texture)
{
  GError **error;

  g_return_val_if_fail (GOCL_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (texture > 0, NULL);

  error = gocl_error_prepare ();

  return g_initable_new (GOCL_TYPE_IMAGE,
                         NULL,
                         error,
                         "context", context,
                         "flags", flags,
                         "gl-texture", texture,
                         NULL);
}
