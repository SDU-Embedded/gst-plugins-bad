/*
 * GStreamer
 * Copyright (C) 2005 Thomas Vander Stichele <thomas@apestaart.org>
 * Copyright (C) 2005 Ronald S. Bultje <rbultje@ronald.bitfreak.net>
 * Copyright (C) YEAR AUTHOR_NAME AUTHOR_EMAIL
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 *
 * Alternatively, the contents of this file may be used under the
 * GNU Lesser General Public License Version 2.1 (the "LGPL"), in
 * which case the following provisions apply instead of the ones
 * mentioned above:
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

/**
 * SECTION:element-emitter
 *
 * FIXME:Describe emitter here.
 *
 * <refsect2>
 * <title>Example launch line</title>
 * |[
 * gst-launch -v -m fakesrc ! emitter ! fakesink silent=TRUE
 * ]|
 * </refsect2>
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <gst/gst.h>
#include "gstpowereventemitter.h"

GST_DEBUG_CATEGORY_STATIC (gst_power_event_emitter_debug);
#define GST_CAT_DEFAULT gst_power_event_emitter_debug

/* Filter signals and args */
enum
{
  /* FILL ME */
  LAST_SIGNAL
};

enum
{
  PROP_0,
  PROP_WINDOW_SIZE,
  PROP_WINDOW_FUNCTION,
  PROP_OVERLAP,
  PROP_NUMBER_OF_BINS,
  PROP_THRESHOLD_LOW,
  PROP_THRESHOLD_HIGH,
  PROP_START_MAX_VALUE
};

// Enumeration of the options for the window function property
enum
{
  WINDOW_FUNCTION_RECTANGULAR,
  WINDOW_FUNCTION_HAMMING,
  WINDOW_FUNCTION_HANN,
  WINDOW_FUNCTION_BARTLETT,
  WINDOW_FUNCTION_BLACKMAN
};

/* the capabilities of the inputs and outputs.
 *
 * describe the real formats here.
 */
static GstStaticPadTemplate sink_factory = GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw, "
        "format = (string) S16LE, "
        "layout = (string) interleaved, "
        "rate = (int) [ 8000, 96000 ], " "channels = (int) 1")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_power_event_emitter_object_class object_class
G_DEFINE_TYPE (GstPowerEventEmitter, gst_power_event_emitter, GST_TYPE_ELEMENT);

// Function declarations
static void gst_power_event_emitter_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_power_event_emitter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static GstFlowReturn gst_power_event_emitter_chain (GstPad * pad,
    GstObject * object, GstBuffer * buf);
static gfloat gst_power_event_emitter_get_power (GstPowerEventEmitter *
    object_handle, gint16 * audio_data);
static GstFlowReturn gst_power_event_emitter_push_event (GstPowerEventEmitter *
    object_handle);
static void updateThresholds (GstPowerEventEmitter * object_handle);

// Functions
static GType
gst_power_event_emitter_window_function_get_type (void)
{
  static GType gtype = 0;

  if (gtype == 0) {
    static const GEnumValue values[] = {
      {WINDOW_FUNCTION_RECTANGULAR, "Rectangular window (default)",
          "rectangular"},
      {WINDOW_FUNCTION_HAMMING, "Hamming window", "Hamming"},
      {WINDOW_FUNCTION_HANN, "Hann window", "Hann"},
      {WINDOW_FUNCTION_BARTLETT, "Bartlett window", "Bartlett"},
      {WINDOW_FUNCTION_BLACKMAN, "Blackman window", "Blackman"},
      {0, NULL, NULL}
    };

    gtype =
        g_enum_register_static ("GstPowerEventEmitterWindowFunctionTypes",
        values);
  }
  return gtype;
}


static void
gst_power_event_emitter_class_init (GstPowerEventEmitterClass * klass)
{
  // Variables
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  // Init object
  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  // Init callbacks for set and get properties
  gobject_class->set_property = gst_power_event_emitter_set_property;
  gobject_class->get_property = gst_power_event_emitter_get_property;

  // Install properties
  g_object_class_install_property (gobject_class, PROP_WINDOW_SIZE,
      g_param_spec_uint ("window_size", "Window size",
          "Sets the window size for the FFT", 100, 1000, 200,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_WINDOW_FUNCTION,
      g_param_spec_enum ("window_function", "Window function",
          "Sets the window function for the FFT",
          gst_power_event_emitter_window_function_get_type (),
          WINDOW_FUNCTION_RECTANGULAR, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_OVERLAP,
      g_param_spec_uint ("overlap", "Overlap",
          "Sets the overlap between FFT windows", 0, 100, 0,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_THRESHOLD_HIGH,
      g_param_spec_float ("threshold_high", "Threshold_high",
          "Sets the percentage threshold for onset (deprecated)", 0.0, 100.0,
          5.0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_THRESHOLD_LOW,
      g_param_spec_float ("threshold_low", "Threshold_low",
          "Sets the percentage threshold for offset (deprecated)", 0.0, 100.0,
          1.0, G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_START_MAX_VALUE,
      g_param_spec_float ("start_max_value", "Start max value",
          "Sets the start max value (deprecated)", 0.0, 1000000.0, 0.0,
          G_PARAM_READWRITE));

  g_object_class_install_property (gobject_class, PROP_NUMBER_OF_BINS,
      g_param_spec_uint ("bins", "Number of bins",
          "Sets the number of frequency bins in FFT", 50, 1000, 100,
          G_PARAM_READWRITE));

  // Set metadata
  gst_element_class_set_static_metadata (gstelement_class,
      "Power event emitter", "Feature extraction",
      "Event emitter based on power threshold",
      "Leon Bonde Larsen <leon@bondelarsen.dk>");

  // Init pads
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

static void
gst_power_event_emitter_init (GstPowerEventEmitter * object_handle)
{
  // Init class variables
  object_handle->window_size = 0;
  object_handle->window_function = 0;
  object_handle->overlap = 0;
  object_handle->number_of_bins = 200;
  object_handle->threshold_percentage_high = 0.0;
  object_handle->threshold_percentage_low = 0.0;
  object_handle->samples_per_fft =
      gst_fft_next_fast_length ((2 * object_handle->number_of_bins) - 2);
  object_handle->in_event_state = FALSE;
  object_handle->low_threshold = 0.0;
  object_handle->high_threshold = 0.0;
  object_handle->power_max = 0.0;
  object_handle->current_power = 0.0;
  object_handle->previous_power = 0.0;
  object_handle->text_pre_buffer_length = 0;

  // Init sink
  object_handle->sinkpad =
      gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_chain_function (object_handle->sinkpad,
      GST_DEBUG_FUNCPTR (gst_power_event_emitter_chain));
  GST_PAD_SET_PROXY_CAPS (object_handle->sinkpad);
  gst_element_add_pad (GST_ELEMENT (object_handle), object_handle->sinkpad);

  // Init source
  object_handle->srcpad =
      gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (object_handle->srcpad);
  gst_element_add_pad (GST_ELEMENT (object_handle), object_handle->srcpad);

  // Allocate GstFFTS16 and GstFFTS16Complex objects
  if (object_handle->fft_ctx)
    gst_fft_s16_free (object_handle->fft_ctx);
  object_handle->fft_ctx =
      gst_fft_s16_new (object_handle->samples_per_fft, FALSE);

  if (object_handle->freq_data)
    g_free (object_handle->freq_data);
  object_handle->freq_data =
      g_new (GstFFTS16Complex, (object_handle->samples_per_fft / 2) + 1);

  // Print settings
  //g_print ("Window function: %d\n", object_handle->window_function);
  //g_print ("Overlap: %d\n", object_handle->overlap);
  //g_print ("Number of bins: %d\n", object_handle->number_of_bins);
  //g_print ("Window size(derived): %d\n", object_handle->samples_per_fft);
  //g_print ("Low threshold: %f\n", object_handle->threshold_percentage_low);
  //g_print ("High threshold: %f\n", object_handle->threshold_percentage_high);

}

static void
gst_power_event_emitter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  GstPowerEventEmitter *object_handle = GST_POWER_EVENT_EMITTER (object);

  switch (prop_id) {
    case PROP_WINDOW_SIZE:
      object_handle->window_size = g_value_get_uint (value);
      break;
    case PROP_WINDOW_FUNCTION:
      object_handle->window_function = g_value_get_uint (value);
      break;
    case PROP_OVERLAP:
      object_handle->overlap = g_value_get_uint (value);
      break;
    case PROP_NUMBER_OF_BINS:
      object_handle->number_of_bins = g_value_get_uint (value);
      break;
    case PROP_THRESHOLD_LOW:
      object_handle->threshold_percentage_low = g_value_get_float (value);
      updateThresholds (object_handle);
      break;
    case PROP_THRESHOLD_HIGH:
      object_handle->threshold_percentage_high = g_value_get_float (value);
      updateThresholds (object_handle);
      break;
    case PROP_START_MAX_VALUE:
      object_handle->power_max = g_value_get_float (value);
      updateThresholds (object_handle);
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
  GstPowerEventEmitter *object_handle = GST_POWER_EVENT_EMITTER (object);

  switch (prop_id) {
    case PROP_WINDOW_SIZE:
      g_value_set_uint (value, object_handle->window_size);
      break;
    case PROP_WINDOW_FUNCTION:
      g_value_set_uint (value, object_handle->window_function);
      break;
    case PROP_OVERLAP:
      g_value_set_uint (value, object_handle->overlap);
      break;
    case PROP_NUMBER_OF_BINS:
      g_value_set_uint (value, object_handle->number_of_bins);
      break;
    case PROP_THRESHOLD_LOW:
      g_value_set_float (value, object_handle->threshold_percentage_low);
      break;
    case PROP_THRESHOLD_HIGH:
      g_value_set_float (value, object_handle->threshold_percentage_high);
      break;
    case PROP_START_MAX_VALUE:
      g_value_set_float (value, object_handle->power_max);
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

static GstFlowReturn
gst_power_event_emitter_chain (GstPad * pad, GstObject * object,
    GstBuffer * input_buffer)
{
  // Variables for input buffer
  GstMapInfo audio_map;

  // Variables for FFT
  gint16 *audio_data;

  // Variables general
  guint sample;
  gfloat power;
  GstFlowReturn return_value;
  GstPowerEventEmitter *object_handle;
  return_value = GST_FLOW_ERROR;

  // Parse object
  object_handle = GST_POWER_EVENT_EMITTER (object);

  // Prepare input buffer
  gst_buffer_map (input_buffer, &audio_map, GST_MAP_READ);

  // Clear output buffer
  object_handle->text_pre_buffer[0] = '\0';

  // Pass chunks of data to check TODO: Currently skips samples that exeed largest multiple of samples_per_fft
  object_handle->current_power = 0;
  for (sample = 0;
      (sample + object_handle->samples_per_fft) < audio_map.size;
      sample += object_handle->samples_per_fft) {

    // Copy data
    audio_data = (gint16 *) g_memdup (audio_map.data + sample, object_handle->samples_per_fft * 2);     // x2 since size is given in 8-bit bytes

    // Check data
    object_handle->current_power +=
        gst_power_event_emitter_get_power (object_handle, audio_data);

    // Free data
    if (audio_data)
      g_free (audio_data);
  }

  // Running average
  power = (object_handle->current_power + object_handle->previous_power) / 2.0;
  object_handle->previous_power = object_handle->current_power;

  // Check for new maximum
  if (power > object_handle->power_max) {
    object_handle->power_max = power;
    updateThresholds (object_handle);
    //g_print ("Setting new max: %f Low: %f High: %f\n", object_handle->power_max,object_handle->threshold_percentage_low, object_handle->threshold_percentage_high);
  }
  //g_print("Power: %f Low: %f High: %f\n", power, object_handle->low_threshold, object_handle->high_threshold);
  //g_print("Power: %f\n", power);
  sprintf ((char *) object_handle->text_pre_buffer, "%f\n", power);

  // Fill output buffer
  //if ((object_handle->in_event_state) && (power < object_handle->low_threshold)) {
  //  object_handle->in_event_state = FALSE;
  //  //g_print("Offset case\n");
  //  sprintf ((char *) object_handle->text_pre_buffer, "offset\n");
  //} else if ((!object_handle->in_event_state)
  //    && (power > object_handle->high_threshold)) {
  //  object_handle->in_event_state = TRUE;
  //  //g_print("Onset case\n");
  //  sprintf ((char *) object_handle->text_pre_buffer, "onset\n");
  //}
  // Push event
  return_value = gst_power_event_emitter_push_event (object_handle);

  // Clean up
  gst_buffer_unmap (input_buffer, &audio_map);
  gst_buffer_unref (input_buffer);
  return return_value;
}

static void
updateThresholds (GstPowerEventEmitter * object_handle)
{
  object_handle->low_threshold =
      object_handle->power_max * object_handle->threshold_percentage_low /
      100.0;
  object_handle->high_threshold =
      object_handle->power_max * object_handle->threshold_percentage_high /
      100.0;
}

static GstFlowReturn
gst_power_event_emitter_push_event (GstPowerEventEmitter * object_handle)
{
  // Variables for output buffer
  GstBuffer *text_buffer;
  GstMapInfo text_map;
  guint i;
  GstFlowReturn ret = GST_FLOW_ERROR;

  // Check buffer length
  object_handle->text_pre_buffer_length =
      strlen (object_handle->text_pre_buffer);
  if (object_handle->text_pre_buffer_length) {

    // Prepare output buffer
    text_buffer =
        gst_buffer_new_allocate (NULL, object_handle->text_pre_buffer_length,
        NULL);

    // Fill output buffer
    gst_buffer_map (text_buffer, &text_map, GST_MAP_WRITE);
    for (i = 0; i <= object_handle->text_pre_buffer_length; i++) {
      text_map.data[i] = object_handle->text_pre_buffer[i];
    }
    gst_buffer_unmap (text_buffer, &text_map);

    // Push to src pad
    ret = gst_pad_push (object_handle->srcpad, text_buffer);

    return ret;
  } else {
    return GST_FLOW_OK;
  }

}

static gfloat
gst_power_event_emitter_get_power (GstPowerEventEmitter * object_handle,
    gint16 * audio_data)
{
  // Variables
  guint bin;
  gfloat f_real, f_imaginary, power;
  GstFFTS16Complex *fdata;

  // Point to pre-allocated memory
  fdata = object_handle->freq_data;

  // Run fft
  gst_fft_s16_window (object_handle->fft_ctx, audio_data,
      GST_FFT_WINDOW_HAMMING);
  gst_fft_s16_fft (object_handle->fft_ctx, audio_data, fdata);

  // For each bin in the fft calculate intensity and add to power
  power = 0;
  for (bin = 0; bin < object_handle->samples_per_fft; bin++) {
    f_real = (gfloat) fdata[1 + bin].r / 65536;
    f_imaginary = (gfloat) fdata[1 + bin].i / 65536;
    power += sqrt (f_real * f_real + f_imaginary * f_imaginary);
  }
  power = power / object_handle->samples_per_fft * 100;
  return power;
}


/* entry point to initialize the plug-in
 * initialize the plug-in itself
 * register the element factories and other features
 */
gboolean
gst_power_event_emitter_plugin_init (GstPlugin * plugin)
{
  /* debug category for fltering log messages
   *
   * exchange the string 'Template plugin' with your description
   */
  GST_DEBUG_CATEGORY_INIT (gst_power_event_emitter_debug, "powereventemitter",
      0, "Power event emitter");

  return gst_element_register (plugin, "powereventemitter", GST_RANK_NONE,
      GST_TYPE_POWER_EVENT_EMITTER);
}
