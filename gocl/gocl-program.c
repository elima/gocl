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

/**
 * SECTION:gocl-program
 * @short_description: Object that represents an OpenCL program
 * @stability: Unstable
 *
 * A #GoclProgram allows to transform OpenCL source code into kernel objects
 * that can run on OpenCL runtimes.
 *
 * A #GoclProgram is created with gocl_program_new(), provinding a
 * null-terminated array of strings, each one representing OpenCL source code.
 * Alternative;y, gocl_program_new_from_file_sync() can be used for simple cases
 * when source code is in a single file.
 *
 * Currently, creating a program from pre-compiled, binary code is not supported,
 * but will be in the future.
 *
 * Once a program is created, it needs to be built before kernels can be created
 * from it. To build a program asynchronously, gocl_program_build() and
 * gocl_program_build_finish() methods are provided. For building synchronously,
 * gocl_program_build_sync() is used.
 *
 * Once a program is successfully built, kernels can be obtained from it using
 * gocl_program_get_kernel() method.
 **/

/**
 * GoclProgramClass:
 * @parent_class: The parent class
 *
 * The class for #GoclProgram objects.
 **/

#include <string.h>

#include "gocl-program.h"

#include "gocl-error.h"
#include "gocl-error-private.h"

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
  /* GoclProgram *self = GOCL_PROGRAM (obj); */
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
  gchar *options;

  options = g_simple_async_result_get_op_res_gpointer (res);

  if (! gocl_program_build_sync (self, options))
    {
      GError *error;

      error = gocl_error_get_last ();
      g_simple_async_result_take_error (res, error);
    }

  g_object_unref (res);

  self->priv->building = FALSE;
}

/* public */

/**
 * gocl_program_new:
 * @context: The #GoclContext
 * @sources: (array length=num_sources) (type utf8): Array of source code
 * null-terminated strings
 * @num_sources: The number of elements in @sources
 *
 * Creates and returns a new #GoclProgram. Upon error, %NULL is returned.
 *
 * Returns: (transfer full): A newly created #GoclProgram
 **/
GoclProgram *
gocl_program_new (GoclContext  *context,
                  const gchar **sources,
                  guint         num_sources)
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

  self->priv->program =
    clCreateProgramWithSource (gocl_context_get_context (context),
                               num_sources,
                               sources,
                               NULL,
                               &err_code);
  if (gocl_error_check_opencl_internal (err_code))
    return NULL;

  return self;
}

/**
 * gocl_program_new_from_file_sync:
 * @context: The #GoclContext
 * @filename: The source code file
 *
 * Creates and returns a new #GoclProgram. This is a convenient constructor for
 * cases when there is only one file containing the source code. Upon error,
 * %NULL is returned.
 *
 * Returns: (transfer full): A newly created #GoclProgram, or %NULL on error
 **/
GoclProgram *
gocl_program_new_from_file_sync (GoclContext *context, const gchar *filename)
{
  gchar *source;
  GoclProgram *self;
  GError **error;

  g_return_val_if_fail (filename != NULL, NULL);

  error = gocl_error_prepare ();

  if (! g_file_get_contents (filename,
                             &source,
                             NULL,
                             error))
    {
      return NULL;
    }

  self = gocl_program_new (context, (const gchar **) &source, 1);
  g_free (source);

  return self;
}

/**
 * gocl_program_get_program:
 * @self: The #GoclProgram
 *
 * Retrieves the OpenCL internal #cl_program object. This is not normally called
 * by applications. It is rather a low-level, internal API.
 *
 * Returns: (transfer none) (type guint64): The internal #cl_program
 **/
cl_program
gocl_program_get_program (GoclProgram *self)
{
  g_return_val_if_fail (GOCL_IS_PROGRAM (self), NULL);

  return self->priv->program;
}

/**
 * gocl_program_get_context:
 * @self: The #GoclProgram
 *
 * Obtain the #GoclContext the program belongs to.
 *
 * Returns: (transfer none): A #GoclContext. The returned object is owned by
 *   the program, do not free.
 **/
GoclContext *
gocl_program_get_context (GoclProgram *self)
{
  g_return_val_if_fail (GOCL_IS_PROGRAM (self), NULL);

  return self->priv->context;
}

/**
 * gocl_program_get_kernel:
 * @self: The #GoclProgram
 * @kernel_name: A string representing the name of a kernel function
 *
 * Creates and retrieves a new #GoclKernel object from a kernel function
 * in the source code, specified by @kernel_name string. Upon success,
 * a new #GoclKernel is returned, otherwise %NULL is returned.
 *
 * Returns: (transfer full): A newly created #GoclKernel object
 **/
GoclKernel *
gocl_program_get_kernel (GoclProgram *self, const gchar *kernel_name)
{
  GError **error;

  g_return_val_if_fail (GOCL_IS_PROGRAM (self), NULL);
  g_return_val_if_fail (kernel_name != NULL, NULL);

  error = gocl_error_prepare ();

  return GOCL_KERNEL (g_initable_new (GOCL_TYPE_KERNEL,
                                      NULL,
                                      error,
                                      "program", self,
                                      "name", kernel_name,
                                      NULL));
}

/**
 * gocl_program_build_sync:
 * @self: The #GoclProgram
 * @options: A string specifying OpenCL program build options
 *
 * Builds the program using the build options specified in
 * @options. This method is blocking. For an asynchronous version,
 * gocl_program_build() is provided. On error, %FALSE is returned.
 *
 * A detailed description of the build options is available at Kronos
 * documentation website:
 * http://www.khronos.org/registry/cl/sdk/1.0/docs/man/xhtml/clBuildProgram.html
 *
 * Returns: %TRUE on success or %FALSE on error
 **/
gboolean
gocl_program_build_sync (GoclProgram *self, const gchar *options)
{
  cl_int err_code;

  g_return_val_if_fail (GOCL_IS_PROGRAM (self), FALSE);

  err_code = clBuildProgram (self->priv->program,
                             0,
                             NULL,
                             options,
                             NULL,
                             NULL);

  return ! gocl_error_check_opencl_internal (err_code);
}

/**
 * gocl_program_build:
 * @self: The #GoclProgram
 * @options: A string specifying OpenCL program build options
 * @cancellable: (allow-none): A #GCancellable object, or %NULL
 * @callback: (allow-none): Callback to be called upon completion, or %NULL
 * @user_data: (allow-none): Arbitrary data to pass in @callback, or %NULL
 *
 * Builds the program using the build options specified in
 * @options. This method is non-blocking. If @callback is provided, it will
 * be called when the operation completes, and gocl_program_build_finish()
 * can be used within the callback to retrieve the result of the operation.
 *
 * A #GCancellable object can be passed in @cancellable to allow cancelling
 * the operation.
 **/
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

/**
 * gocl_program_build_finish:
 * @self: The #GoclProgram
 * @result: The #GAsyncResult object from callback's arguments
 * @error: (out) (allow-none): A pointer to a #GError, or %NULL
 *
 * Retrieves the result of a gocl_program_build() asynchronous operation.
 * On error, %FALSE is returned and @error is filled accordingly.
 *
 * Returns: %TRUE on success, or %FALSE on error
 **/
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
