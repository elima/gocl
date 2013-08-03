/*
 * hello-world.c
 *
 * Gocl - GLib/GObject wrapper for OpenCL
 * Copyright (C) 2013 Igalia S.L.
 *
 * Authors:
 *  Eduardo Lima Mitev <elima@igalia.com>
 */

#include <string.h>
#include <gocl.h>

#define WIDTH  32
#define HEIGHT 32

static GoclContext *context;
static GoclDevice *device;
static GoclKernel *kernel;

static guint8 *data = NULL;
static gsize data_size = 0;

static GMainLoop *main_loop;

static void
on_read_complete (GoclEvent *event,
                  GError    *error,
                  gpointer   user_data)
{
  gint i, j;

  if (error != NULL)
    {
      g_print ("Kernel execution failed: %s\n", error->message);
      g_error_free (error);
    }
  else
    {
      g_print ("Kernel execution finished\n");
    }

  /* print results */
  if (WIDTH * HEIGHT <= 32 * 32)
    for (i=0; i<HEIGHT; i++) {
      for (j=0; j<WIDTH; j++)
        g_print ("%02x ", data[i * WIDTH + j]);
      g_print ("\n");
    }
  g_print ("\n");

  g_main_loop_quit (main_loop);
}

static void
on_program_built (GObject      *obj,
                  GAsyncResult *res,
                  gpointer      user_data)
{
  GError *error = NULL;
  GoclProgram *program = GOCL_PROGRAM (obj);
  GoclBuffer *buffer = NULL;

  if (! gocl_program_build_finish (program, res, &error))
    {
      g_print ("Failed to build program: %s\n", error->message);
      g_error_free (error);
      goto out;
    }
  g_print ("Program built\n");

  /* get a kernel */
  kernel = gocl_program_get_kernel (program, "my_kernel", &error);
  if (kernel == NULL)
    {
      g_print ("Failed to create kernel: %s\n", error->message);
      goto out;
    }
  g_print ("Kernel created\n");

  /* get work sizes */
  gsize max_workgroup_size;
  gint32 size = WIDTH * HEIGHT;

  max_workgroup_size = gocl_device_get_max_work_group_size (device, &error);
  if (max_workgroup_size == 0)
    {
      g_print ("Failed to obtain device's max work group size: %s\n", error->message);
      goto out;
    }

  g_print ("Max work group size: %lu\n", max_workgroup_size);

  gocl_kernel_set_work_dimension (kernel, 2);
  gocl_kernel_set_global_work_size (kernel,
                                    WIDTH,
                                    HEIGHT,
                                    0);
  gocl_kernel_set_local_work_size (kernel,
                                   0,
                                   0,
                                   0);

  /* create data buffer */
  data_size = sizeof (*data) * WIDTH * HEIGHT;
  data = g_slice_alloc0 (data_size);

  buffer = gocl_buffer_new (context,
                            GOCL_BUFFER_FLAGS_READ_WRITE,
                            data_size,
                            NULL,
                            &error);
  if (buffer == NULL)
    {
      g_print ("Failed to create buffer: %s\n", error->message);
      goto out;
    }
  g_print ("Buffer created\n");

  /* initialize buffer */
  GoclEvent *write_event;

  memset (data, 0, data_size);

  write_event = gocl_buffer_write (buffer,
                                   gocl_device_get_default_queue (device, NULL),
                                   data,
                                   data_size,
                                   0,
                                   NULL);

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

  /* run the kernel asynchronously */
  GoclEvent *run_event;
  GList *event_wait_list = NULL;

  g_print ("Kernel execution starts\n");

  event_wait_list = g_list_append (event_wait_list, write_event);

  run_event = gocl_kernel_run_in_device (kernel,
                                         device,
                                         event_wait_list);

  g_list_free (event_wait_list);
  event_wait_list = NULL;

  g_print ("Kernel running\n");

  /* read back from buffer, synchronizing with kernel execution */
  GoclEvent *read_event;

  event_wait_list = g_list_append (event_wait_list, run_event);

  read_event = gocl_buffer_read (buffer,
                                 gocl_device_get_default_queue (device, NULL),
                                 data,
                                 data_size,
                                 0,
                                 event_wait_list);
  g_list_free (event_wait_list);

  gocl_event_then (read_event, on_read_complete, NULL);

  return;

 out:
  if (data != NULL)
    g_slice_free1 (data_size, data);
  g_object_unref (kernel);
  if (buffer != NULL)
    g_object_unref (buffer);

  if (error != NULL)
    {
      g_error_free (error);
      g_main_loop_quit (main_loop);
    }
}

gint
main (gint argc, gchar *argv[])
{
  gint exit_code = 0;
  GError *error = NULL;
  GoclProgram *prog = NULL;

#ifndef GLIB_VERSION_2_36
  g_type_init ();
#endif

  /* create context */

  /* First attempt to create a GPU context and if that fails,try with CPU */
  context = gocl_context_get_default_gpu_sync (&error);
  if (context == NULL)
    {
      g_print ("Failed to create GPU context (%d): %s\n",
               error->code,
               error->message);
      g_error_free (error);
      error = NULL;

      g_print ("Trying with CPU context... ");
      context = gocl_context_get_default_cpu_sync (&error);
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

  g_print ("Max compute units: %u\n", gocl_device_get_max_compute_units (device));
  if (gocl_device_has_extension (device, "cl_khr_gl_sharing"))
    {
      g_print ("Supports gl sharing\n");
    }

  /* create a program */
  prog = gocl_program_new_from_file_sync (context,
                                          EXAMPLES_DIR "hello-world.cl",
                                          &error);
  if (prog == NULL)
    {
      g_print ("Failed to create program: %s\n", error->message);
      goto out;
    }
  g_print ("Program created\n");

  /* build the program asynchronously */
  gocl_program_build (prog, "", NULL, on_program_built, context);

  /* start the show */
  main_loop = g_main_loop_new (NULL, FALSE);
  g_main_loop_run (main_loop);

 out:
  /* free stuff */
  if (prog != NULL)
    g_object_unref (prog);
  if (device != NULL)
    g_object_unref (device);
  if (context != NULL)
    g_object_unref (context);

  if (error != NULL)
    {
      g_print ("Exit with error: %s\n", error->message);
      exit_code = error->code;
      g_error_free (error);
    }
  else
    {
      g_print ("Clean exit :)\n");
    }

  return exit_code;
}
