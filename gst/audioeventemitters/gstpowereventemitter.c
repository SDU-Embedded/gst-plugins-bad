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
  PROP_SILENT
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
        "rate = (int) [ 8000, 96000 ], "
        "channels = (int) 1, " "channel-mask = (bitmask) 0x3")
    );

static GstStaticPadTemplate src_factory = GST_STATIC_PAD_TEMPLATE ("src",
    GST_PAD_SRC,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("ANY")
    );

#define gst_power_event_emitter_parent_class parent_class
G_DEFINE_TYPE (GstPowerEventEmitter, gst_power_event_emitter, GST_TYPE_ELEMENT);

static void gst_power_event_emitter_set_property (GObject * object,
    guint prop_id, const GValue * value, GParamSpec * pspec);
static void gst_power_event_emitter_get_property (GObject * object,
    guint prop_id, GValue * value, GParamSpec * pspec);
static gboolean gst_power_event_emitter_sink_event (GstPad * pad,
    GstObject * parent, GstEvent * event);
static GstFlowReturn gst_power_event_emitter_chain (GstPad * pad,
    GstObject * parent, GstBuffer * buf);
gfloat gst_power_event_emitter_check (GstObject * parent, gint16 * audio_data);

/* GObject vmethod implementations */

/* initialize the emitter's class */
static void
gst_power_event_emitter_class_init (GstPowerEventEmitterClass * klass)
{
  GObjectClass *gobject_class;
  GstElementClass *gstelement_class;

  gobject_class = (GObjectClass *) klass;
  gstelement_class = (GstElementClass *) klass;

  gobject_class->set_property = gst_power_event_emitter_set_property;
  gobject_class->get_property = gst_power_event_emitter_get_property;

  g_object_class_install_property (gobject_class, PROP_SILENT,
      g_param_spec_boolean ("silent", "Silent", "Produce verbose output ?",
          FALSE, G_PARAM_READWRITE));

  gst_element_class_set_details_simple (gstelement_class,
      "Plugin",
      "FIXME:Generic",
      "FIXME:Generic Template Element", "AUTHOR_NAME AUTHOR_EMAIL");

  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&src_factory));
  gst_element_class_add_pad_template (gstelement_class,
      gst_static_pad_template_get (&sink_factory));
}

/* initialize the new element
 * instantiate pads and add them to element
 * set pad calback functions
 * initialize instance structure
 */
static void
gst_power_event_emitter_init (GstPowerEventEmitter * event_emitter_handle)
{
  // Init class variables
  event_emitter_handle->number_of_bins = 200;
  event_emitter_handle->len =
      gst_fft_next_fast_length ((2 * event_emitter_handle->number_of_bins) - 2);
  event_emitter_handle->fft_array =
      gst_allocator_alloc (NULL, event_emitter_handle->number_of_bins, NULL);
  gst_memory_map (event_emitter_handle->fft_array,
      &event_emitter_handle->fft_array_info, GST_MAP_READ | GST_MAP_WRITE);
  event_emitter_handle->samples_per_fft = event_emitter_handle->len;    //TODO: Combine

  // Init sink
  event_emitter_handle->sinkpad =
      gst_pad_new_from_static_template (&sink_factory, "sink");
  gst_pad_set_event_function (event_emitter_handle->sinkpad,
      GST_DEBUG_FUNCPTR (gst_power_event_emitter_sink_event));
  gst_pad_set_chain_function (event_emitter_handle->sinkpad,
      GST_DEBUG_FUNCPTR (gst_power_event_emitter_chain));
  GST_PAD_SET_PROXY_CAPS (event_emitter_handle->sinkpad);
  gst_element_add_pad (GST_ELEMENT (event_emitter_handle),
      event_emitter_handle->sinkpad);

  // Init source
  event_emitter_handle->srcpad =
      gst_pad_new_from_static_template (&src_factory, "src");
  GST_PAD_SET_PROXY_CAPS (event_emitter_handle->srcpad);
  gst_element_add_pad (GST_ELEMENT (event_emitter_handle),
      event_emitter_handle->srcpad);

  // Allocate GstFFTS16 and GstFFTS16Complex objects
  if (event_emitter_handle->fft_ctx)
    gst_fft_s16_free (event_emitter_handle->fft_ctx);
  g_free (event_emitter_handle->freq_data);

  event_emitter_handle->fft_ctx =
      gst_fft_s16_new (event_emitter_handle->len, FALSE);
  event_emitter_handle->freq_data =
      g_new (GstFFTS16Complex, (event_emitter_handle->len / 2) + 1);
}

static void
gst_power_event_emitter_set_property (GObject * object, guint prop_id,
    const GValue * value, GParamSpec * pspec)
{
  //GstPowerEventEmitter *filter = GST_POWER_EVENT_EMITTER (object);

  switch (prop_id) {
    case PROP_SILENT:
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
  //GstPowerEventEmitter *filter = GST_POWER_EVENT_EMITTER (object);

  switch (prop_id) {
    case PROP_SILENT:
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
  }
}

/* GstElement vmethod implementations */

/* this function handles sink events */
static gboolean
gst_power_event_emitter_sink_event (GstPad * pad, GstObject * parent,
    GstEvent * event)
{
  GstPowerEventEmitter *filter;
  gboolean ret;

  filter = GST_POWER_EVENT_EMITTER (parent);

  GST_LOG_OBJECT (filter, "Received %s event: %" GST_PTR_FORMAT,
      GST_EVENT_TYPE_NAME (event), event);

  switch (GST_EVENT_TYPE (event)) {
    case GST_EVENT_CAPS:
    {
      GstCaps *caps;

      gst_event_parse_caps (event, &caps);
      /* do something with the caps */

      /* and forward */
      ret = gst_pad_event_default (pad, parent, event);
      break;
    }
    default:
      ret = gst_pad_event_default (pad, parent, event);
      break;
  }
  return ret;
}

/* chain function
 * this function does the actual processing
 */
static GstFlowReturn
gst_power_event_emitter_chain (GstPad * pad, GstObject * parent,
    GstBuffer * input_buffer)
{
  // Variables for input buffer
  GstMapInfo audio_map;

  // Variables for output buffer
  GstBuffer *text_buffer;
  GstMemory *text_memory;
  GstMapInfo text_map;

  // Variables for FFT
  gint16 *audio_data;

  // Variables general
  guint sample;
  gfloat power;
  GstFlowReturn return_value;
  GstPowerEventEmitter *event_emitter_handle;

  // Parse object
  event_emitter_handle = GST_POWER_EVENT_EMITTER (parent);

  // Prepare input buffer
  gst_buffer_map (input_buffer, &audio_map, GST_MAP_READ);

  // Pass chunks of data to check TODO: Currently skips samples that exeed largest multiple of samples_per_fft
  power = 0;
  for (sample = 0;
      (sample + event_emitter_handle->samples_per_fft) < audio_map.size;
      sample += event_emitter_handle->samples_per_fft) {

    // Copy data
    audio_data =
        (gint16 *) g_memdup (audio_map.data + sample,
        event_emitter_handle->samples_per_fft);

    // Check data
    power += gst_power_event_emitter_check (parent, audio_data);

    // Free data
    g_free (audio_data);
  }

  // Prepare output buffer
  text_buffer = gst_buffer_new ();
  text_memory = gst_allocator_alloc (NULL, 100, NULL);
  gst_buffer_append_memory (text_buffer, text_memory);

  // Fill output buffer
  gst_buffer_map (text_buffer, &text_map, GST_MAP_WRITE);
  if (power > 500)
    sprintf ((char *) text_map.data, "On\n");
  else
    sprintf ((char *) text_map.data, "Off\n");
  gst_buffer_unmap (text_buffer, &text_map);

  // Push the buffer to the src pad
  return_value = gst_pad_push (event_emitter_handle->srcpad, text_buffer);

  // Clean up
  gst_buffer_unmap (input_buffer, &audio_map);

  return return_value;
}

gfloat
gst_power_event_emitter_check (GstObject * parent, gint16 * audio_data)
{
  // Variables
  guint bin;
  gfloat f_real, f_imaginary, power;
  GstPowerEventEmitter *event_emitter_handle;
  GstFFTS16Complex *fdata;

  // Parse object
  event_emitter_handle = GST_POWER_EVENT_EMITTER (parent);

  // Point to pre-allocated memory
  fdata = event_emitter_handle->freq_data;

  // Run fft
  gst_fft_s16_window (event_emitter_handle->fft_ctx, audio_data,
      GST_FFT_WINDOW_HAMMING);
  gst_fft_s16_fft (event_emitter_handle->fft_ctx, audio_data, fdata);

  // For each bin in the fft calculate intensity and add to power
  power = 0;
  for (bin = 0; bin < event_emitter_handle->number_of_bins; bin++) {
    f_real = (gfloat) fdata[1 + bin].r / 512.0;
    f_imaginary = (gfloat) fdata[1 + bin].i / 512.0;
    power += sqrt (f_real * f_real + f_imaginary * f_imaginary);
  }

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
