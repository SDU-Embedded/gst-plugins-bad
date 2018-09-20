/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
 * gstpowereventemitter.c: frequency spectrum scope
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
 * SECTION:element-powereventemitter
 * @title: powereventemitter
 * @see_also: goom
 *
 * Spectrogramscope is a simple spectrum visualisation element. It renders the
 * frequency spectrum as a series of bars.
 *
 * ## Example launch line
 * |[
 * gst-launch-1.0 audiotestsrc ! audioconvert ! powereventemitter ! ximagesink
 * ]|
 *
 */
#ifdef HAVE_CONFIG_H
#include "config.h"
#endif
#include <stdlib.h>

#include "gstpowereventemitter.h"

#if G_BYTE_ORDER == G_BIG_ENDIAN
#define RGB_ORDER "xRGB"
#else
#define RGB_ORDER "BGRx"
#endif

// Enumeration of the properties
enum
{
  PROP_0,
  PROP_COLORMAP
};

// Enumeration of the options for the colormap property
enum
{
  COLORMAP_GREY = 0,
  COLORMAP_GOLD
};

// Function prototypes
static void gst_power_event_emitter_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_power_event_emitter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static void gst_power_event_emitter_finalize (GObject * object);
static gboolean gst_power_event_emitter_setup (GstAudioEventEmitter * scope);
static gboolean gst_power_event_emitter_render (GstAudioEventEmitter * scope,
    GstBuffer * audio, GstVideoFrame * video);
static gint32 gst_power_event_emitter_greymap (gint);
static gint32 gst_power_event_emitter_colormap (gint);

// Gstreamer declarations
#define GST_TYPE_POWER_EVENT_EMITTER_STYLE (gst_power_event_emitter_colormap_get_type ())
#define GST_CAT_DEFAULT power_event_emitter_debug

GST_DEBUG_CATEGORY_STATIC (power_event_emitter_debug);
G_DEFINE_TYPE (GstPowerEventEmitter, gst_power_event_emitter,
    GST_TYPE_AUDIO_EVENT_EMITTER);

static GstStaticPadTemplate gst_power_event_emitter_src_template =
GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS (GST_VIDEO_CAPS_MAKE (RGB_ORDER))
    );

static GstStaticPadTemplate gst_power_event_emitter_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) " GST_AUDIO_NE (S16) ", "
        "layout = (string) interleaved, "
        "rate = (int) [ 8000, 96000 ], "
        "channels = (int) 2, " "channel-mask = (bitmask) 0x3")
    );

static GType
gst_power_event_emitter_colormap_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      {COLORMAP_GREY, "black to white (default)", "grey"},
      {COLORMAP_GOLD, "black to yellow to white", "gold"},
      {0, NULL, NULL}
    };

    gtype = g_enum_register_static ("GstPowerEventEmitterColormap", values);
  }
  return gtype;
}

static void
gst_power_event_emitter_class_init (GstPowerEventEmitterClass * g_class)
{
  GObjectClass *gobject_class = (GObjectClass *) g_class;
  GstElementClass *element_class = (GstElementClass *) g_class;
  GstAudioEventEmitterClass *scope_class =
      (GstAudioEventEmitterClass *) g_class;

  gobject_class->finalize = gst_power_event_emitter_finalize;

  gst_element_class_set_static_metadata (element_class,
      "Spectrogram scope", "Visualization",
      "Simple spectrogram scope", "Leon Bonde Larsen <leon@bondelarsen.dk>");

  gst_element_class_add_static_pad_template (element_class,
      &gst_power_event_emitter_src_template);
  gst_element_class_add_static_pad_template (element_class,
      &gst_power_event_emitter_sink_template);

  gobject_class->set_property = gst_power_event_emitter_set_property;
  gobject_class->get_property = gst_power_event_emitter_get_property;

  scope_class->setup = GST_DEBUG_FUNCPTR (gst_power_event_emitter_setup);
  scope_class->render = GST_DEBUG_FUNCPTR (gst_power_event_emitter_render);

  g_object_class_install_property (gobject_class, PROP_COLORMAP,
      g_param_spec_enum ("colormap", "colormap",
          "Colormap for the spectrogram scope display.",
          GST_TYPE_POWER_EVENT_EMITTER_STYLE, COLORMAP_GREY,
          G_PARAM_CONSTRUCT | G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

}

static void
gst_power_event_emitter_init (GstPowerEventEmitter * scope)
{
}

static void
gst_power_event_emitter_finalize (GObject * object)
{
  GstPowerEventEmitter *scope = GST_POWER_EVENT_EMITTER (object);

  if (scope->fft_ctx) {
    gst_fft_s16_free (scope->fft_ctx);
    scope->fft_ctx = NULL;
  }
  if (scope->freq_data) {
    g_free (scope->freq_data);
    scope->freq_data = NULL;
  }

  G_OBJECT_CLASS (gst_power_event_emitter_parent_class)->finalize (object);
}

static gboolean
gst_power_event_emitter_setup (GstAudioEventEmitter * bscope)
{
  GstPowerEventEmitter *scope = GST_POWER_EVENT_EMITTER (bscope);
  guint video_width, video_height;

  video_width = GST_VIDEO_INFO_WIDTH (&bscope->vinfo);
  video_height = GST_VIDEO_INFO_HEIGHT (&bscope->vinfo);

  // Calculate number of samples needed per render() call
  bscope->req_spf = video_height * 2 - 2;

  // Allocate GstFFTS16 and GstFFTS16Complex objects
  if (scope->fft_ctx)
    gst_fft_s16_free (scope->fft_ctx);
  g_free (scope->freq_data);

  scope->fft_ctx = gst_fft_s16_new (bscope->req_spf, FALSE);
  scope->freq_data = g_new (GstFFTS16Complex, video_width + 1);

  // Allocate memory for holding the fft data for the spectrogram
  scope->fft_array =
      gst_allocator_alloc (NULL, video_width * video_height, NULL);
  gst_memory_map (scope->fft_array, &scope->fft_array_info,
      GST_MAP_READ | GST_MAP_WRITE);

  // Allocate memory for holding the power data
  scope->power_value_array = malloc (video_width * sizeof (guint32));

  // Allocate memory for holding the entropy scores
  scope->entropy_scores = malloc (video_width * sizeof (gfloat));

  // Set initial value for power max
  scope->power_max = 8000;

  // Set initial state for event generation
  scope->above_threshold = FALSE;

  return TRUE;
}

static gint32
gst_power_event_emitter_greymap (gint intensity)
{
  // Set all three colors to the current intensity
  return (intensity << 16) | (intensity << 8) | intensity;
}

static gint32
gst_power_event_emitter_colormap (gint color)
{
  guint red, green, blue;
  static gfloat normalised_input, color_number, group, residue;

  // Update max value so far
  static gint max = 1;
  if (color > max) {
    max = color;
    //printf ("New max = %d\n", max);
  }
  // Normalise input and find the color number, color group and the residue
  normalised_input = (gfloat) color / (gfloat) max;
  color_number = normalised_input / 0.25;
  group = floor (color_number);
  residue = floor (255 * (color_number - group));

  // Find color mix from group and residue
  switch ((gint) group) {
    case 0:                    //black to yellow
      red = residue;
      green = residue;
      blue = 0;
      break;
    case 1:                    //yellow to red
      red = 255;
      green = 255 - residue;
      blue = 0;
      break;
    case 2:                    //red to magenta
      red = 255;
      green = 0;
      blue = residue;
      break;
    case 3:                    //magenta to white
      red = 255;
      green = residue;
      blue = 255;
      break;
    default:
      red = 255;
      green = 255;
      blue = 255;
      break;
  }

  // Return 32-bit color
  return (red << 16) | (green << 8) | blue;
}

static gboolean
gst_power_event_emitter_render (GstAudioEventEmitter * bscope,
    GstBuffer * audio, GstVideoFrame * video)
{
  GstPowerEventEmitter *scope = GST_POWER_EVENT_EMITTER (bscope);

  gint16 *mono_adata;
  GstFFTS16Complex *fdata = scope->freq_data;

  guint x = 0, y = 0, x_ptr = 0, index = 0;
  guint video_width = GST_VIDEO_INFO_WIDTH (&bscope->vinfo);
  guint video_height = GST_VIDEO_INFO_HEIGHT (&bscope->vinfo) - 1;
  gfloat fr, fi, power, fft_intensity;
  GstMapInfo amap;
  guint32 *vdata;
  guint32 off = 0;
  gfloat entropy_score = 0;
  gint channels;
  static guint32 collumn_pointer = 0;

  // Map the video data
  vdata = (guint32 *) GST_VIDEO_FRAME_PLANE_DATA (video, 0);

  // Map the audio data
  gst_buffer_map (audio, &amap, GST_MAP_READ);
  mono_adata = (gint16 *) g_memdup (amap.data, amap.size);

  // If multiple channels, deinterleave and mixdown adata
  channels = GST_AUDIO_INFO_CHANNELS (&bscope->ainfo);
  if (channels > 1) {
    guint ch = channels;
    guint num_samples = amap.size / (ch * sizeof (gint16));
    guint i, c, v, s = 0;

    for (i = 0; i < num_samples; i++) {
      v = 0;
      for (c = 0; c < ch; c++) {
        v += mono_adata[s++];
      }
      mono_adata[i] = v / ch;
    }
  }
  // Run fft
  gst_fft_s16_window (scope->fft_ctx, mono_adata, GST_FFT_WINDOW_HAMMING);
  gst_fft_s16_fft (scope->fft_ctx, mono_adata, fdata);
  g_free (mono_adata);

  // Increment the pointer to the most resent collumn in the spectrogram
  collumn_pointer++;
  if (collumn_pointer == video_width) {
    collumn_pointer = 0;
  }
  // For each bin in the fft calculate intensity and push to fft_array
  scope->power_in_collumn = 0;
  scope->geometric_mean = 1;
  scope->aritmetic_mean = 0;
  for (y = 0; y < video_height; y++) {
    fr = (gfloat) fdata[1 + y].r / 512.0;
    fi = (gfloat) fdata[1 + y].i / 512.0;
    index = (y * video_width) + collumn_pointer;
    fft_intensity = (guint) (video_height * sqrt (fr * fr + fi * fi));
    scope->fft_array_info.data[index] = fft_intensity;

    // Calculate power between 1 and 20 kHz
    if (y > (video_height / 48.0) && y < (video_height * 20) / 48.0) {
      scope->power_in_collumn += fft_intensity;

    }
    // Calculate means
    scope->geometric_mean *= fft_intensity;
    scope->aritmetic_mean += fft_intensity;
  }

  // Calculate entropy
  scope->geometric_mean = pow (scope->geometric_mean, 1 / video_height);
  scope->aritmetic_mean /= video_height;
  scope->entropy = scope->geometric_mean / scope->aritmetic_mean;
  //printf("Entropy: %f\n", scope->entropy);

  // Save entropy as number between 0 and 1
  if (scope->entropy > 1) {
    scope->entropy = 1;
  }
  if (scope->entropy < 0) {
    scope->entropy = 0;
  }
  scope->entropy_scores[collumn_pointer] = scope->entropy;

  // Save power
  scope->power_value_array[collumn_pointer] = scope->power_in_collumn;

  // Update power max value
  if (scope->power_in_collumn > scope->power_max) {
    scope->power_max = scope->power_in_collumn;
    //printf ("Power max: %f\n", scope->power_max);
  }
  // For each bin in the spectrogram update the corresponding pixel in vdata
  for (x = 0; x < video_width; x++) {
    x_ptr = (collumn_pointer + x + 1) % video_width;
    for (y = 0; y < video_height; y++) {
      off = ((video_height - y - 1) * video_width) + x;
      index = (y * video_width) + x_ptr;
      vdata[off] =
          (*scope->colormap_function) (scope->fft_array_info.data[index]);

      // Plot power
      power =
          (((gfloat) scope->power_value_array[x_ptr]) * video_height) /
          scope->power_max;
      if (power >= video_height) {
        power = video_height - 1;
      }
      if (power < 0) {
        power = 0;
      }
      off = ((video_height - (guint32) power - 1) * (video_width)) + x;
      vdata[off] = 0x00FF0000;  // Red

      // Plot entropy
      entropy_score = ((1 - scope->entropy_scores[x_ptr]) * (video_height - 1));
      off = ((video_height - (guint32) entropy_score - 1) * (video_width)) + x;
      vdata[off] = 0x0000FF00;  // Green

      // Plot events
      if (scope->power_value_array[x_ptr] > (scope->power_max / 16)) {
        vdata[x] = 0x00FF00FF;  // Magenta
      }

      if (entropy_score > 0.9) {
        vdata[(video_width * 2) + x] = 0x00FFFF00;      // Yellow
      }

      if (entropy_score > 0.9
          && scope->power_value_array[x_ptr] > (scope->power_max / 16)) {
        vdata[(video_width * 4) + x] = 0x00FFFFFF;      // White
      }

    }
  }

  // Unmap the audio data
  gst_buffer_unmap (audio, &amap);

  return TRUE;
}

static void
gst_power_event_emitter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPowerEventEmitter *scope = GST_POWER_EVENT_EMITTER (object);

  switch (prop_id) {
    case PROP_COLORMAP:
      scope->colormap = g_value_get_enum (value);
      switch (scope->colormap) {
        case COLORMAP_GREY:
          scope->colormap_function = (void *) gst_power_event_emitter_greymap;
          break;
        case COLORMAP_GOLD:
          scope->colormap_function = (void *) gst_power_event_emitter_colormap;
          break;
      }
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static void
gst_power_event_emitter_get_property (GObject * object, guint prop_id,
    GValue * value, GParamSpec * pspec)
{
  GstPowerEventEmitter *scope = GST_POWER_EVENT_EMITTER (object);

  switch (prop_id) {
    case PROP_COLORMAP:
      g_value_set_enum (value, scope->colormap);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}


gboolean
gst_power_event_emitter_plugin_init (GstPlugin * plugin)
{
  GST_DEBUG_CATEGORY_INIT (power_event_emitter_debug, "powereventemitter", 0,
      "powereventemitter");

  return gst_element_register (plugin, "powereventemitter", GST_RANK_NONE,
      GST_TYPE_POWER_EVENT_EMITTER);
}
