/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * gstspectrogramscope.c: frequency spectrum scope
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.

 */
/**
 * SECTION:element-spectrogramscope
 * @title: spectrogramscope
 * @see_also: goom
 *
 * Spectrogramscope is a simple spectrum visualisation element. It renders the
 * frequency spectrum as a series of bars.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! spectrogramscope ! ximagesink
 * ]|
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>

#include "gstspectrogramscope.h"

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define RGB_ORDER "xRGB"
#else
#define RGB_ORDER "BGRx"
#endif

static GstStaticPadTemplate gst_spectrogram_scope_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (RGB_ORDER))
    );

static GstStaticPadTemplate gst_spectrogram_scope_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 8000, 96000 ], "
        "channels = (int) 2, " "channel-mask = (bitmask) 0x3")
    );


GST_DEBUG_CATEGORY_STATIC (spectrogram_scope_debug);
#define GST_CAT_DEFAULT spectrogram_scope_debug

static void gst_spectrogram_scope_finalize (GObject * object);

static gboolean gst_spectrogram_scope_setup (GstAudioVisualizer * scope);
static gboolean gst_spectrogram_scope_render (GstAudioVisualizer * scope,
    GstBuffer * audio, GstVideoFrame * video);


G_DEFINE_TYPE (GstSpectrogramScope, gst_spectrogram_scope,
    GST_TYPE_AUDIO_VISUALIZER);

static void
gst_spectrogram_scope_class_init (GstSpectrogramScopeClass * g_class)
{
  GObjectClass *gobject_class = (GObjectClass *) g_class;
  GstElementClass *element_class = (GstElementClass *) g_class;
  GstAudioVisualizerClass *scope_class = (GstAudioVisualizerClass *) g_class;

  gobject_class->finalize = gst_spectrogram_scope_finalize;

  gst_element_class_set_static_metadata (element_class,
      "Frequency spectrum scope", "Visualization",
      "Simple frequency spectrum scope", "Stefan Kost <ensonic@users.sf.net>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_spectrogram_scope_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_spectrogram_scope_sink_template);

  scope_class->setup = GST_DEBUG_FUNCPTR (gst_spectrogram_scope_setup);
  scope_class->render = GST_DEBUG_FUNCPTR (gst_spectrogram_scope_render);
}

static void
gst_spectrogram_scope_init (GstSpectrogramScope * scope)
{
  /* do nothing */
}

static void
gst_spectrogram_scope_finalize (GObject * object)
{
  GstSpectrogramScope *scope = GST_SPECTROGRAM_SCOPE (object);

  if (scope->fft_ctx) {
    gst_fft_s16_free (scope->fft_ctx);
    scope->fft_ctx = NULL;
  }
  if (scope->freq_data) {
    g_free (scope->freq_data);
    scope->freq_data = NULL;
  }

  G_OBJECT_CLASS (gst_spectrogram_scope_parent_class)->finalize (object);
}

static gboolean
gst_spectrogram_scope_setup (GstAudioVisualizer * bscope)
{
  GstSpectrogramScope *scope = GST_SPECTROGRAM_SCOPE (bscope);
  guint num_freq = GST_VIDEO_INFO_WIDTH (&bscope->vinfo) + 1;

  if (scope->fft_ctx)
    gst_fft_s16_free (scope->fft_ctx);
  g_free (scope->freq_data);

  /* we'd need this amount of samples per render() call */
  bscope->req_spf = num_freq * 2 - 2;
  scope->fft_ctx = gst_fft_s16_new (bscope->req_spf, FALSE);
  scope->freq_data = g_new (GstFFTS16Complex, num_freq);

  return TRUE;
}

static inline void
add_pixel (guint32 * _p, guint32 _c)
{
  guint8 *p = (guint8 *) _p;
  guint8 *c = (guint8 *) & _c;

  if (p[0] < 255 - c[0])
    p[0] += c[0];
  else
    p[0] = 255;
  if (p[1] < 255 - c[1])
    p[1] += c[1];
  else
    p[1] = 255;
  if (p[2] < 255 - c[2])
    p[2] += c[2];
  else
    p[2] = 255;
  if (p[3] < 255 - c[3])
    p[3] += c[3];
  else
    p[3] = 255;
}

static gboolean
gst_spectrogram_scope_render (GstAudioVisualizer * bscope, GstBuffer * audio,
    GstVideoFrame * video)
{
  GstSpectrogramScope *scope = GST_SPECTROGRAM_SCOPE (bscope);
  gint16 *mono_adata;
  GstFFTS16Complex *fdata = scope->freq_data;
  guint x = 0, y = 0;
  guint w = GST_VIDEO_INFO_WIDTH (&bscope->vinfo);
  guint h = GST_VIDEO_INFO_HEIGHT (&bscope->vinfo) - 1;
  gfloat fr, fi;
  GstMapInfo amap;
  guint32 *vdata;
  guint32 off = 0;
  gint channels;

  static guint fft_array[SPECTROGRAM_WIDTH][SPECTROGRAM_HIGHT];
  static guint32 collumn_pointer = 0;

  gst_buffer_map (audio, &amap, GST_MAP_READ);
  vdata = (guint32 *) GST_VIDEO_FRAME_PLANE_DATA (video, 0);

  channels = GST_AUDIO_INFO_CHANNELS (&bscope->ainfo);

  mono_adata = (gint16 *) g_memdup (amap.data, amap.size);

  if (channels > 1) {
    guint ch = channels;
    guint num_samples = amap.size / (ch * sizeof (gint16));
    guint i, c, v, s = 0;

    /* deinterleave and mixdown adata */
    for (i = 0; i < num_samples; i++) {
      v = 0;
      for (c = 0; c < ch; c++) {
        v += mono_adata[s++];
      }
      mono_adata[i] = v / ch;
    }
  }

  /* run fft */
  gst_fft_s16_window (scope->fft_ctx, mono_adata, GST_FFT_WINDOW_HAMMING);
  gst_fft_s16_fft (scope->fft_ctx, mono_adata, fdata);
  g_free (mono_adata);

  collumn_pointer++;
  if (collumn_pointer == SPECTROGRAM_WIDTH) {
    collumn_pointer = 0;
  }

  /* for each pixel in the current fft line, calculate fft height and push to array */
  for (y = 0; y < h; y++) {
    fr = (gfloat) fdata[1 + y].r / 512.0;
    fi = (gfloat) fdata[1 + y].i / 512.0;
    fft_array[collumn_pointer][y] = (guint) (h * sqrt (fr * fr + fi * fi));
  }

  /* draw array */
  for (x = 0; x < w; x++) {
    for (y = 0; y < h; y++) {
      off = ((h - y - 1) * w) + x;
      vdata[off] =
          (fft_array[x][y] << 16) | (fft_array[x][y] << 8) | (fft_array[x][y]);
    }
  }

  gst_buffer_unmap (audio, &amap);
  return TRUE;
}

gboolean
gst_spectrogram_scope_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (spectrogram_scope_debug, "spectrogramscope", 0,
      "spectrogramscope");

  return gst_element_register (plugin, "spectrogramscope", GST_RANK_NONE,
      GST_TYPE_SPECTROGRAM_SCOPE);
}
