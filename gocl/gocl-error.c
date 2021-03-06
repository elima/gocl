/*
 * gocl-error.c
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
 * SECTION:gocl-error
 * @short_description: API related to error management
 * @stability: Unstable
 *
 * Internal functions related to error management. This API is private, not
 * supposed to be used in applications.
 **/

#include "gocl-error.h"
#include "gocl-private.h"

static GError *last_error = NULL;

static const gchar *
get_error_code_description (cl_int err_code)
{
  switch (err_code) {
  case CL_SUCCESS:                            return "Success!";
  case CL_DEVICE_NOT_FOUND:                   return "Device not found.";
  case CL_DEVICE_NOT_AVAILABLE:               return "Device not available";
  case CL_COMPILER_NOT_AVAILABLE:             return "Compiler not available";
  case CL_MEM_OBJECT_ALLOCATION_FAILURE:      return "Memory object allocation failure";
  case CL_OUT_OF_RESOURCES:                   return "Out of resources";
  case CL_OUT_OF_HOST_MEMORY:                 return "Out of host memory";
  case CL_PROFILING_INFO_NOT_AVAILABLE:       return "Profiling information not available";
  case CL_MEM_COPY_OVERLAP:                   return "Memory copy overlap";
  case CL_IMAGE_FORMAT_MISMATCH:              return "Image format mismatch";
  case CL_IMAGE_FORMAT_NOT_SUPPORTED:         return "Image format not supported";
  case CL_BUILD_PROGRAM_FAILURE:              return "Program build failure";
  case CL_MAP_FAILURE:                        return "Map failure";
  case CL_INVALID_VALUE:                      return "Invalid value";
  case CL_INVALID_DEVICE_TYPE:                return "Invalid device type";
  case CL_INVALID_PLATFORM:                   return "Invalid platform";
  case CL_INVALID_DEVICE:                     return "Invalid device";
  case CL_INVALID_CONTEXT:                    return "Invalid context";
  case CL_INVALID_QUEUE_PROPERTIES:           return "Invalid queue properties";
  case CL_INVALID_COMMAND_QUEUE:              return "Invalid command queue";
  case CL_INVALID_HOST_PTR:                   return "Invalid host pointer";
  case CL_INVALID_MEM_OBJECT:                 return "Invalid memory object";
  case CL_INVALID_IMAGE_FORMAT_DESCRIPTOR:    return "Invalid image format descriptor";
  case CL_INVALID_IMAGE_SIZE:                 return "Invalid image size";
  case CL_INVALID_SAMPLER:                    return "Invalid sampler";
  case CL_INVALID_BINARY:                     return "Invalid binary";
  case CL_INVALID_BUILD_OPTIONS:              return "Invalid build options";
  case CL_INVALID_PROGRAM:                    return "Invalid program";
  case CL_INVALID_PROGRAM_EXECUTABLE:         return "Invalid program executable";
  case CL_INVALID_KERNEL_NAME:                return "Invalid kernel name";
  case CL_INVALID_KERNEL_DEFINITION:          return "Invalid kernel definition";
  case CL_INVALID_KERNEL:                     return "Invalid kernel";
  case CL_INVALID_ARG_INDEX:                  return "Invalid argument index";
  case CL_INVALID_ARG_VALUE:                  return "Invalid argument value";
  case CL_INVALID_ARG_SIZE:                   return "Invalid argument size";
  case CL_INVALID_KERNEL_ARGS:                return "Invalid kernel arguments";
  case CL_INVALID_WORK_DIMENSION:             return "Invalid work dimension";
  case CL_INVALID_WORK_GROUP_SIZE:            return "Invalid work group size";
  case CL_INVALID_WORK_ITEM_SIZE:             return "Invalid work item size";
  case CL_INVALID_GLOBAL_OFFSET:              return "Invalid global offset";
  case CL_INVALID_EVENT_WAIT_LIST:            return "Invalid event wait list";
  case CL_INVALID_EVENT:                      return "Invalid event";
  case CL_INVALID_OPERATION:                  return "Invalid operation";
  case CL_INVALID_GL_OBJECT:                  return "Invalid OpenGL object";
  case CL_INVALID_BUFFER_SIZE:                return "Invalid buffer size";
  case CL_INVALID_MIP_LEVEL:                  return "Invalid mip-map level";
  default: return "Unknown";
  }
}

/**
 * gocl_error_check_opencl:
 * @err_code: (type guint64): An OpenCL error code
 * @error: (out) (allow-none): A pointer to a #GError, or %NULL
 *
 * Checks if @err_code describes an OpenCL error and fills @error pointer,
 * if not %NULL, with a new #GError with the corresponding domain, code
 * and description.
 *
 * This is a Gocl private function, not exposed to applications.
 *
 * Returns: %TRUE if there is an error, %FALSE otherwise
 **/
gboolean
gocl_error_check_opencl (cl_int err_code, GError **error)
{
  if (err_code != CL_SUCCESS)
    {
      g_set_error_literal (error,
                           GOCL_OPENCL_ERROR,
                           err_code,
                           get_error_code_description (err_code));
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

gboolean
gocl_error_check_opencl_internal (cl_int err_code)
{
  g_clear_error (&last_error);

  if (err_code != CL_SUCCESS)
    {
      g_set_error_literal (&last_error,
                           GOCL_OPENCL_ERROR,
                           err_code,
                           get_error_code_description (err_code));
      return TRUE;
    }
  else
    {
      return FALSE;
    }
}

/**
 * gocl_error_prepare:
 *
 * Prepares the internal Gocl error for immediate use, by freeing it if
 * non-%NULL.
 *
 * This is a Gocl private function, not exposed to applications.
 *
 * Returns: (transfer none): A pointer to Gocl's internal #GError
 **/
GError **
gocl_error_prepare (void)
{
  g_clear_error (&last_error);

  return &last_error;
}

/**
 * gocl_error_get_last:
 *
 * Retrieves the error that ocurred in the last Gocl operation, if any, or
 * %NULL if the last operation was successful.
 *
 * Returns: (transfer full): A pointer to a newly created error, or %NULL
 **/
GError *
gocl_error_get_last (void)
{
  return g_error_copy (last_error);
}

/**
 * gocl_error_free:
 *
 * Frees the internal Gocl error if it is not %NULL. Applications should
 * not normally need to ever call this function, except before the end of
 * execution of the program, to avoid leaking memory from a potential error
 * in the last Gocl operation.
 **/
void
gocl_error_free (void)
{
  gocl_error_prepare ();
}
