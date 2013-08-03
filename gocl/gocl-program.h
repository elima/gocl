/*
 * gocl-program.h
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

#ifndef __GOCL_PROGRAM_H__
#define __GOCL_PROGRAM_H__

#include <glib-object.h>
#include <gio/gio.h>
#include <CL/opencl.h>

#include "gocl-context.h"
#include "gocl-kernel.h"

G_BEGIN_DECLS

#define GOCL_TYPE_PROGRAM              (gocl_program_get_type ())
#define GOCL_PROGRAM(obj)              (G_TYPE_CHECK_INSTANCE_CAST ((obj), GOCL_TYPE_PROGRAM, GoclProgram))
#define GOCL_PROGRAM_CLASS(klass)      (G_TYPE_CHECK_CLASS_CAST ((klass), GOCL_TYPE_PROGRAM, GoclProgramClass))
#define GOCL_IS_PROGRAM(obj)           (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GOCL_TYPE_PROGRAM))
#define GOCL_IS_PROGRAM_CLASS(klass)   (G_TYPE_CHECK_CLASS_TYPE ((klass), GOCL_TYPE_PROGRAM))
#define GOCL_PROGRAM_GET_CLASS(obj)    (G_TYPE_INSTANCE_GET_CLASS ((obj), GOCL_TYPE_PROGRAM, GoclProgramClass))

typedef struct _GoclProgramClass GoclProgramClass;
typedef struct _GoclProgram GoclProgram;
typedef struct _GoclProgramPrivate GoclProgramPrivate;

struct _GoclProgram
{
  GObject parent_instance;

  GoclProgramPrivate *priv;
};

struct _GoclProgramClass
{
  GObjectClass parent_class;
};

GType                  gocl_program_get_type                   (void) G_GNUC_CONST;

GoclProgram *          gocl_program_new                        (GoclContext  *context,
                                                                const gchar **sources,
                                                                guint         num_sources);
GoclProgram *          gocl_program_new_from_file_sync         (GoclContext *context,
                                                                const gchar *filename);

cl_program             gocl_program_get_program                (GoclProgram *self);

GoclContext *          gocl_program_get_context                (GoclProgram *self);

GoclKernel *           gocl_program_get_kernel                 (GoclProgram *self,
                                                                const gchar *kernel_name);

gboolean               gocl_program_build_sync                 (GoclProgram *self,
                                                                const gchar *options);
void                   gocl_program_build                      (GoclProgram         *self,
                                                                const gchar         *options,
                                                                GCancellable        *cancellable,
                                                                GAsyncReadyCallback  callback,
                                                                gpointer             user_data);
gboolean               gocl_program_build_finish               (GoclProgram   *self,
                                                                GAsyncResult  *result,
                                                                GError       **error);

G_END_DECLS

#endif /* __GOCL_PROGRAM_H__ */
