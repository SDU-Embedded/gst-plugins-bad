/* GStreamer
 * Copyright (C) <2011> Stefan Kost <ensonic@users.sf.net>
 *
/ * gstpowereventemitter.h: simple oscilloscope
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

#ifndef __GST_POWER_EVENT_EMITTER_H__
#define __GST_POWER_EVENT_EMITTER_H__

#include "gst/pbutils/gstaudioeventemitter.h"
#include <gst/fft/gstffts16.h>
#include <stdio.h>

G_BEGIN_DECLS
#define GST_TYPE_POWER_EVENT_EMITTER            (gst_power_event_emitter_get_type())
#define GST_POWER_EVENT_EMITTER(obj)            (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_POWER_EVENT_EMITTER,GstPowerEventEmitter))
#define GST_POWER_EVENT_EMITTER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_POWER_EVENT_EMITTER,GstPowerEventEmitterClass))
#define GST_IS_POWER_EVENT_EMITTER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_POWER_EVENT_EMITTER))
#define GST_IS_POWER_EVENT_EMITTER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_POWER_EVENT_EMITTER))
typedef struct _GstPowerEventEmitter GstPowerEventEmitter;
typedef struct _GstPowerEventEmitterClass GstPowerEventEmitterClass;

struct _GstPowerEventEmitter
{
  GstAudioEventEmitter parent;

  GstFFTS16 *fft_ctx;
  GstFFTS16Complex *freq_data;

  gint colormap;
    gint32 (*colormap_function) (guint);

  GstMemory *fft_array;
  GstMapInfo fft_array_info;

  gfloat power_max;
  gfloat power_in_collumn;
  guint32 *power_value_array;
  gfloat *similarity_scores;

  gboolean above_threshold;
  guint32 hysteresis_time;
  guint32 event_counter;

  gfloat geometric_mean;
  gfloat aritmetic_mean;
  gfloat entropy;
  gfloat *entropy_scores;

};

struct _GstPowerEventEmitterClass
{
  GstAudioEventEmitterClass parent_class;
};

GType gst_power_event_emitter_get_type (void);
gboolean gst_power_event_emitter_plugin_init (GstPlugin * plugin);

G_END_DECLS
#endif /* __GST_POWER_EVENT_EMITTER_H__ */
