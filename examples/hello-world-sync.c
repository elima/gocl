/*
 * hello-world-sync.c
 *
 * Gocl - GLib/GObject wrapper for OpenCL
 * Copyright (C) 2012-2013 Igalia S.L.
 *
 * Authors:
 *  Eduardo Lima Mitev <elima@igalia.com>
 */

#include <gocl.h>

#define SIZE 1920 * 1080
#define RUNS 1

/* a simple OpenCL program */
static const gchar *source =
  ""
  "__kernel void my_kernel (__global char *data, const int size) {"
  "  int tid = get_global_id (0);"
  "  int local_work_size = get_local_size (0);"
  ""
  "  for (int i=tid * local_work_size; i<tid * local_work_size + local_work_size; i++) {"
  "    if (i < size)"
  "      data[i] = tid;"
  "  }"
  "}";

gint
main (gint argc, gchar *argv[])
{
  gint exit_code = 0;
  GError *error = NULL;
  gint i;

  GoclContext *context;
  GoclDevice *device;
  GoclBuffer *buffer;
  GoclProgram *prog;
  GoclKernel *kernel;

  gsize local_worksize;
  gsize global_worksize;
  guchar *data;

  g_type_init ();

  /* create context */

  /* First attempt to create a GPU context and if that fails,try with CPU */
  context = gocl_context_get_default_gpu (&error);
  if (context == NULL)
    {
      g_print ("Failed to create GPU context (%d): %s\n",
               error->code,
               error->message);
      g_error_free (error);
      error = NULL;


      g_print ("Trying with CPU context... ");
      context = gocl_context_get_default_cpu (&error);
      if (context == NULL)
        {
          g_print ("Failed to create CPU context: %s\n", error->message);
          goto out;
        }
    }

  g_print ("Context created\n");
  g_print ("Num devices: %u\n", gocl_context_get_num_devices (context));

  /* get the first device in context */
  device = gocl_context_get_device_by_index (context, 0);

  /* create a program */
  prog = gocl_program_new (context, &source, 1, &error);
  if (prog == NULL)
    {
      g_print ("Failed to create program: %s\n", error->message);
      goto out;
    }
  g_print ("Program created\n");

  /* build the program */
  if (! gocl_program_build_sync (prog, "", &error))
    {
      g_print ("Failed to build program: %s\n", error->message);
      goto out;
    }
  g_print ("Program built\n");

  /* get a kernel */
  kernel = gocl_program_get_kernel (prog, "my_kernel", &error);
  if (kernel == NULL)
    {
      g_print ("Failed to create kernel: %s\n", error->message);
      goto out;
    }
  g_print ("Kernel created\n");

  /* get work sizes */
  gsize max_workgroup_size;
  gint32 size = SIZE;

  max_workgroup_size = gocl_device_get_max_work_group_size (device, &error);
  if (max_workgroup_size == 0)
    {
      g_print ("Failed to obtain device's max work group size: %s\n", error->message);
      goto out;
    }

  g_print ("Max work group size: %lu\n", max_workgroup_size);

  local_worksize = MIN (size, max_workgroup_size);
  global_worksize = (size % local_worksize == 0) ?
    size :
    (size / local_worksize + 1) * local_worksize;

  g_print ("Global work size: %lu\n", global_worksize);
  g_print ("Local work size: %lu\n", local_worksize);

  /* create data buffer */
  data = g_slice_alloc0 (sizeof (guchar) * size);

  buffer = gocl_context_create_buffer (context,
                                       GOCL_BUFFER_FLAGS_READ_WRITE,
                                       sizeof (guchar) * size,
                                       data,
                                       &error);
  if (buffer == NULL)
    {
      g_print ("Failed to create buffer: %s\n", error->message);
      goto out;
    }
  g_print ("Buffer created\n");

  /* set kernel arguments */
  if (! gocl_kernel_set_argument_buffer (kernel,
                                         0,
                                         buffer,
                                         &error))
    {
      g_print ("ERROR: Failed to set 'data' argument to kernel: %s\n", error->message);
      goto out;
    }

  if (! gocl_kernel_set_argument_int32 (kernel,
                                        1,
                                        1,
                                        &size,
                                        &error))
    {
      g_print ("ERROR: Failed to set 'size' argument to kernel: %s\n", error->message);
      goto out;
    }

  g_print ("Kernel execution starts\n");

  /* run the kernel */
  for (i=0; i<RUNS; i++)
    {
      if (! gocl_kernel_run_in_device_sync (kernel,
                                            device,
                                            global_worksize,
                                            local_worksize,
                                            NULL,
                                            &error))
        {
          g_print ("ERROR: Failed to run kernel: %s\n", error->message);
          goto out;
        }
    }

  g_print ("Kernel execution finished\n");

  /* read back buffer */
  if (! gocl_buffer_read_sync (buffer,
                               gocl_device_get_default_queue (device, NULL),
                               data,
                               sizeof (guchar) * size,
                               0,
                               NULL,
                               &error))
    {
      g_print ("Failed to read buffer: %s\n", error->message);
      goto out;
    }

  /* print results */
  if (SIZE < 500)
    for (i=0; i<SIZE; i++)
      g_print ("%d ", data[i]);
  g_print ("\n");

  /* free stuff */
  g_slice_free1 (SIZE, data);

  g_object_unref (buffer);
  g_object_unref (kernel);
  g_object_unref (prog);
  g_object_unref (device);
  g_object_unref (context);

 out:
  if (error != NULL)
    {
      g_print ("Exit with error: %s\n", error->message);
      exit_code = error->code;
      g_error_free (error);
    }
  else
    {
      g_print ("Exit clean :)\n");
    }

  return exit_code;
}
