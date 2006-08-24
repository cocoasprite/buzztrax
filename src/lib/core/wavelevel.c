/* $Id: wavelevel.c,v 1.15 2006-08-24 20:00:52 ensonic Exp $
 *
 * Buzztard
 * Copyright (C) 2006 Buzztard team <buzztard-devel@lists.sf.net>
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
 * SECTION:btwavelevel
 * @short_description: a single part of a #BtWave item
 *
 * #BtWavelevel contain the digital audio data of a #BtWave to be used for a 
 * certain key-range.
 */ 
 
#define BT_CORE
#define BT_WAVELEVEL_C

#include <libbtcore/core.h>

//-- property ids

enum {
  WAVELEVEL_SONG=1,
  WAVELEVEL_ROOT_NOTE,
  WAVELEVEL_LENGTH,
  WAVELEVEL_LOOP_START,
  WAVELEVEL_LOOP_END,
  WAVELEVEL_RATE
};

struct _BtWavelevelPrivate {
  /* used to validate if dispose has run */
  gboolean dispose_has_run;
  
  /* the song the wavelevel belongs to */
  union {
    BtSong *song;
    gpointer song_ptr;
  };
  
  /* the wave the wavelevel belongs to */
  BtWave *wave;

  /* the keyboard note associated to this sample */
  guchar root_note;
  /* the number of samples */
  gulong length;
  /* the loop markers, -1 means no loop */
  glong loop_start,loop_end;
  /* the sampling rate */
  gulong rate;
  
  // data format

  gpointer *sample;    // sample data
};

static GObjectClass *parent_class=NULL;

//-- constructor methods

/**
 * bt_wavelevel_new:
 * @song: the song the new instance belongs to
 * @wave: the wave the new wavelevel belongs to
 * @root_note: the keyboard note this sample is related
 * @length: the number of samples
 * @loop_start: the start of the loop
 * @loop_end: the end of the loop
 * @rate: the sampling rate
 *
 * Create a new instance
 *
 * Returns: the new instance or NULL in case of an error
 */
BtWavelevel *bt_wavelevel_new(const BtSong *song,const BtWave *wave,guchar root_note,gulong length,glong loop_start,glong loop_end,gulong rate) {
  BtWavelevel *self;

  g_assert(BT_IS_SONG(song));
  g_assert(BT_IS_WAVE(wave));

  if(!(self=BT_WAVELEVEL(g_object_new(BT_TYPE_WAVELEVEL,"song",song,
    "root-note",root_note,"length",length,"loop_start",loop_start,
    "loop_end",loop_end,"rate",rate,NULL)))
  ) {
    goto Error;
  }
  // add the wavelevel to the wave
  bt_wave_add_wavelevel(wave,self);
  return(self);
Error:
  g_object_try_unref(self);
  return(NULL);
}

//-- private methods

//-- public methods


//-- io interface

static xmlNodePtr bt_wavelevel_persistence_save(BtPersistence *persistence, xmlNodePtr parent_node, BtPersistenceSelection *selection) {
  BtWavelevel *self = BT_WAVELEVEL(persistence);
  xmlNodePtr node=NULL;
  
  GST_DEBUG("PERSISTENCE::wavelevel");

  if((node=xmlNewChild(parent_node,NULL,XML_CHAR_PTR("wavelevel"),NULL))) {
    xmlNewProp(node,XML_CHAR_PTR("root-note"),XML_CHAR_PTR(bt_persistence_strfmt_uchar(self->priv->root_note)));
    xmlNewProp(node,XML_CHAR_PTR("length"),XML_CHAR_PTR(bt_persistence_strfmt_ulong(self->priv->length)));
    xmlNewProp(node,XML_CHAR_PTR("rate"),XML_CHAR_PTR(bt_persistence_strfmt_ulong(self->priv->rate)));
    xmlNewProp(node,XML_CHAR_PTR("loop-start"),XML_CHAR_PTR(bt_persistence_strfmt_long(self->priv->loop_start)));
    xmlNewProp(node,XML_CHAR_PTR("loop-end"),XML_CHAR_PTR(bt_persistence_strfmt_long(self->priv->loop_end)));
  }
  return(node);
}

static gboolean bt_wavelevel_persistence_load(BtPersistence *persistence, xmlNodePtr node, BtPersistenceLocation *location) {
  //BtWavelevel *self = BT_WAVELEVEL(persistence);
  gboolean res=FALSE;
  
  // @todo: implement me
  res=TRUE;
  return(res);
}

static void bt_wavelevel_persistence_interface_init(gpointer g_iface, gpointer iface_data) {
  BtPersistenceInterface *iface = g_iface;
  
  iface->load = bt_wavelevel_persistence_load;
  iface->save = bt_wavelevel_persistence_save;
}

//-- wrapper

//-- class internals

/* returns a property for the given property_id for this object */
static void bt_wavelevel_get_property(GObject      *object,
                               guint         property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  BtWavelevel *self = BT_WAVELEVEL(object);
  return_if_disposed();
  switch (property_id) {
    case WAVELEVEL_SONG: {
      g_value_set_object(value, self->priv->song);
    } break;
    case WAVELEVEL_ROOT_NOTE: {
      g_value_set_uchar(value, self->priv->root_note);
    } break;
    case WAVELEVEL_LENGTH: {
      g_value_set_ulong(value, self->priv->length);
    } break;
    case WAVELEVEL_LOOP_START: {
      g_value_set_long(value, self->priv->loop_start);
    } break;
    case WAVELEVEL_LOOP_END: {
      g_value_set_long(value, self->priv->loop_end);
    } break;
    case WAVELEVEL_RATE: {
      g_value_set_ulong(value, self->priv->rate);
    } break;
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

/* sets the given properties for this object */
static void bt_wavelevel_set_property(GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BtWavelevel *self = BT_WAVELEVEL(object);
  return_if_disposed();
  switch (property_id) {
    case WAVELEVEL_SONG: {
      g_object_try_weak_unref(self->priv->song);
      self->priv->song = BT_SONG(g_value_get_object(value));
      g_object_try_weak_ref(self->priv->song);
      //GST_DEBUG("set the song for wavelevel: %p",self->priv->song);
    } break;
     case WAVELEVEL_ROOT_NOTE: {
      self->priv->root_note = g_value_get_uchar(value);
      GST_DEBUG("set the root-note for wavelevel: %d",self->priv->root_note);
    } break;
     case WAVELEVEL_LENGTH: {
      self->priv->length = g_value_get_ulong(value);
      GST_DEBUG("set the length for wavelevel: %d",self->priv->length);
    } break;
     case WAVELEVEL_LOOP_START: {
      self->priv->loop_start = g_value_get_long(value);
      GST_DEBUG("set the loop-start for wavelevel: %d",self->priv->loop_start);
    } break;
     case WAVELEVEL_LOOP_END: {
      self->priv->loop_end = g_value_get_long(value);
      GST_DEBUG("set the loop-start for wavelevel: %d",self->priv->loop_start);
    } break;
     case WAVELEVEL_RATE: {
      self->priv->rate = g_value_get_ulong(value);
      GST_DEBUG("set the rate for wavelevel: %d",self->priv->rate);
    } break;
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

static void bt_wavelevel_dispose(GObject *object) {
  BtWavelevel *self = BT_WAVELEVEL(object);

  return_if_disposed();
  self->priv->dispose_has_run = TRUE;

  GST_DEBUG("!!!! self=%p",self);

  g_object_try_weak_unref(self->priv->song);

  if(G_OBJECT_CLASS(parent_class)->dispose) {
    (G_OBJECT_CLASS(parent_class)->dispose)(object);
  }
}

static void bt_wavelevel_finalize(GObject *object) {
  BtWavelevel *self = BT_WAVELEVEL(object);

  GST_DEBUG("!!!! self=%p",self);

  g_free(self->priv->sample);

  if(G_OBJECT_CLASS(parent_class)->finalize) {
    (G_OBJECT_CLASS(parent_class)->finalize)(object);
  }
}

static void bt_wavelevel_init(GTypeInstance *instance, gpointer g_class) {
  BtWavelevel *self = BT_WAVELEVEL(instance);
  
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, BT_TYPE_WAVELEVEL, BtWavelevelPrivate);
}

static void bt_wavelevel_class_init(BtWavelevelClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  parent_class=g_type_class_peek_parent(klass);
  g_type_class_add_private(klass,sizeof(BtWavelevelPrivate));

  gobject_class->set_property = bt_wavelevel_set_property;
  gobject_class->get_property = bt_wavelevel_get_property;
  gobject_class->dispose      = bt_wavelevel_dispose;
  gobject_class->finalize     = bt_wavelevel_finalize;

  g_object_class_install_property(gobject_class,WAVELEVEL_SONG,
                                  g_param_spec_object("song",
                                     "song contruct prop",
                                     "Set song object, the wavelevel belongs to",
                                     BT_TYPE_SONG, /* object type */
                                     G_PARAM_CONSTRUCT_ONLY |G_PARAM_READWRITE));

  // @idea make this an own type
  g_object_class_install_property(gobject_class,WAVELEVEL_ROOT_NOTE,
                                  g_param_spec_uchar("root-note",
                                     "root-note prop",
                                     "the base note associated with the sample",
                                     0,
                                     G_MAXUINT8,
                                     0,
                                     G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,WAVELEVEL_LENGTH,
                                  g_param_spec_ulong("length",
                                     "length prop",
                                     "length of the sample",
                                     0,
                                     G_MAXLONG,  // loop-pos are LONG as well
                                     0,
                                     G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,WAVELEVEL_LOOP_START,
                                  g_param_spec_long("loop-start",
                                     "loop-start prop",
                                     "start of the sample loop",
                                     -1,
                                     G_MAXLONG,
                                     -1,
                                     G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,WAVELEVEL_LOOP_END,
                                  g_param_spec_long("loop-end",
                                     "loop-end prop",
                                     "end of the sample loop",
                                     -1,
                                     G_MAXLONG,
                                     -1,
                                     G_PARAM_READWRITE));

  g_object_class_install_property(gobject_class,WAVELEVEL_RATE,
                                  g_param_spec_ulong("rate",
                                     "rate prop",
                                     "sampling rate of the sample",
                                     0,
                                     G_MAXULONG,
                                     0,
                                     G_PARAM_READWRITE));
}

GType bt_wavelevel_get_type(void) {
  static GType type = 0;
  if (type == 0) {
    static const GTypeInfo info = {
      G_STRUCT_SIZE(BtWavelevelClass),
      NULL, // base_init
      NULL, // base_finalize
      (GClassInitFunc)bt_wavelevel_class_init, // class_init
      NULL, // class_finalize
      NULL, // class_data
      G_STRUCT_SIZE(BtWavelevel),
      0,   // n_preallocs
      (GInstanceInitFunc)bt_wavelevel_init, // instance_init
      NULL // value_table
    };
    static const GInterfaceInfo persistence_interface_info = {
      (GInterfaceInitFunc) bt_wavelevel_persistence_interface_init,  // interface_init
      NULL, // interface_finalize
      NULL  // interface_data
    };
    type = g_type_register_static(G_TYPE_OBJECT,"BtWavelevel",&info,0);
    g_type_add_interface_static(type, BT_TYPE_PERSISTENCE, &persistence_interface_info);
  }
  return type;
}
