/* $Id$
 *
 * Buzztard
 * Copyright (C) 2007 Buzztard team <buzztard-devel@lists.sf.net>
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
 * SECTION:bticregistry
 * @short_description: buzztards interaction controller registry
 *
 * Manages a dynamic list of controller devices. It uses HAL and dbus.
 */
/*
 * http://webcvs.freedesktop.org/hal/hal/doc/spec/hal-spec.html?view=co
 */
#define BTIC_CORE
#define BTIC_REGISTRY_C

#include "ic_private.h"

enum {
  REGISTRY_DEVICE_LIST=1
};

struct _BtIcRegistryPrivate {
  /* used to validate if dispose has run */
  gboolean dispose_has_run;

  /* list of BtIcDevice objects */
  GList *devices;

#if USE_GUDEV
  GUdevClient *client;
#elif USE_HAL
  LibHalContext *ctx;
  DBusError dbus_error;
  DBusConnection *dbus_conn;
#endif
};

static BtIcRegistry *singleton=NULL;

//-- the class

G_DEFINE_TYPE (BtIcRegistry, btic_registry, G_TYPE_OBJECT);

//-- helper

static void remove_device_by_udi(BtIcRegistry *self, const gchar *udi) {
  GList *node;
  BtIcDevice *device;
  gchar *device_udi;

  // search for device by udi
  for(node=self->priv->devices;node;node=g_list_next(node)) {
    device=BTIC_DEVICE(node->data);
    g_object_get(device,"udi",&device_udi,NULL);
    if(!strcmp(udi,device_udi)) {
      // remove devices from our list and trigger notify
      self->priv->devices=g_list_delete_link(self->priv->devices,node);
      g_object_unref(device);
      g_object_notify(G_OBJECT(self),"devices");
      break;
    }
    g_free(device_udi);
  }
}

static void add_device(BtIcRegistry *self, BtIcDevice *device) {
  if(btic_device_has_controls(device) || BTIC_IS_LEARN(device)) {
    // add devices to our list and trigger notify
    self->priv->devices=g_list_prepend(self->priv->devices,(gpointer)device);
    g_object_notify(G_OBJECT(self),"devices");
  }
  else {
    GST_DEBUG("device has no controls, not adding");
    g_object_unref(device);
  }
}

//-- handler

#if USE_GUDEV

/*

remove: subsys=   input, devtype=       (null), name=    event1, number= 1, devnode=/dev/input/event1
remove: subsys=   input, devtype=       (null), name=       js0, number= 0, devnode=/dev/input/js0
remove: subsys=   input, devtype=       (null), name=   input11, number=11, devnode=(null)
remove: subsys=  hidraw, devtype=       (null), name=   hidraw0, number= 0, devnode=/dev/hidraw0
remove: subsys=     hid, devtype=       (null), name=0003:06A3:0502.0008, number=0008, devnode=(null)
remove: subsys=     usb, devtype=usb_interface, name=   5-5:1.0, number= 0, devnode=(null)
remove: subsys=     usb, devtype=usb_device,    name=       5-5, number= 5, devnode=/dev/bus/usb/005/014
   add: subsys=     usb, devtype=usb_device,    name=       5-5, number= 5, devnode=/dev/bus/usb/005/015
   add: subsys=     usb, devtype=usb_interface, name=   5-5:1.0, number= 0, devnode=(null)
   add: subsys=     hid, devtype=       (null), name=0003:06A3:0502.0009, number=0009, path=(null)
   add: subsys=  hidraw, devtype=       (null), name=   hidraw0, number= 0, devnode=/dev/hidraw0
   add: subsys=   input, devtype=       (null), name=   input12, number=12, devnode=(null)
   add: subsys=   input, devtype=       (null), name=       js0, number= 0, devnode=/dev/input/js0
   add: subsys=   input, devtype=       (null), name=    event1, number= 1, devnode=/dev/input/event1

 
old HAL output:
registry.c:305:hal_scan: 20 alsa devices found, trying add..
registry.c:275:on_device_added: midi device added: product=Hoontech SoundTrack Audio DSP24 ALSA MIDI Device, devnode=/dev/snd/midiC2D1
registry.c:275:on_device_added: midi device added: product=Hoontech SoundTrack Audio DSP24 ALSA MIDI Device, devnode=/dev/snd/midiC2D0
registry.c:305:hal_scan: 0 alsa.sequencer devices found, trying add..
registry.c:305:hal_scan: 14 oss devices found, trying add..
registry.c:249:on_device_added: midi device added: product=ICE1712 multi OSS MIDI Device, devnode=/dev/midi2
registry.c:249:on_device_added: midi device added: product=ICE1712 multi OSS MIDI Device, devnode=/dev/amidi2

*/

static void on_uevent(GUdevClient *client,gchar *action,GUdevDevice *udevice,gpointer user_data) {
  BtIcRegistry *self=BTIC_REGISTRY(user_data);
  const gchar *udi=g_udev_device_get_sysfs_path(udevice);
  const gchar *devnode=g_udev_device_get_device_file(udevice);
  const gchar *name=g_udev_device_get_name(udevice);
  const gchar *subsystem=g_udev_device_get_subsystem(udevice);
  
  GST_WARNING("action=%6s: subsys=%8s, devtype=%15s, name=%10s, number=%2s, devnode=%s, driver=%s",
    action,subsystem,g_udev_device_get_devtype(udevice),name,
    g_udev_device_get_number(udevice),devnode,g_udev_device_get_driver(udevice));
  
  if(!devnode)
    return;
  
  if(!strcmp(action,"add")) {
    BtIcDevice *device=NULL;
    GUdevDevice *uparent;
    const gchar *full_name=NULL;
    const gchar *vendor_name, *model_name;
    gboolean free_full_name=FALSE;
    gchar *cat_full_name;

    /*
    const gchar* const *props=g_udev_device_get_property_keys(udevice);
    while(*props) {
      GST_INFO("  %s: %s", *props, g_udev_device_get_property(udevice,*props));
      props++;
    }
    */

    /* get human readable device name */
    uparent=udevice;
    vendor_name=g_udev_device_get_property(uparent, "ID_VENDOR_FROM_DATABASE");
    if(!vendor_name) vendor_name=g_udev_device_get_property(uparent, "ID_VENDOR");
    model_name=g_udev_device_get_property(uparent, "ID_MODEL_FROM_DATABASE");
    if(!model_name) model_name=g_udev_device_get_property(uparent, "ID_MODEL");
    GST_INFO("  v m:  '%s' '%s'",vendor_name,model_name);
    while(uparent && !(vendor_name && model_name)) {
      if((uparent=g_udev_device_get_parent(uparent))) {
        if(!vendor_name) vendor_name=g_udev_device_get_property(uparent, "ID_VENDOR_FROM_DATABASE");
        if(!vendor_name) vendor_name=g_udev_device_get_property(uparent, "ID_VENDOR");
        if(!model_name) model_name=g_udev_device_get_property(uparent, "ID_MODEL_FROM_DATABASE");
        if(!model_name) model_name=g_udev_device_get_property(uparent, "ID_MODEL");
        GST_INFO("  v m:  '%s' '%s'",vendor_name,model_name);
      }
    }
    if(vendor_name && model_name) {
      full_name=g_strconcat(vendor_name," ",model_name,NULL);
      free_full_name=TRUE;
    } else {
      full_name=name;
    }
    
    /* FIXME: we got different names with HAL (we save those in songs :/):
    * http://cgit.freedesktop.org/hal/tree/hald/linux/device.c#n3400
    * http://cgit.freedesktop.org/hal/tree/hald/linux/device.c#n3363
    */
    
    if(!strcmp(subsystem,"input")) {
      cat_full_name=g_strconcat("input: ",full_name,NULL);
      device=BTIC_DEVICE(btic_input_device_new(udi,cat_full_name,devnode));
      g_free(cat_full_name);
    } else if(!strcmp(subsystem,"sound")) {
      /* http://cgit.freedesktop.org/hal/tree/hald/linux/device.c#n3509 */
      if(!strncmp(name, "midiC", 5)) {
        /* alsa */
        cat_full_name=g_strconcat("alsa midi: ",full_name,NULL);
        device=BTIC_DEVICE(btic_midi_device_new(udi,cat_full_name,devnode));
        g_free(cat_full_name);
      } else if(!strcmp(name, "midi2") || !strcmp(name, "amidi2")) {
        /* oss */
        cat_full_name=g_strconcat("oss midi: ",full_name,NULL);
        device=BTIC_DEVICE(btic_midi_device_new(udi,cat_full_name,devnode));
        g_free(cat_full_name);
      }
    }
    
    if(free_full_name) {
      g_free((gchar *)full_name);
    }

    if(device) {
      add_device(self,device);
    }
    else {
      GST_DEBUG("unknown device found, not added: name=%s",name);
    }
  } else if(!strcmp(action,"remove")) {
    remove_device_by_udi(self,udi);
  }
    
}

static void gudev_scan(BtIcRegistry *self, const gchar *subsystem) {
  GList *list,*node;
  GUdevDevice *device;

  if((list=g_udev_client_query_by_subsystem(self->priv->client,subsystem))) {
    for(node=list;node;node=g_list_next(node)) {
      device=(GUdevDevice *)node->data;
      on_uevent(self->priv->client,"add",device,(gpointer)self);
      g_object_unref(device);
    }
    g_list_free(list);
  }
}
  
static gboolean gudev_setup(BtIcRegistry *self) {
  /* check with 'udevadm monitor' */
  const gchar * const subsystems[]={
    /*
    "input",
    "sound",*/
    NULL
  };

  // get a gudev client
  if(!(self->priv->client=g_udev_client_new(subsystems))) {
    GST_WARNING("Could not create gudev client context");
    return(FALSE);
  }
  // register notifications
  g_signal_connect(self->priv->client,"uevent",G_CALLBACK(on_uevent),(gpointer)self);
  
  // check already known devices
  gudev_scan(self,"input");
  gudev_scan(self,"sound");

  return(TRUE);
}

#elif USE_HAL

static void on_device_added(LibHalContext *ctx, const gchar *udi) {
  BtIcRegistry *self=BTIC_REGISTRY(singleton);
  gchar **cap;
  gchar *hal_category;
  gchar *temp,*parent_udi;
  gchar *name,*devnode,*type;
  size_t n;
  BtIcDevice *device=NULL;

  if(!(cap=libhal_device_get_property_strlist(ctx,udi,"info.capabilities",NULL))) {
    return;
  }
  if(!(hal_category=libhal_device_get_property_string(ctx,udi,"info.category",NULL))) {
    libhal_free_string_array(cap);
    return;
  }
  name=libhal_device_get_property_string(ctx,udi,"info.product",NULL);

  for(n=0;cap[n];n++) {
    // midi devices seem to appear only as oss under hal?
    // @todo: try alsa.sequencer
    if(!strcmp(cap[n],"alsa.sequencer")) {
      temp=libhal_device_get_property_string(ctx,udi,"info.parent",NULL);
      parent_udi=libhal_device_get_property_string(ctx,temp,"info.parent",NULL);
      libhal_free_string(temp);

      devnode=libhal_device_get_property_string(ctx,udi,"alsa.device_file",NULL);

      GST_INFO("alsa device added: type=%s, device_file=%s, vendor=%s",
        libhal_device_get_property_string(ctx,udi,"alsa.type",NULL),
        devnode,
        libhal_device_get_property_string(ctx,parent_udi,"info.vendor",NULL)
      );
      // create device
      device=BTIC_DEVICE(btic_midi_device_new(udi,name,devnode));
      libhal_free_string(devnode);
      libhal_free_string(parent_udi);
    }
    if(!strcmp(cap[n],"oss")) {
      type=libhal_device_get_property_string(ctx,udi,"oss.type",NULL);
      if(!strcmp(type,"midi")) {
        devnode=libhal_device_get_property_string(ctx,udi,"oss.device_file",NULL);

        GST_INFO("midi device added: product=%s, devnode=%s",
          name,devnode);
        // create device
        device=BTIC_DEVICE(btic_midi_device_new(udi,name,devnode));
        libhal_free_string(devnode);
      }
      libhal_free_string(type);
    }
#ifdef HAVE_LINUX_INPUT_H
    else if(!strcmp(cap[n],"input")) {
      devnode=libhal_device_get_property_string(ctx,udi,"input.device",NULL);

      GST_INFO("input device added: product=%s, devnode=%s", name,devnode);
      // create device
      device=BTIC_DEVICE(btic_input_device_new(udi,name,devnode));
      libhal_free_string(devnode);
    }
#endif
  }
  libhal_free_string_array(cap);

  // finished checking devices regarding capabilities, now checking category
  if(!strcmp(hal_category,"alsa")) {
    gchar *alsatype = libhal_device_get_property_string(ctx,udi,"alsa.type",NULL);
    if(!strcmp(alsatype,"midi")) {
      devnode=libhal_device_get_property_string(ctx,udi,"linux.device_file",NULL);

      GST_INFO("midi device added: product=%s, devnode=%s", name,devnode);
      // create device
      device=BTIC_DEVICE(btic_midi_device_new(udi,name,devnode));
      libhal_free_string(devnode);
    }
    libhal_free_string(alsatype);
  }

  if(device) {
    add_device(self,device);
  }
  else {
    GST_DEBUG("unknown device found, not added: name=%s",name);
  }

  libhal_free_string(hal_category);
  libhal_free_string(name);
}

static void on_device_removed(LibHalContext *ctx, const gchar *udi) {
  BtIcRegistry *self=BTIC_REGISTRY(singleton);
  
  remove_device_by_udi(self,udi);
}

static void hal_scan(BtIcRegistry *self, const gchar *subsystem) {
  gchar **devices;
  gint i,num_devices;

  if((devices=libhal_find_device_by_capability(self->priv->ctx,subsystem,&num_devices,&self->priv->dbus_error))) {
    GST_INFO("%d %s devices found, trying add..",num_devices,subsystem);
    for(i=0;i<num_devices;i++) {
      on_device_added(self->priv->ctx,devices[i]);
    }
    libhal_free_string_array(devices);
  }
}

static gboolean hal_setup(BtIcRegistry *self) {
  // init dbus
  dbus_error_init(&self->priv->dbus_error);
  self->priv->dbus_conn=dbus_bus_get(DBUS_BUS_SYSTEM,&self->priv->dbus_error);
  if(dbus_error_is_set(&self->priv->dbus_error)) {
    GST_WARNING("Could not connect to system bus %s", self->priv->dbus_error.message);
    return(FALSE);
  }
  dbus_connection_setup_with_g_main(self->priv->dbus_conn,NULL);
  dbus_connection_set_exit_on_disconnect(self->priv->dbus_conn,FALSE);
  
  GST_DEBUG("dbus init okay");

  // init hal
  if(!(self->priv->ctx=libhal_ctx_new())) {
    GST_WARNING("Could not create hal context");
    return(FALSE);
  }
  if(!libhal_ctx_set_dbus_connection(self->priv->ctx,self->priv->dbus_conn)) {
    GST_WARNING("Failed to set dbus connection to hal ctx");
    return(FALSE);
  }
  
  GST_DEBUG("hal init okay");
  
  // register notify handler for add/remove
  libhal_ctx_set_device_added(self->priv->ctx,on_device_added);
  libhal_ctx_set_device_removed(self->priv->ctx,on_device_removed);
  if(!(libhal_ctx_init(self->priv->ctx,&self->priv->dbus_error))) {
    if(dbus_error_is_set(&self->priv->dbus_error)) {
      GST_WARNING("Could not init hal %s", singleton->priv->dbus_error.message);
    }
    return(FALSE);
  }
  // scan already plugged devices via hal
  hal_scan(self,"input");
  hal_scan(self,"alsa");
  hal_scan(self,"alsa.sequencer");
  hal_scan(self,"oss");
  return(TRUE);
}
#endif

//-- constructor methods

/**
 * btic_registry_new:
 *
 * Create a new instance
 *
 * Returns: the new instance
 */
BtIcRegistry *btic_registry_new(void) {
  return(g_object_new(BTIC_TYPE_REGISTRY,NULL));
}

//-- methods

//-- wrapper

//-- class internals

/* returns a property for the given property_id for this object */
static void btic_registry_get_property(GObject * const object, const guint property_id, GValue * const value, GParamSpec * const pspec) {
  const BtIcRegistry * const self = BTIC_REGISTRY(object);
  return_if_disposed();
  switch (property_id) {
    case REGISTRY_DEVICE_LIST: {
      g_value_set_pointer(value,g_list_copy(self->priv->devices));
    } break;
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

/* sets the given properties for this object */
static void btic_registry_set_property(GObject * const object, const guint property_id, const GValue * const value, GParamSpec * const pspec) {
  const BtIcRegistry * const self = BTIC_REGISTRY(object);
  return_if_disposed();
  switch (property_id) {
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

static void btic_registry_dispose(GObject * const object) {
  const BtIcRegistry * const self = BTIC_REGISTRY(object);

  return_if_disposed();
  self->priv->dispose_has_run = TRUE;

  GST_DEBUG("!!!! self=%p, self->ref_ct=%d",self,G_OBJECT_REF_COUNT(self));
#if USE_GUDEV
  g_object_try_unref(self->priv->client);
#elif USE_HAL
  libhal_ctx_free(self->priv->ctx);
  dbus_error_free(&self->priv->dbus_error);
#endif
  if(self->priv->devices) {
    GST_DEBUG("!!!! free devices: %d",g_list_length(self->priv->devices));
    GList* node;
    for(node=self->priv->devices;node;node=g_list_next(node)) {
      g_object_try_unref(node->data);
      node->data=NULL;
    }
  }

  GST_DEBUG("  chaining up");
  G_OBJECT_CLASS(btic_registry_parent_class)->dispose(object);
  GST_DEBUG("  done");
}

static void btic_registry_finalize(GObject * const object) {
  const BtIcRegistry * const self = BTIC_REGISTRY(object);

  GST_DEBUG("!!!! self=%p",self);

  if(self->priv->devices) {
    g_list_free(self->priv->devices);
    self->priv->devices=NULL;
  }

  GST_DEBUG("  chaining up");
  G_OBJECT_CLASS(btic_registry_parent_class)->finalize(object);
  GST_DEBUG("  done");
}

static GObject *btic_registry_constructor(GType type,guint n_construct_params,GObjectConstructParam *construct_params) {
  GObject *object;

  if(G_UNLIKELY(!singleton)) {
    object=G_OBJECT_CLASS(btic_registry_parent_class)->constructor(type,n_construct_params,construct_params);
    singleton=BTIC_REGISTRY(object);
    g_object_add_weak_pointer(object,(gpointer*)(gpointer)&singleton);
    
    GST_INFO("new device registry created");
#if USE_GUDEV
    if(gudev_setup(singleton)) {
      GST_INFO("gudev device registry initialized");
    } else {
      GST_INFO("gudev device registry setup failed");
    }
#elif USE_HAL
    if(hal_setup(singleton)) {
      GST_INFO("hal device registry initialized");
    } else {
      GST_INFO("hal device registry setup failed");
    }
#else
    GST_INFO("no GUDev/HAL support, not creating device registry");
#endif
  }
  else {
    object=g_object_ref(singleton);
  }
  return object;
}


static void btic_registry_init(BtIcRegistry *self) {
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, BTIC_TYPE_REGISTRY, BtIcRegistryPrivate);
}

static void btic_registry_class_init(BtIcRegistryClass * const klass) {
  GObjectClass * const gobject_class = G_OBJECT_CLASS(klass);

  g_type_class_add_private(klass,sizeof(BtIcRegistryPrivate));

  gobject_class->constructor  = btic_registry_constructor;
  gobject_class->set_property = btic_registry_set_property;
  gobject_class->get_property = btic_registry_get_property;
  gobject_class->dispose      = btic_registry_dispose;
  gobject_class->finalize     = btic_registry_finalize;

  g_object_class_install_property(gobject_class,REGISTRY_DEVICE_LIST,
                                  g_param_spec_pointer("devices",
                                     "device list prop",
                                     "A copy of the list of control devices",
                                     G_PARAM_READABLE|G_PARAM_STATIC_STRINGS));
}

