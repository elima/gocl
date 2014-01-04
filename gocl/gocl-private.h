/*
 * gocl-private.h
 *
 * Gocl - GLib/GObject wrapper for OpenCL
 * Copyright (C) 2014 Igalia S.L.
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

#ifndef __GOCL_PRIVATE_H__
#define __GOCL_PRIVATE_H__

#include <glib.h>
#include <CL/opencl.h>

#include "gocl-context.h"
#include "gocl-program.h"
#include "gocl-kernel.h"
#include "gocl-buffer.h"
#include "gocl-queue.h"
#include "gocl-event.h"

G_BEGIN_DECLS

cl_context        gocl_context_get_context         (GoclContext *self);

cl_program        gocl_program_get_program         (GoclProgram *self);

cl_kernel         gocl_kernel_get_kernel           (GoclKernel *self);

cl_mem            gocl_buffer_get_buffer           (GoclBuffer *self);

cl_command_queue  gocl_queue_get_queue             (GoclQueue *self);

cl_event          gocl_event_get_event             (GoclEvent *self);


gboolean          gocl_error_check_opencl          (cl_int   err_code,
                                                    GError **error);
gboolean          gocl_error_check_opencl_internal (cl_int err_code);

GError **         gocl_error_prepare               (void);
void              gocl_error_free                  (void);

G_END_DECLS

#endif /* __GOCL_PRIVATE_H__ */
