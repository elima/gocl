/*
 * gocl-program.c
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

#include <string.h>

#include "gocl-program.h"

#include "gocl-error.h"

struct _GoclProgramPrivate
{
  cl_program program;

  GoclContext *context;

  gboolean building;
};

/* properties */
enum
{
  PROP_0,
  PROP_CONTEXT
};

static void           gocl_program_class_init            (GoclProgramClass *class);
static void           gocl_program_init                  (GoclProgram *self);
static void           gocl_program_constructed           (GObject *obj);

static void           gocl_program_finalize              (GObject *obj);

static void           set_property                       (GObject      *obj,
                                                          guint         prop_id,
                                                          const GValue *value,
                                                          GParamSpec   *pspec);
static void           get_property                       (GObject    *obj,
                                                          guint       prop_id,
                                                          GValue     *value,
                                                          GParamSpec *pspec);

G_DEFINE_TYPE (GoclProgram, gocl_program, G_TYPE_OBJECT);

#define GOCL_PROGRAM_GET_PRIVATE(obj)                   \
  (G_TYPE_INSTANCE_GET_PRIVATE ((obj),                  \
                                GOCL_TYPE_PROGRAM,      \
                                GoclProgramPrivate))    \

static void
gocl_program_class_init (GoclProgramClass *class)
{
  GObjectClass *obj_class = G_OBJECT_CLASS (class);

  obj_class->finalize = gocl_program_finalize;
  obj_class->constructed = gocl_program_constructed;
  obj_class->get_property = get_property;
  obj_class->set_property = set_property;

  g_object_class_install_property (obj_class, PROP_CONTEXT,
                                   g_param_spec_object ("context",
                                                        "Context",
                                                        "The OpenCL context",
                                                        GOCL_TYPE_CONTEXT,
                                                        G_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY |
                                                        G_PARAM_STATIC_STRINGS));

  g_type_class_add_private (class, sizeof (GoclProgramPrivate));
}

static void
gocl_program_init (GoclProgram *self)
{
  GoclProgramPrivate *priv;

  self->priv = priv = GOCL_PROGRAM_GET_PRIVATE (self);

  priv->building = FALSE;
}

static void
gocl_program_constructed (GObject *obj)
{
  //GoclProgram *self = GOCL_PROGRAM (obj);
}

static void
gocl_program_finalize (GObject *obj)
{
  GoclProgram *self = GOCL_PROGRAM (obj);

  g_object_unref (self->priv->context);

  clReleaseProgram (self->priv->program);

  G_OBJECT_CLASS (gocl_program_parent_class)->finalize (obj);
}

static void
set_property (GObject      *obj,
              guint         prop_id,
              const GValue *value,
              GParamSpec   *pspec)
{
  GoclProgram *self;

  self = GOCL_PROGRAM (obj);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      self->priv->context = g_value_dup_object (value);
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
  GoclProgram *self;

  self = GOCL_PROGRAM (obj);

  switch (prop_id)
    {
    case PROP_CONTEXT:
      g_value_set_object (value, self->priv->context);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (obj, prop_id, pspec);
      break;
    }
}

static void
build_in_thread (GSimpleAsyncResult *res,
                 GObject            *object,
                 GCancellable       *cancellable)
{
  GoclProgram *self = GOCL_PROGRAM (object);
  GError *error = NULL;
  gchar *options;

  options = g_simple_async_result_get_op_res_gpointer (res);

  if (! gocl_program_build_sync (self,
                                 options,
                                 &error))
    {
      g_simple_async_result_take_error (res, error);
    }

  g_object_unref (res);

  self->priv->building = FALSE;
}

/* public */

/**
 * gocl_program_new:
 * @context: The #GoclContext
 * @sources: (array length=num_sources) (type utf8): Array of source code null
 * terminated strings
 * @num_sources: The number of elements in @sources
 * @error: (out) (allow-none): A #GError pointer
 *
 * Creates a new #GoclProgram.
 *
 * Returns: (transfer full):
 **/
GoclProgram *
gocl_program_new (GoclContext  *context,
                  const gchar **sources,
                  guint         num_sources,
                  GError      **error)
{
  GoclProgram *self;
  cl_int err_code;

  g_return_val_if_fail (GOCL_IS_CONTEXT (context), NULL);
  g_return_val_if_fail (sources != NULL, NULL);

  self = g_object_new (GOCL_TYPE_PROGRAM,
                       "context", context,
                       NULL);

  if (num_sources < 1)
    num_sources = g_strv_length ((gchar **) sources);

  self->priv->program = clCreateProgramWithSource (gocl_context_get_context (context),
                                                   num_sources,
                                                   sources,
                                                   NULL,
                                                   &err_code);
  if (gocl_error_check_opencl (err_code, error))
    return NULL;

  return self;
}

/**
 * gocl_program_get_program:
 *
 * Returns: (transfer none) (type guint64):
 **/
cl_program
gocl_program_get_program (GoclProgram *self)
{
  g_return_val_if_fail (GOCL_IS_PROGRAM (self), NULL);

  return self->priv->program;
}

/**
 * gocl_program_get_context:
 *
 * Returns: (transfer none):
 **/
GoclContext *
gocl_program_get_context (GoclProgram *self)
{
  g_return_val_if_fail (GOCL_IS_PROGRAM (self), NULL);

  return self->priv->context;
}

/**
 * gocl_program_get_kernel:
 *
 * Returns: (transfer full):
 **/
GoclKernel *
gocl_program_get_kernel (GoclProgram  *self,
                         const gchar  *kernel_name,
                         GError      **error)
{
  g_return_val_if_fail (GOCL_IS_PROGRAM (self), NULL);
  g_return_val_if_fail (kernel_name != NULL, NULL);

  return GOCL_KERNEL (g_initable_new (GOCL_TYPE_KERNEL,
                                      NULL,
                                      error,
                                      "program", self,
                                      "name", kernel_name,
                                      NULL));
}

gboolean
gocl_program_build_sync (GoclProgram  *self,
                         const gchar  *options,
                         GError      **error)
{
  cl_int err_code;

  g_return_val_if_fail (GOCL_IS_PROGRAM (self), FALSE);

  err_code = clBuildProgram (self->priv->program,
                             0,
                             NULL,
                             options,
                             NULL,
                             NULL);

  return ! gocl_error_check_opencl (err_code, error);
}

void
gocl_program_build (GoclProgram         *self,
                    const gchar         *options,
                    GCancellable        *cancellable,
                    GAsyncReadyCallback  callback,
                    gpointer             user_data)
{
  GSimpleAsyncResult *res;

  g_return_if_fail (GOCL_IS_PROGRAM (self));

  res = g_simple_async_result_new (G_OBJECT (self),
                                   callback,
                                   user_data,
                                   gocl_program_build);

  if (self->priv->building)
    {
      g_simple_async_result_set_error (res,
                                       G_IO_ERROR,
                                       G_IO_ERROR_PENDING,
                                       "A previous build operation is pending");
      g_simple_async_result_complete_in_idle (res);
      g_object_unref (res);
    }
  else
    {
      self->priv->building = TRUE;

      if (options == NULL)
        options = "";

      g_simple_async_result_set_op_res_gpointer (res,
                                                 g_strdup (options),
                                                 g_free);

      g_simple_async_result_run_in_thread (res,
                                           build_in_thread,
                                           G_PRIORITY_DEFAULT,
                                           cancellable);
    }
}

gboolean
gocl_program_build_finish (GoclProgram   *self,
                           GAsyncResult  *result,
                           GError       **error)
{
  g_return_val_if_fail (GOCL_IS_PROGRAM (self), FALSE);
  g_return_val_if_fail (g_simple_async_result_is_valid (result,
                                                        G_OBJECT (self),
                                                        gocl_program_build),
                        FALSE);

  return
    ! g_simple_async_result_propagate_error (G_SIMPLE_ASYNC_RESULT (result),
                                             error);
}
