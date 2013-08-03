/*
 * gocl-error-private.h
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

#ifndef __GOCL_ERROR_PRIVATE_H__
#define __GOCL_ERROR_PRIVATE_H__

#include <glib.h>
#include <CL/opencl.h>

G_BEGIN_DECLS

gboolean              gocl_error_check_opencl              (cl_int    err_code,
                                                            GError  **error);

GError **             gocl_error_prepare                   (void);
void                  gocl_error_free                      (void);

G_END_DECLS

#endif /* __GOCL_ERROR_PRIVATE_H__ */
