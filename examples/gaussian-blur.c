/*
 * gaussian-blur.c
 *
 * Gocl - GLib/GObject wrapper for OpenCL
 * Copyright (C) 2013 Igalia S.L.
 *
 * Authors:
 *  Eduardo Lima Mitev <elima@igalia.com>
 */

#define COGL_ENABLE_EXPERIMENTAL_API

#include <stdio.h>
#include <glib.h>
#include <cogl/cogl.h>
#include <gocl.h>
#include <GL/glx.h>
#include <math.h>

#define BLUR_FACTOR 8.0f

static GMainLoop *loop = NULL;

typedef struct _Data
{
    CoglContext *ctx;
    CoglFramebuffer *fb;
    CoglPrimitive *triangle;
    CoglPipeline *pipeline;
} Data;

typedef struct
{
  GoclContext *context;
  GoclDevice *device;
  GoclProgram *program;
  GoclKernel *kernel;
  GoclImage *img;
  GoclImage *img1;
} GoclData;

static void
trap_signals (gint sig)
{
  signal (SIGINT, NULL);
  signal (SIGTERM, NULL);

  if (loop != NULL)
    {
      g_main_loop_quit (loop);
    }
}

static CoglBool
paint_cb (gpointer user_data)
{
  Data *data = user_data;

  cogl_framebuffer_clear4f (data->fb, COGL_BUFFER_BIT_COLOR, 0, 0, 0, 1);
  cogl_framebuffer_draw_primitive (data->fb, data->pipeline, data->triangle);
  cogl_onscreen_swap_buffers (COGL_ONSCREEN (data->fb));

  return FALSE;
}

static void
frame_event_cb (CoglOnscreen   *onscreen,
                CoglFrameEvent  event,
                CoglFrameInfo  *info,
                gpointer        user_data)
{
  if (event == COGL_FRAME_EVENT_SYNC)
    paint_cb (user_data);
}

static gfloat *
create_blur_mask (gfloat sigma, gint32 *maskSizePointer) {
  gint32 maskSize;
  gfloat *mask;
  gfloat sum = 0.0f;
  gint a, b, i;

  maskSize = (gint32) ceil (3.0f * sigma);
  mask = g_slice_alloc0 (sizeof (gfloat) * (maskSize * 2 + 1) * (maskSize * 2 + 1));

  for (a = -maskSize; a < maskSize + 1; a++)
    for (b = -maskSize; b < maskSize + 1; b++)
      {
        gfloat temp;

        temp = exp (-((gfloat) (a*a+b*b) / (2*sigma*sigma)));
        sum += temp;
        mask[a + maskSize + (b + maskSize) * (maskSize * 2 + 1)] = temp;
      }

  for (i = 0; i < (maskSize * 2 + 1) * (maskSize * 2 + 1); i++)
    mask[i] = mask[i] / sum;

  *maskSizePointer = maskSize;

  return mask;
}

gint
main (gint argc, gchar *argv[])
{
  Data data;
  GoclData gocl_data;
  CoglOnscreen *onscreen;
  GError *err = NULL;
  CoglVertexP2T2 triangle_vertices[] =
    {
      { -1.0, -1.0,  0.0, 1.0},
      {  1.0,  1.0,  1.0, 0.0},
      {  1.0, -1.0,  1.0, 1.0},
      { -1.0,  1.0,  0.0, 0.0},
      {  1.0,  1.0,  1.0, 0.0},
      { -1.0, -1.0,  0.0, 1.0}
    };
  GSource *cogl_source;

  data.ctx = cogl_context_new (NULL, &err);
  g_assert_no_error (err);

  onscreen = cogl_onscreen_new (data.ctx, 800, 600);
  cogl_onscreen_show (onscreen);
  data.fb = COGL_FRAMEBUFFER (onscreen);

  cogl_onscreen_set_resizable (onscreen, TRUE);

  data.triangle = cogl_primitive_new_p2t2 (data.ctx,
                                           COGL_VERTICES_MODE_TRIANGLES,
                                           6, triangle_vertices);
  data.pipeline = cogl_pipeline_new (data.ctx);

  cogl_source = cogl_glib_source_new (data.ctx, G_PRIORITY_DEFAULT);

  g_source_attach (cogl_source, NULL);

  cogl_onscreen_add_frame_callback (COGL_ONSCREEN (data.fb),
                                    frame_event_cb,
                                    &data,
                                    NULL);
  g_idle_add (paint_cb, &data);

  /* create two textures */
  CoglTexture *tex = cogl_texture_new_from_file (EXAMPLES_DIR "colorful.jpg",
                                                 COGL_TEXTURE_NONE,
                                                 COGL_PIXEL_FORMAT_RGBA_8888,
                                                 &err);
  g_assert_no_error (err);

  CoglTexture *tex1 = cogl_texture_new_with_size (cogl_texture_get_width (tex),
                                                  cogl_texture_get_height (tex),
                                                  COGL_TEXTURE_NONE,
                                                  COGL_PIXEL_FORMAT_RGBA_8888);

  cogl_pipeline_set_layer_texture (data.pipeline, 0, tex1);

  /* create OpenCL context */
  gboolean gl_sharing = FALSE;
  GLXContext glContext;
  Display *glDisplay;

  glContext = glXGetCurrentContext ();
  glDisplay = glXGetCurrentDisplay ();

  gocl_data.context = gocl_context_gpu_new_sync (glContext, glDisplay, &err);
  if (gocl_data.context == NULL)
    {
      g_print ("Error creating GPU context: %s\n", err->message);
      g_clear_error (&err);

      gocl_data.context = gocl_context_get_default_cpu_sync (&err);
      if (gocl_data.context == NULL)
        {
          g_print ("Error creating CPU context: %s\n", err->message);
          g_error_free (err);
          return -1;
        }
    }
  else
    {
      gl_sharing = TRUE;
    }

  /* create image buffers */
  gsize width, height;
  guint8 *tex_data = NULL;
  gsize tex_size;

  if (gl_sharing)
    {
      GLuint gl_tex, gl_tex1;

      cogl_texture_get_gl_texture (tex, &gl_tex, NULL);
      gocl_data.img = gocl_image_new_from_gl_texture (gocl_data.context,
                                                      GOCL_BUFFER_FLAGS_READ_ONLY,
                                                      gl_tex,
                                                      &err);
      g_assert_no_error (err);

      cogl_texture_get_gl_texture (tex1, &gl_tex1, NULL);
      gocl_data.img1 = gocl_image_new_from_gl_texture (gocl_data.context,
                                                       GOCL_BUFFER_FLAGS_WRITE_ONLY,
                                                       gl_tex1,
                                                       &err);
      g_assert_no_error (err);

      g_object_get (gocl_data.img1,
                    "width", &width,
                    "height", &height,
                    NULL);
    }
  else
    {
      tex_size = cogl_texture_get_data (tex,
                                        COGL_PIXEL_FORMAT_RGBA_8888,
                                        0,
                                        NULL);
      tex_data = g_new0 (guint8, tex_size);
      cogl_texture_get_data (tex,
                             COGL_PIXEL_FORMAT_RGBA_8888,
                             0,
                             tex_data);

      gocl_data.img = gocl_image_new (gocl_data.context,
                                      GOCL_BUFFER_FLAGS_READ_WRITE |
                                      GOCL_BUFFER_FLAGS_USE_HOST_PTR,
                                      tex_data,
                                      GOCL_IMAGE_TYPE_2D,
                                      cogl_texture_get_width (tex),
                                      cogl_texture_get_height (tex),
                                      0,
                                      &err);
      g_assert_no_error (err);

      g_object_get (gocl_data.img,
                    "width", &width,
                    "height", &height,
                    NULL);

      gocl_data.img1 = gocl_image_new (gocl_data.context,
                                       GOCL_BUFFER_FLAGS_READ_WRITE,
                                       NULL,
                                       GOCL_IMAGE_TYPE_2D,
                                       width,
                                       height,
                                       0,
                                       &err);
      g_assert_no_error (err);
    }

  g_print ("CL images created\n");
  g_print ("Image size: %lux%lu\n", width, height);

  /* device */
  gocl_data.device = gocl_context_get_device_by_index (gocl_data.context, 0);
  g_print ("Device created\n");

  /* OpenCL program */
  gocl_data.program =
    gocl_program_new_from_file_sync (gocl_data.context,
                                     EXAMPLES_DIR "gaussian-blur.cl",
                                     &err);
  g_assert_no_error (err);
  g_print ("Program created\n");

  /* build program */
  gocl_program_build_sync (gocl_data.program, "", &err);
  g_assert_no_error (err);
  g_print ("Program built\n");

  /* get kernel */
  gocl_data.kernel = gocl_program_get_kernel (gocl_data.program,
                                              "gaussian_blur",
                                              &err);
  g_assert_no_error (err);
  g_print ("Kernel ready\n");

  /* create gaussian mask */
  gfloat *mask;
  gint32 mask_size;
  gsize mask_alloc_size;
  GoclBuffer *mask_buf;
  mask = create_blur_mask (BLUR_FACTOR, (gint32 *) &mask_size);
  mask_alloc_size = sizeof (gfloat) * (mask_size * 2 + 1) * (mask_size * 2 + 1);
  mask_buf = gocl_buffer_new (gocl_data.context,
                              GOCL_BUFFER_FLAGS_READ_ONLY |
                              GOCL_BUFFER_FLAGS_USE_HOST_PTR,
                              mask_alloc_size,
                              mask,
                              &err);
  g_assert_no_error (err);

  /* set kernel arguments */
  gocl_kernel_set_argument_buffer (gocl_data.kernel,
                                   0,
                                   GOCL_BUFFER (gocl_data.img),
                                   &err);
  g_assert_no_error (err);
  gocl_kernel_set_argument_buffer (gocl_data.kernel,
                                   1,
                                   GOCL_BUFFER (gocl_data.img1),
                                   &err);
  g_assert_no_error (err);
  gocl_kernel_set_argument_buffer (gocl_data.kernel, 2, mask_buf, &err);
  g_assert_no_error (err);
  gocl_kernel_set_argument_int32 (gocl_data.kernel, 3, 1, &mask_size, &err);
  g_assert_no_error (err);

  gocl_kernel_set_work_dimension (gocl_data.kernel, 2);
  gocl_kernel_set_global_work_size (gocl_data.kernel, width, height, 0);
  gocl_kernel_set_local_work_size (gocl_data.kernel, 0, 0, 0);

  /* run kernel */
  gocl_kernel_run_in_device_sync (gocl_data.kernel,
                                  gocl_data.device,
                                  NULL,
                                  &err);
  g_assert_no_error (err);
  g_print ("kernel ran\n");

  if (! gl_sharing)
    {
      GoclQueue *queue;

      queue = gocl_device_get_default_queue (gocl_data.device, &err);
      g_assert_no_error (err);

      /* read back from the blurred image */
      gocl_buffer_read_all_sync (GOCL_BUFFER (gocl_data.img1),
                                 queue,
                                 tex_data,
                                 NULL,
                                 NULL,
                                 &err);
      g_assert_no_error (err);

      /* fill the target texture */
      cogl_texture_set_region (tex1,
                               0, 0,
                               0, 0,
                               width, height,
                               width, height,
                               COGL_PIXEL_FORMAT_RGBA_8888,
                               0,
                               tex_data);
      g_free (tex_data);
    }

  signal (SIGINT, trap_signals);
  signal (SIGTERM, trap_signals);

  /* start the show */
  loop = g_main_loop_new (NULL, TRUE);
  g_main_loop_run (loop);

  /* free stuff */
  g_main_loop_unref (loop);
  g_slice_free1 (mask_alloc_size, mask);

  g_object_unref (gocl_data.context);
  g_object_unref (gocl_data.device);
  g_object_unref (gocl_data.program);
  g_object_unref (gocl_data.kernel);
  g_object_unref (gocl_data.img);
  g_object_unref (gocl_data.img1);

  cogl_object_unref (tex);
  cogl_object_unref (tex1);
  cogl_object_unref (data.ctx);
  cogl_object_unref (data.fb);
  cogl_object_unref (data.pipeline);
  cogl_object_unref (data.triangle);

  g_print ("Clean exit\n");

  return 0;
}
