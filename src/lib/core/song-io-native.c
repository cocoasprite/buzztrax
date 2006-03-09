// $Id: song-io-native.c,v 1.103 2006-03-09 21:50:23 ensonic Exp $
/**
 * SECTION:btsongionative
 * @short_description: class for song input and output in builtin native format
 *
 * Buzztard stores its songs in a own file-format. This internal io-module 
 * implements loading and saving of this format.
 * The format is an archive, that contains an XML file and optionally binary
 * data, such as audio samples.
 */ 
 
#define BT_CORE
#define BT_SONG_IO_NATIVE_C

// the new bt_persistence code takes over
// use defines below toreenable old code
//#define USE_OLD_SAVER
//#define USE_OLD_LOADER

#include <libbtcore/core.h>

/* @todo
 * - remove song_doc args that are not used
 * - pass song-component object to children, to avoid looking them up again and again
 * - better error handling
 *   - if a machine cannot be instantiated:
 *     - collect machine-names
 *     - tell user afterwards about *all* missing machines
 */

struct _BtSongIONativePrivate {
  /* used to validate if dispose has run */
  gboolean dispose_has_run;
};

static GQuark error_domain=0;

static BtSongIOClass *parent_class=NULL;

//-- plugin detect
/**
 * bt_song_io_native_detect:
 * @file_name: the file to check against
 *
 * Checks if this plugin should manage this kind of file.
 *
 * Returns: the GType of this plugin of %NULL
 */
GType bt_song_io_native_detect(const gchar *file_name) {
  GType type=0;
  char *absolute_uri_string=NULL;
  GnomeVFSURI *input_uri=NULL;
  GnomeVFSFileInfo *file_info=NULL;
  GnomeVFSResult result;

  // test filename first  
  GST_INFO("file_name=\"%s\"",file_name);
  if(!file_name) return(type);

  // creating a absolute uri string from the given input string.
  // works also if the given string was a absolute uri.
  absolute_uri_string = gnome_vfs_make_uri_from_input_with_dirs (file_name, GNOME_VFS_MAKE_URI_DIR_CURRENT);
  GST_INFO("creating absolute file uri string: %s\n",absolute_uri_string);
  // creating the gnome-vfs uri from the absolute path string
  input_uri = gnome_vfs_uri_new(absolute_uri_string);
  // check if the input uri is ok
  if (input_uri == NULL) {
    GST_WARNING("cannot create input uri for gnome vfs\n");
    goto Error;
  }
  // check if the given uri exists
  if (!gnome_vfs_uri_exists(input_uri)) {
    gchar *lc_file_name;
    
    GST_WARNING("given input uri doe's not exists ... abort loading\n");

    lc_file_name=g_ascii_strdown(file_name,-1);
    if(g_str_has_suffix(lc_file_name,".xml")) {
      type=BT_TYPE_SONG_IO_NATIVE;
    }
    g_free(lc_file_name);
  }
  else {
    // create new file info pointer.
    file_info = gnome_vfs_file_info_new ();
    // now we check the mime type
    if((result=gnome_vfs_get_file_info_uri(input_uri,file_info,GNOME_VFS_FILE_INFO_GET_MIME_TYPE))!=GNOME_VFS_OK) {
      GST_WARNING("Cannot determine mime type. Error: %s\n", gnome_vfs_result_to_string (result));
      goto Error;
    }
    // @todo: check mime-type ?
    type=BT_TYPE_SONG_IO_NATIVE;
  }
Error:
  if(absolute_uri_string) g_free(absolute_uri_string);
  if(file_info) gnome_vfs_file_info_unref(file_info);
  return(type);
}

#ifdef USE_OLD_LOADER

//-- xml helper methods

/**
 * xpath_type_filter:
 * @xpath_optr: the xpath object to test
 * @type: the required type
 *
 * test if the given XPathObject is of the expected type, otherwise discard the object
 * Returns: the supplied xpath object or %NULL is types do not match
 */
static xmlXPathObjectPtr xpath_type_filter(xmlXPathObjectPtr xpath_optr,const xmlXPathObjectType type) {
  if(xpath_optr && (xpath_optr->type!=type)) {
    GST_ERROR("xpath expr does not returned the expected type: ist=%ld <-> soll=%ld",(unsigned long)xpath_optr->type,(unsigned long)type);
    xmlXPathFreeObject(xpath_optr);
    xpath_optr=NULL;
  }
  return(xpath_optr);
}


/**
 * cxpath_get_object:
 * @doc: gitk dialog
 * @xpath_comp_expression: compiled xpath expression to use
 * @root_node: from where to search, uses doc root when NULL
 *
 * return the result as xmlXPathObjectPtr of the evaluation of the supplied
 * compiled xpath expression agains the given document
 *
 * Returns: the xpathobject; do not forget to free the result after use
 */
static xmlXPathObjectPtr cxpath_get_object(const xmlDocPtr doc,xmlXPathCompExprPtr const xpath_comp_expression, xmlNodePtr const root_node) {
  xmlXPathObjectPtr result=NULL;
  xmlXPathContextPtr ctxt;

  g_assert(doc);
  g_assert(xpath_comp_expression);

  //gitk_log_intro();
  
  if((ctxt=xmlXPathNewContext(doc))) {
    if(root_node) {
      ctxt->node=root_node;
    }  else {
      ctxt->node=xmlDocGetRootElement(doc);
    }
    if( (!xmlXPathRegisterNs(ctxt,
                             XML_CHAR_PTR(BT_NS_PREFIX),
                             XML_CHAR_PTR(BT_NS_URL)))
        && (!xmlXPathRegisterNs(ctxt,XML_CHAR_PTR("dc"),
                                XML_CHAR_PTR("http://purl.org/dc/elements/1.1/")))
    ) {
      result=xmlXPathCompiledEval(xpath_comp_expression,ctxt);
      xmlXPathRegisteredNsCleanup(ctxt);
    }
    else  {
      GST_ERROR("failed to register \"buzztard\" or \"dc\" namespace");
    }
    xmlXPathFreeContext(ctxt);
  }
  else {
    GST_ERROR("failed to get xpath context");
  }
  return(result);
}

#endif

//-- string formatting helper

// @todo: move to bt_persistence_*

#ifdef USE_OLD_SAVER

static const gchar *strfmt_long(glong val) {
  static gchar str[20];

  g_sprintf(str,"%ld",val);
  return(str);
}

static const gchar *strfmt_ulong(gulong val) {
  static gchar str[20];

  g_sprintf(str,"%lu",val);
  return(str);
}

#endif

//-- loader helper methods

#ifdef USE_OLD_LOADER

static gboolean bt_song_io_native_load_properties(const BtSongIONative *self, const BtSong *song, xmlNodePtr xml_node, GObject *object) {
  xmlNodePtr xml_subnode;
  GHashTable *properties;
  xmlChar *key,*value;
  
  // get property hashtable from object, return if NULL
  g_object_get(object,"properties",&properties,NULL);
  if(!properties) return(TRUE);
  
  // look for <properties> node
  while(xml_node) {
    if((!xmlNodeIsText(xml_node)) && (!strncmp((char *)xml_node->name,"properties\0",11))) {
      GST_DEBUG("  reading properties ...");
      // iterate over children
      xml_subnode=xml_node->children;
      while(xml_subnode) {
        if(!xmlNodeIsText(xml_subnode) && !strncmp((char *)xml_subnode->name,"property\0",9)) {
          key=xmlGetProp(xml_subnode,XML_CHAR_PTR("key"));
          value=xmlGetProp(xml_subnode,XML_CHAR_PTR("value"));
          GST_DEBUG("    [%s] => [%s]",key,value);
          // @idea would be nice if we could use this instead
          //g_object_set_data(object,key,value);
          g_hash_table_insert(properties,key,value);
          // do not free, as the hastable now owns the memory
          //xmlFree(key);xmlFree(value);
        }
        xml_subnode=xml_subnode->next;
      }
    }
    xml_node=xml_node->next;
  }
  return(TRUE);
}

static gboolean bt_song_io_native_load_song_info(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc) {
  BtSongInfo *song_info;
  xmlXPathObjectPtr items_xpoptr;
  xmlNodePtr xml_node,xml_child_node;
  xmlChar *elem;
  
  GST_INFO("loading the meta-data from the song");
  g_object_get(G_OBJECT(song),"song-info",&song_info,NULL);
  // get top xml-node
  if((items_xpoptr=xpath_type_filter(
        cxpath_get_object(song_doc,BT_SONG_IO_NATIVE_GET_CLASS(self)->xpath_get_meta,NULL),
        XPATH_NODESET)))
  {
    gint i;
    xmlNodeSetPtr items=(xmlNodeSetPtr)items_xpoptr->nodesetval;
    gint items_len=xmlXPathNodeSetGetLength(items);
    const gchar *property_name;

    GST_INFO(" got meta root node with %d items",items_len);
    for(i=0;i<items_len;i++) {
      xml_node=xmlXPathNodeSetItem(items,i);
      if(!xmlNodeIsText(xml_node)) {
        //GST_DEBUG("  %2d : \"%s\"",i,xml_node->name);
        xml_child_node=xml_node->children;
        if(xml_child_node && xmlNodeIsText(xml_child_node)) {
          if(!xmlIsBlankNode(xml_child_node)) {
            if((elem=xmlNodeGetContent(xml_child_node))) {
              property_name=(gchar *)xml_node->name;
              GST_DEBUG("  %2d : \"%s\"=\"%s\"",i,property_name,elem);
              // depending on the name of the property, treat it's type
              if(!strncmp(property_name,"info",4) ||
                !strncmp(property_name,"name",4) ||
                !strncmp(property_name,"genre",5) ||
                !strncmp(property_name,"author",6) ||
                !strncmp(property_name,"create-dts",10) ||
                !strncmp(property_name,"change-dts",10)
              ) {
                g_object_set(G_OBJECT(song_info),property_name,elem,NULL);
              }
              else if(!strncmp(property_name,"bpm",3) ||
                !strncmp(property_name,"tpb",3) ||
                !strncmp(property_name,"bars",4)) {
                g_object_set(G_OBJECT(song_info),property_name,atol((char *)elem),NULL);
              }
              xmlFree(elem);
            }
          }
        }
      }
    }
    xmlXPathFreeObject(items_xpoptr);
  }
  // release the references
  g_object_try_unref(song_info);  
  return(TRUE);
}

static gboolean bt_song_io_native_load_setup_machines(const BtSongIONative *self, const BtSong *song, xmlNodePtr xml_node) {
  BtSetup *setup;
  BtMachine *machine;
  xmlChar *id,*plugin_name,*voices_str;
  glong voices;
  gchar *name;
  
  GST_INFO(" got setup.machines root node");
  g_object_get(G_OBJECT(song),"setup",&setup,NULL);
  while(xml_node) {
    if(!xmlNodeIsText(xml_node)) {
      machine=NULL;
      id=xmlGetProp(xml_node,XML_CHAR_PTR("id"));
      plugin_name=xmlGetProp(xml_node,XML_CHAR_PTR("plugin-name"));
      name=bt_setup_get_unique_machine_id(setup,(gchar *)id);
      // parse additional params
      if( (voices_str=xmlGetProp(xml_node,XML_CHAR_PTR("voices"))) ) {
        voices=atol((char *)voices_str);
      }
      else voices=0;
      if(!strncmp((char *)xml_node->name,"sink\0",5)) {
        GST_INFO("  new sink_machine(\"%s\") -----------------",id);
        // create new sink machine
        machine=BT_MACHINE(bt_sink_machine_new(song,name));
      }
      else if(!strncmp((char *)xml_node->name,"source\0",7)) {
        GST_INFO("  new source_machine(\"%s\",\"%s\",%d) -----------------",id,plugin_name,voices);
        // create new source machine
        machine=BT_MACHINE(bt_source_machine_new(song,name,(gchar *)plugin_name,voices));
      }
      else if(!strncmp((char *)xml_node->name,"processor\0",10)) {
        GST_INFO("  new processor_machine(\"%s\",\"%s\",%d) -----------------",id,plugin_name,voices);
        // create new processor machine
        machine=BT_MACHINE(bt_processor_machine_new(song,name,(gchar *)plugin_name,voices));
      }
      if(machine) {
        // load properties
        bt_song_io_native_load_properties(self,song,xml_node->children,G_OBJECT(machine));
        // @todo load global data
        g_object_unref(machine);
      }
      xmlFree(id);xmlFree(plugin_name);xmlFree(voices_str);
      g_free(name);
    }
    xml_node=xml_node->next;
  }
  //-- release the reference
  g_object_try_unref(setup);
  return(TRUE);
}

static gboolean bt_song_io_native_load_setup_wires(const BtSongIONative *self, const BtSong *song, xmlNodePtr xml_node) {
  BtSetup *setup;
  BtMachine *src_machine,*dst_machine;
  xmlChar *src,*dst;

  GST_INFO(" got setup.wires root node");
  g_object_get(G_OBJECT(song),"setup",&setup,NULL);
  while(xml_node) {
    if((!xmlNodeIsText(xml_node)) && (!strncmp((char *)xml_node->name,"wire\0",5))) {
      src=xmlGetProp(xml_node,XML_CHAR_PTR("src"));
      dst=xmlGetProp(xml_node,XML_CHAR_PTR("dst"));
      src_machine=bt_setup_get_machine_by_id(setup,(gchar *)src);
      dst_machine=bt_setup_get_machine_by_id(setup,(gchar *)dst);
      GST_INFO("  new wire(%p=\"%s\",%p=\"%s\") --------------------",src_machine,src,dst_machine,dst);
      // create new wire
      bt_wire_new(song,src_machine,dst_machine);
      xmlFree(src);xmlFree(dst);
      g_object_unref(src_machine);
      g_object_unref(dst_machine);
    }
    xml_node=xml_node->next;
  }
  //-- release the reference
  g_object_try_unref(setup);
  return(TRUE);
}

static gboolean bt_song_io_native_load_setup(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc) {
  xmlXPathObjectPtr items_xpoptr;
  xmlNodePtr xml_node;
  
  GST_INFO("loading the setup-data from the song");

  // get top xml-node
  if((items_xpoptr=xpath_type_filter(
        cxpath_get_object(song_doc,BT_SONG_IO_NATIVE_GET_CLASS(self)->xpath_get_setup,NULL),
        XPATH_NODESET)))
  {
    gint i;
    xmlNodeSetPtr items=(xmlNodeSetPtr)items_xpoptr->nodesetval;
    gint items_len=xmlXPathNodeSetGetLength(items);

    GST_INFO(" got setup root node with %d items",items_len);
    for(i=0;i<items_len;i++) {
      xml_node=xmlXPathNodeSetItem(items,i);
      if(!xmlNodeIsText(xml_node)) {
        if(!strncmp((gchar *)xml_node->name,"machines\0",8)) {
          bt_song_io_native_load_setup_machines(self,song,xml_node->children);
        }
        else if(!strncmp((gchar *)xml_node->name,"wires\0",6)) {
          bt_song_io_native_load_setup_wires(self,song,xml_node->children);
        }
      }
    }
    xmlXPathFreeObject(items_xpoptr);
  }
  return(TRUE);
}

static gboolean bt_song_io_native_load_pattern_data(const BtSongIONative *self, const BtPattern *pattern, const xmlDocPtr song_doc, xmlNodePtr xml_node, GError **error) {
  gboolean ret=TRUE;
  xmlNodePtr xml_subnode;
  xmlChar *tick_str,*name,*value,*voice_str;
  glong tick,voice,param;
  GError *tmp_error=NULL;
  
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  
  while(xml_node && ret) {
    if((!xmlNodeIsText(xml_node)) && (!strncmp((gchar *)xml_node->name,"tick\0",5))) {
      tick_str=xmlGetProp(xml_node,XML_CHAR_PTR("time"));
      tick=atoi((char *)tick_str);
      GST_DEBUG("   loading tick at %d",tick);
      // iterate over children
      xml_subnode=xml_node->children;
      while(xml_subnode && ret) {
        if(!xmlNodeIsText(xml_subnode)) {
          name=xmlGetProp(xml_subnode,XML_CHAR_PTR("name"));
          value=xmlGetProp(xml_subnode,XML_CHAR_PTR("value"));
          GST_DEBUG("     \"%s\" -> \"%s\"",safe_string(name),safe_string(value));
          if(!strncmp((char *)xml_subnode->name,"globaldata\0",11)) {
            param=bt_pattern_get_global_param_index(pattern,(gchar *)name,&tmp_error);
            if(!tmp_error) {
              bt_pattern_set_global_event(pattern,tick,param,(gchar *)value);
            }
            else {
              //g_set_error (error, error_domain, /* errorcode= */0,
              //    "can't load global data for pattern %s", name);
              g_propagate_error(error, tmp_error);
              ret=FALSE;
            }
          }
          if(!strncmp((char *)xml_subnode->name,"voicedata\0",10)) {
            voice_str=xmlGetProp(xml_subnode,XML_CHAR_PTR("voice"));
            voice=atol((char *)voice_str);
            param=bt_pattern_get_voice_param_index(pattern,(gchar *)name,&tmp_error);
            if(!tmp_error) {
              bt_pattern_set_voice_event(pattern,tick,voice,param,(gchar *)value);
            }
            else {
              //g_set_error (error, error_domain, /* errorcode= */0,
              //    "can't load voice data for pattern %s", name);
              g_propagate_error(error, tmp_error);
              ret=FALSE;
            }
            xmlFree(voice_str);
          }
          xmlFree(name);xmlFree(value);
        }
        xml_subnode=xml_subnode->next;
      }
      xmlFree(tick_str);
    }
    xml_node=xml_node->next;
  }
  return(ret);
}

static gboolean bt_song_io_native_load_pattern(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc, xmlNodePtr xml_node ) {
  gboolean ret=TRUE;
  BtSetup *setup;
  BtMachine *machine;
  BtPattern *pattern;
  xmlChar *id,*machine_id,*pattern_name,*length_str;
  glong length;
  GError *tmp_error=NULL;
  
  g_object_get(G_OBJECT(song),"setup",&setup,NULL);
  id=xmlGetProp(xml_node,XML_CHAR_PTR("id"));
  machine_id=xmlGetProp(xml_node,XML_CHAR_PTR("machine"));
  pattern_name=xmlGetProp(xml_node,XML_CHAR_PTR("name"));
  length_str=xmlGetProp(xml_node,XML_CHAR_PTR("length"));
  length=atol((char *)length_str);
  // get the related machine
  if((machine=bt_setup_get_machine_by_id(setup,(gchar *)machine_id))) {
    // create pattern, add to machine's pattern-list and load data
    GST_INFO("  new pattern(\"%s\",%d) --------------------",id,length);
    if((pattern=bt_pattern_new(song,(gchar *)id,(gchar *)pattern_name,length,machine))) {
      //bt_song_io_native_load_properties(self,song,xml_node->children,pattern);
      if(!bt_song_io_native_load_pattern_data(self,pattern,song_doc,xml_node->children,&tmp_error)) {
        GST_WARNING("corrupt file: \"%s\"",tmp_error->message);
        g_error_free(tmp_error);
        ret=FALSE;
        bt_machine_remove_pattern(machine,pattern);
      }
      g_object_unref(pattern);
    }
    g_object_unref(machine);
  }
  else {
    GST_ERROR("invalid machine-id=\"%s\"",machine_id);
  }
  xmlFree(id);xmlFree(machine_id);xmlFree(pattern_name);xmlFree(length_str);
  //-- release the reference
  g_object_try_unref(setup);
  return(ret);
}

static gboolean bt_song_io_native_load_patterns(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc) {
  xmlXPathObjectPtr items_xpoptr;

  GST_INFO("loading the pattern-data from the song");
  // get top xml-node
  if((items_xpoptr=xpath_type_filter(
        cxpath_get_object(song_doc,BT_SONG_IO_NATIVE_GET_CLASS(self)->xpath_get_patterns,NULL),
        XPATH_NODESET)))
  {
    gint i;
    xmlNodeSetPtr items=(xmlNodeSetPtr)items_xpoptr->nodesetval;
    gint items_len=xmlXPathNodeSetGetLength(items);
    xmlNodePtr xml_node;

    GST_INFO(" got pattern root node with %d items",items_len);
    for(i=0;i<items_len;i++) {
      xml_node=xmlXPathNodeSetItem(items,i);
      if((!xmlNodeIsText(xml_node)) && (!strncmp((char *)xml_node->name,"pattern\0",8))) {
        bt_song_io_native_load_pattern(self,song,song_doc,xml_node);
      }
    }
    xmlXPathFreeObject(items_xpoptr);
  }
  return(TRUE);
}

static gboolean bt_song_io_native_get_sequence_length(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc, xmlNodePtr root_node) {
  BtSequence *sequence;
  xmlXPathObjectPtr items_xpoptr;

  g_object_get(G_OBJECT(song),"sequence",&sequence,NULL);
  if((items_xpoptr=xpath_type_filter(
    cxpath_get_object(song_doc,BT_SONG_IO_NATIVE_GET_CLASS(self)->xpath_get_sequence_length,root_node),
    XPATH_NODESET)))
  {
    gint i;
    xmlNodeSetPtr items=(xmlNodeSetPtr)items_xpoptr->nodesetval;
    gint items_len=xmlXPathNodeSetGetLength(items);
    glong maxtime=0,curtime;

    for(i=0;i<items_len;i++) {
      curtime=atol((char *)xmlNodeGetContent(xmlXPathNodeSetItem(items,i)));
      if(curtime>maxtime) maxtime=curtime;
    }
    maxtime++;  // time values start with 0
    GST_INFO(" got %d sequence.length with a max time of %d",items_len,maxtime);
    g_object_set(G_OBJECT(sequence),"length",maxtime,NULL);
    xmlXPathFreeObject(items_xpoptr);
  }
  // release the references
  g_object_try_unref(sequence);
  return(TRUE);
}

static gboolean bt_song_io_native_load_sequence_labels(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc, xmlNodePtr root_node) {
  BtSequence *sequence;
  xmlXPathObjectPtr items_xpoptr;
  xmlNodePtr xml_node;

  g_object_get(G_OBJECT(song),"sequence",&sequence,NULL);
  if((items_xpoptr=xpath_type_filter(
    cxpath_get_object(song_doc,BT_SONG_IO_NATIVE_GET_CLASS(self)->xpath_get_sequence_labels,root_node),
    XPATH_NODESET)))
  {
    xmlChar *time_str,*name;
    gint i;
    xmlNodeSetPtr items=(xmlNodeSetPtr)items_xpoptr->nodesetval;
    gint items_len=xmlXPathNodeSetGetLength(items);

    GST_INFO(" got sequence.labels root node with %d items",items_len);
    for(i=0;i<items_len;i++) {
      xml_node=xmlXPathNodeSetItem(items,i);
      if((!xmlNodeIsText(xml_node)) && (!strncmp((char *)xml_node->name,"label\0",6))) {
        time_str=xmlGetProp(xml_node,XML_CHAR_PTR("time"));
        name=xmlGetProp(xml_node,XML_CHAR_PTR("name"));
        GST_INFO("  new label(%s,\"%s\")",time_str,name);
        bt_sequence_set_label(sequence,atol((char *)time_str),(gchar *)name);
        xmlFree(time_str);xmlFree(name);
      }
    }
    xmlXPathFreeObject(items_xpoptr);
  }
  // release the references
  g_object_try_unref(sequence);
  return(TRUE);
}

static gboolean bt_song_io_native_load_sequence_track_data(const BtSongIONative *self, const BtSong *song, const BtMachine *machine, glong index, xmlNodePtr xml_node) {
  BtSequence *sequence;
  BtPattern *pattern;
  xmlChar *time_str,*pattern_id;

  g_object_get(G_OBJECT(song),"sequence",&sequence,NULL);

  bt_sequence_set_machine(sequence,index,machine);
  while(xml_node) {
    if((!xmlNodeIsText(xml_node)) && (!strncmp((char *)xml_node->name,"position\0",9))) {
      time_str=xmlGetProp(xml_node,XML_CHAR_PTR("time"));
      pattern_id=xmlGetProp(xml_node,XML_CHAR_PTR("pattern"));
      GST_DEBUG("  at %s, pattern \"%s\"",time_str,safe_string(pattern_id));
      if(pattern_id) {
        // get pattern by name from machine
        if((pattern=bt_machine_get_pattern_by_id(machine,(gchar *)pattern_id))) {
          bt_sequence_set_pattern(sequence,atol((char *)time_str),index,pattern);
          g_object_unref(pattern);
        }
        else GST_ERROR("  unknown pattern \"%s\"",pattern_id);
        xmlFree(pattern_id);
      }
      xmlFree(time_str);
    }
    xml_node=xml_node->next;
  }
  // release the references
  g_object_try_unref(sequence);
  return(TRUE);
}

static gboolean bt_song_io_native_load_sequence_tracks(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc, xmlNodePtr root_node) {
  BtSequence *sequence;
  BtSetup *setup;
  xmlXPathObjectPtr items_xpoptr;
  xmlNodePtr xml_node;
  xmlChar *index_str,*machine_name;
  BtMachine *machine;

  g_object_get((gpointer)song,"sequence",&sequence,"setup",&setup,NULL);
  if((items_xpoptr=xpath_type_filter(
    cxpath_get_object(song_doc,BT_SONG_IO_NATIVE_GET_CLASS(self)->xpath_get_sequence_tracks,root_node),
    XPATH_NODESET)))
  {
    gint i;
    xmlNodeSetPtr items=(xmlNodeSetPtr)items_xpoptr->nodesetval;
    gint items_len=xmlXPathNodeSetGetLength(items);
    
    g_object_set(sequence,"tracks",(glong)items_len,NULL);

    GST_INFO(" got sequence.tracks root node with %d items",items_len);
    for(i=0;i<items_len;i++) {
      xml_node=xmlXPathNodeSetItem(items,i);
      if((!xmlNodeIsText(xml_node)) && (!strncmp((char *)xml_node->name,"track\0",6))) {
        machine_name=xmlGetProp(xml_node,XML_CHAR_PTR("machine"));
        index_str=xmlGetProp(xml_node,XML_CHAR_PTR("index"));
        if((machine=bt_setup_get_machine_by_id(setup, (gchar *)machine_name))) {
          GST_DEBUG("loading track with index=%s for machine=\"%s\"",index_str,machine_name);
          bt_song_io_native_load_sequence_track_data(self,song,machine,atol((char *)index_str),xml_node->children);
          g_object_unref(machine);
        }
        else {
          GST_ERROR("invalid machine referenced");
        }
        xmlFree(index_str);xmlFree(machine_name);
      }
    }
    xmlXPathFreeObject(items_xpoptr);
  }
  // release the reference
  g_object_try_unref(sequence);
  g_object_try_unref(setup);
  return(TRUE);
}

static gboolean bt_song_io_native_load_sequence(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc) {
  BtSequence *sequence;
  xmlXPathObjectPtr items_xpoptr;
  xmlNodePtr xml_node;

  GST_INFO("loading the sequence-data from the song");

  g_object_get((gpointer)song,"sequence",&sequence,NULL);
  // get top xml-node
  if((items_xpoptr=xpath_type_filter(
    cxpath_get_object(song_doc,BT_SONG_IO_NATIVE_GET_CLASS(self)->xpath_get_sequence,NULL),
    XPATH_NODESET)))
  {
    xmlNodeSetPtr items=(xmlNodeSetPtr)items_xpoptr->nodesetval;
    gint items_len=xmlXPathNodeSetGetLength(items);

    GST_INFO(" got sequence root node with %d items",items_len);
    if(items_len==1) {
      xmlChar *loop_str,*loop_start_str,*loop_end_str;
      gboolean loop;
      glong loop_start,loop_end;
      
      xml_node=xmlXPathNodeSetItem(items,0);
      
      // get loop settings
      loop_str=xmlGetProp(xml_node,XML_CHAR_PTR("loop"));
      loop_start_str=xmlGetProp(xml_node,XML_CHAR_PTR("loop-start"));
      loop_end_str=xmlGetProp(xml_node,XML_CHAR_PTR("loop-end"));
      loop_start=loop_start_str?atol((char *)loop_start_str):-1;
      loop_end=loop_end_str?atol((char *)loop_end_str):-1;
      loop=loop_str?!strncasecmp((char *)loop_str,"on\0",3):FALSE;
      g_object_set(sequence,"loop",loop,"loop-start",loop_start,"loop-end",loop_end,NULL);
      xmlFree(loop_str);xmlFree(loop_start_str);xmlFree(loop_end_str);
      
      bt_song_io_native_get_sequence_length(self,song,song_doc,xml_node);
            
      bt_song_io_native_load_sequence_labels(self,song,song_doc,xml_node);
      bt_song_io_native_load_sequence_tracks(self,song,song_doc,xml_node);
    }
    xmlXPathFreeObject(items_xpoptr);
  }
  // release the reference
  g_object_try_unref(sequence);
  return(TRUE);
}

static gboolean bt_song_io_native_load_wavetable_wave(const BtSongIONative *self, const BtSong *song, BtWavetable *wavetable, xmlNodePtr root_node) {
  BtWave *wave;
  xmlChar *name,*file_name,*index_str;
    
  // load wave data
  name=xmlGetProp(root_node,XML_CHAR_PTR("name"));
  file_name=xmlGetProp(root_node,XML_CHAR_PTR("url"));
  index_str=xmlGetProp(root_node,XML_CHAR_PTR("index"));
  GST_INFO("loading the wave %s '%s' from the song",index_str,name);
  wave=bt_wave_new(song,(gchar *)name,(gchar *)file_name,atol((char *)index_str));
  // @todo load wave level data

  g_object_unref(wave);
  xmlFree(name);
  xmlFree(file_name);
  xmlFree(index_str);
  return(TRUE);
}

static gboolean bt_song_io_native_load_wavetable(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc) {
  xmlXPathObjectPtr items_xpoptr;
  xmlNodePtr xml_node;
  
  GST_INFO("loading the wavetable-data from the song");

  // get top xml-node
  if((items_xpoptr=xpath_type_filter(
        cxpath_get_object(song_doc,BT_SONG_IO_NATIVE_GET_CLASS(self)->xpath_get_wavetable,NULL),
        XPATH_NODESET)))
  {
    BtWavetable *wavetable;
    gint i;
    xmlNodeSetPtr items=(xmlNodeSetPtr)items_xpoptr->nodesetval;
    gint items_len=xmlXPathNodeSetGetLength(items);

    g_object_get((gpointer)song,"wavetable",&wavetable,NULL);

    GST_INFO(" got wavetable root node with %d items",items_len);
    for(i=0;i<items_len;i++) {
      xml_node=xmlXPathNodeSetItem(items,i);
      if((!xmlNodeIsText(xml_node)) && (!strncmp((char *)xml_node->name,"wave\0",5))) {
        bt_song_io_native_load_wavetable_wave(self,song,wavetable,xml_node);
      }
    }
    xmlXPathFreeObject(items_xpoptr);
    // release the reference
    g_object_try_unref(wavetable);
  }
  return(TRUE);
}

#endif

//-- loader method

gboolean bt_song_io_native_real_load(const gpointer _self, const BtSong *song) {
  const BtSongIONative *self=BT_SONG_IO_NATIVE(_self);
  gboolean result=FALSE;
  xmlParserCtxtPtr ctxt=NULL;
  xmlDocPtr song_doc=NULL;
  gchar *file_name,*status,*msg;
  
  g_object_get(G_OBJECT(self),"file-name",&file_name,NULL);
  GST_INFO("native io will now load song from \"%s\"",file_name);

  msg=_("Loading file \"%s\"");
  status=g_alloca(strlen(msg)+strlen(file_name));
  g_sprintf(status,msg,file_name);
  g_object_set(G_OBJECT(self),"status",status,NULL);
    
  // @todo add gnome-vfs detection method. This method should detect the
  // filetype of the given file and returns a gnome-vfs uri to open this
  // file with gnome-vfs. For example if the given file is song.xml the method
  // should return file:/home/waffel/buzztard/songs/song.xml
  
  /* @todo add zip file processing
   * zip_file=bt_zip_file_new(file_name,BT_ZIP_FILE_MODE_READ);
   * xml_doc_buffer=bt_zip_file_read_file(zip_file,"song.xml",&xml_doc_size);
   */
  
  // @todo read from zip_file
  if((ctxt=xmlNewParserCtxt())) {
    //song_doc=xmlCtxtReadMemory(ctxt,xml_doc_buffer,xml_doc_size,file_name,NULL,0L)
    if((song_doc=xmlCtxtReadFile(ctxt,file_name,NULL,0L))) {
      if(!ctxt->valid) {
        GST_WARNING("the supplied document is not a XML/Buzztard document");
      }
      else if(!ctxt->wellFormed) {
        GST_WARNING("the supplied document is not a wellformed XML document");
      }
      else {
        xmlNodePtr root_node;

        if((root_node=xmlDocGetRootElement(song_doc))==NULL) {
          GST_WARNING("xmlDoc is empty");
        }
        else if(xmlStrcmp(root_node->name,(const xmlChar *)"buzztard")) {
          GST_WARNING("wrong document type root node in xmlDoc src");
        }
        else {
#ifdef USE_OLD_LOADER
          GST_INFO("file looks good!");
          if(bt_song_io_native_load_song_info(self,song,song_doc) &&
            bt_song_io_native_load_setup(    self,song,song_doc) &&
            bt_song_io_native_load_patterns( self,song,song_doc) &&
            bt_song_io_native_load_sequence( self,song,song_doc) &&
            bt_song_io_native_load_wavetable(self,song,song_doc)
          ) {
            result=TRUE;
          }
#else
          result=bt_persistence_load(BT_PERSISTENCE(song),song_doc,root_node,NULL);
#endif
        }
      }
    }
    else GST_ERROR("failed to read song file \"%s\"",file_name);
  }
  else GST_ERROR("failed to create file-parser context");
  if(ctxt) xmlFreeParserCtxt(ctxt);
  if(song_doc) xmlFreeDoc(song_doc);
  g_free(file_name);
  g_object_set(G_OBJECT(self),"status",NULL,NULL);
  return(result);
}

//-- saver helper methods

#ifdef USE_OLD_SAVER

static void bt_song_io_native_save_property_entries(gpointer key, gpointer value, gpointer user_data) {
  xmlNodePtr xml_node;
  
  xml_node=xmlNewChild(user_data,NULL,XML_CHAR_PTR("property"),NULL);
  xmlNewProp(xml_node,XML_CHAR_PTR("key"),XML_CHAR_PTR(key));
  xmlNewProp(xml_node,XML_CHAR_PTR("value"),XML_CHAR_PTR(value));
}

static gboolean bt_song_io_native_save_properties(const BtSongIONative *self, const BtSong *song,xmlNodePtr root_node,GObject *object) {
  xmlNodePtr xml_node;
  GHashTable *properties;

  // get property hashtable from object, return if NULL
  g_object_get(object,"properties",&properties,NULL);
  if(!properties) return(TRUE);
    
  xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("properties"),NULL);
  // iterate over hashtable and store key value pairs
  g_hash_table_foreach(properties,bt_song_io_native_save_property_entries,xml_node);

  return(TRUE);
}

static gboolean bt_song_io_native_save_song_info(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  BtSongInfo *song_info;
  xmlNodePtr xml_node;
  gchar *name,*genre,*author,*info;
  gchar *create_dts,*change_dts;
  gulong bpm,tpb,bars;
  gchar num[20];
  
  GST_INFO("saving the meta-data to the song");
  g_object_get(G_OBJECT(song),"song-info",&song_info,NULL);
  
  xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("meta"),NULL);
  g_object_get(G_OBJECT(song_info),
    "name",&name,"genre",&genre,"author",&author,"info",&info,
    "create-dts",&create_dts,"change-dts",&change_dts,
    "bpm",&bpm,"tpb",&tpb,"bars",&bars,
    NULL);
  if(info) {
    xmlNewChild(xml_node,NULL,XML_CHAR_PTR("info"),XML_CHAR_PTR(info));
    g_free(info);
  }
  if(name) {
    xmlNewChild(xml_node,NULL,XML_CHAR_PTR("name"),XML_CHAR_PTR(name));
    g_free(name);
  }
  if(genre) {
    xmlNewChild(xml_node,NULL,XML_CHAR_PTR("genre"),XML_CHAR_PTR(genre));
    g_free(genre);
  }
  if(author) {
    xmlNewChild(xml_node,NULL,XML_CHAR_PTR("author"),XML_CHAR_PTR(author));
    g_free(author);
  }
  if(create_dts) {
    xmlNewChild(xml_node,NULL,XML_CHAR_PTR("create-dts"),XML_CHAR_PTR(create_dts));
    g_free(create_dts);
  }
  if(change_dts) {
    xmlNewChild(xml_node,NULL,XML_CHAR_PTR("change-dts"),XML_CHAR_PTR(change_dts));
    g_free(change_dts);
  }
  sprintf(num,"%lu",bpm);
  xmlNewChild(xml_node,NULL,XML_CHAR_PTR("bpm"),XML_CHAR_PTR(num));
  sprintf(num,"%lu",tpb);
  xmlNewChild(xml_node,NULL,XML_CHAR_PTR("tpb"),XML_CHAR_PTR(num));
  sprintf(num,"%lu",bars);
  xmlNewChild(xml_node,NULL,XML_CHAR_PTR("bars"),XML_CHAR_PTR(num));
  
  g_object_try_unref(song_info);
  return(TRUE);
}

static gboolean bt_song_io_native_save_setup_machines(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  xmlNodePtr xml_node=NULL;
  BtSetup *setup;
  BtMachine *machine;
  GList *machines,*node;
  gchar *id,*plugin_name;

  g_object_get(G_OBJECT(song),"setup",&setup,NULL);
  g_object_get(G_OBJECT(setup),"machines",&machines,NULL);
  
  for(node=machines;node;node=g_list_next(node)) {
    machine=BT_MACHINE(node->data);
    g_object_get(G_OBJECT(machine),"id",&id,"plugin-name",&plugin_name,NULL);
    if(BT_IS_PROCESSOR_MACHINE(machine)) {
      xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("processor"),NULL);
      xmlNewProp(xml_node,XML_CHAR_PTR("plugin-name"),XML_CHAR_PTR(plugin_name));
    }
    else if(BT_IS_SINK_MACHINE(machine)) {
      xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("sink"),NULL);
    }
    else if(BT_IS_SOURCE_MACHINE(machine)) {
      xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("source"),NULL);
      xmlNewProp(xml_node,XML_CHAR_PTR("plugin-name"),XML_CHAR_PTR(plugin_name));
    }
    if(xml_node) {
      xmlNewProp(xml_node,XML_CHAR_PTR("id"),XML_CHAR_PTR(id));
    }
    g_free(id);
    if(plugin_name) g_free(plugin_name);
    // save properties
    bt_song_io_native_save_properties(self,song,xml_node,G_OBJECT(machine));
    // @todo save global data
  }
  g_list_free(machines);
  g_object_try_unref(setup);
  
  return(TRUE);
}

static gboolean bt_song_io_native_save_setup_wires(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  xmlNodePtr xml_node;
  BtSetup *setup;
  BtWire *wire;
  BtMachine *src_machine,*dst_machine;
  GList *wires,*node;
  gchar *id;

  g_object_get(G_OBJECT(song),"setup",&setup,NULL);
  g_object_get(G_OBJECT(setup),"wires",&wires,NULL);
  
  for(node=wires;node;node=g_list_next(node)) {
    wire=BT_WIRE(node->data);
    
    xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("wire"),NULL);
    g_object_get(G_OBJECT(wire),"src",&src_machine,"dst",&dst_machine,NULL);

    g_object_get(G_OBJECT(src_machine),"id",&id,NULL);
    xmlNewProp(xml_node,XML_CHAR_PTR("src"),XML_CHAR_PTR(id));g_free(id);
    
    g_object_get(G_OBJECT(dst_machine),"id",&id,NULL);
    xmlNewProp(xml_node,XML_CHAR_PTR("dst"),XML_CHAR_PTR(id));g_free(id);

    g_object_try_unref(src_machine);
    g_object_try_unref(dst_machine);
  }
  g_list_free(wires);
  g_object_try_unref(setup);
  
  return(TRUE);
}

static gboolean bt_song_io_native_save_setup(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  xmlNodePtr xml_node,xml_child_node;

  xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("setup"),NULL);
  xml_child_node=xmlNewChild(xml_node,NULL,XML_CHAR_PTR("machines"),NULL);
  bt_song_io_native_save_setup_machines(self,song,song_doc,xml_child_node);
  xml_child_node=xmlNewChild(xml_node,NULL,XML_CHAR_PTR("wires"),NULL);
  bt_song_io_native_save_setup_wires(self,song,song_doc,xml_child_node);
  
  return(TRUE);
}

static gboolean bt_song_io_native_save_pattern_data(const BtSongIONative *self, const BtPattern *pattern,xmlNodePtr root_node) {
  BtMachine *machine;
  xmlNodePtr xml_node,xml_child_node;
  gulong i,j,k,length,voices,global_params,voice_params;
  const gchar *voice_str;
  gchar *value;
  
  g_object_get(G_OBJECT(pattern),"length",&length,"voices",&voices,"machine",&machine,NULL);
  g_object_get(G_OBJECT(machine),"global-params",&global_params,"voice-params",&voice_params,NULL);
  
  GST_INFO("saving the pattern-data: len=%d, global=%d, voices=%d",length,global_params,voice_params);
  
  for(i=0;i<length;i++) {
    // check if there are any GValues stored ?
    if(bt_pattern_tick_has_data(pattern,i)) {
      xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("tick"),NULL);
      xmlNewProp(xml_node,XML_CHAR_PTR("time"),XML_CHAR_PTR(strfmt_ulong(i)));
      // save tick data
      for(j=0;j<global_params;j++) {
        if((value=bt_pattern_get_global_event(pattern,i,j))) {
          xml_child_node=xmlNewChild(xml_node,NULL,XML_CHAR_PTR("globaldata"),NULL);
          xmlNewProp(xml_child_node,XML_CHAR_PTR("name"),XML_CHAR_PTR(bt_machine_get_global_param_name(machine,j)));
          xmlNewProp(xml_child_node,XML_CHAR_PTR("value"),XML_CHAR_PTR(value));g_free(value);
        }
      }
      for(j=0;j<voices;j++) {
        voice_str=strfmt_ulong(j);
        for(k=0;k<voice_params;k++) {
          if((value=bt_pattern_get_voice_event(pattern,i,j,k))) {
            xml_child_node=xmlNewChild(xml_node,NULL,XML_CHAR_PTR("voicedata"),NULL);
            xmlNewProp(xml_child_node,XML_CHAR_PTR("voice"),XML_CHAR_PTR(voice_str));
            xmlNewProp(xml_child_node,XML_CHAR_PTR("name"),XML_CHAR_PTR(bt_machine_get_voice_param_name(machine,j)));
            xmlNewProp(xml_child_node,XML_CHAR_PTR("value"),XML_CHAR_PTR(value));g_free(value);
          }
        }
      }
    }
  }
  g_object_unref(machine);
  return(TRUE);
}

static gboolean bt_song_io_native_save_patterns(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  xmlNodePtr xml_node,xml_child_node;
  BtSetup *setup;
  BtMachine *machine;
  BtPattern *pattern;
  GList *machines,*patterns,*node1,*node2;
  gchar *id,*machine_id,*name;
  gulong length;

  xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("patterns"),NULL);

  g_object_get(G_OBJECT(song),"setup",&setup,NULL);
  g_object_get(G_OBJECT(setup),"machines",&machines,NULL);
  
  for(node1=machines;node1;node1=g_list_next(node1)) {
    machine=BT_MACHINE(node1->data);
    g_object_get(G_OBJECT(machine),"id",&machine_id,"patterns",&patterns,NULL);
    // save all patterns for this machine  
    for(node2=patterns;node2;node2=g_list_next(node2)) {
      pattern=BT_PATTERN(node2->data);
      g_object_get(G_OBJECT(pattern),"name",&name,"length",&length,NULL);
      
      xml_child_node=xmlNewChild(xml_node,NULL,XML_CHAR_PTR("pattern"),NULL);
      // generate "id"
      id=g_alloca(strlen(machine_id)+strlen(name)+2);
      g_sprintf(id,"%s_%s",machine_id,name);
      g_object_set(G_OBJECT(pattern),"id",id,NULL);
      // save attributes
      xmlNewProp(xml_child_node,XML_CHAR_PTR("id"),XML_CHAR_PTR(id));
      xmlNewProp(xml_child_node,XML_CHAR_PTR("machine"),XML_CHAR_PTR(machine_id));
      xmlNewProp(xml_child_node,XML_CHAR_PTR("name"),XML_CHAR_PTR(name));g_free(name);
      xmlNewProp(xml_child_node,XML_CHAR_PTR("length"),XML_CHAR_PTR(strfmt_ulong(length)));
      // save tick data
      bt_song_io_native_save_pattern_data(self,pattern,xml_child_node);
    }
    g_list_free(patterns);
    g_free(machine_id);
  }
  g_list_free(machines);
  g_object_try_unref(setup);

  return(TRUE);
}

static gboolean bt_song_io_native_save_sequence_labels(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  xmlNodePtr xml_node;
  BtSequence *sequence;
  gchar *label;
  gulong i,length;
  
  g_object_get(G_OBJECT(song),"sequence",&sequence,NULL);
  g_object_get(G_OBJECT(sequence),"length",&length,NULL);

  // iterate over timelines
  for(i=0;i<length;i++) {
    if((label=bt_sequence_get_label(sequence,i))) {
      xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("label"),NULL);
      xmlNewProp(xml_node,XML_CHAR_PTR("name"),XML_CHAR_PTR(label));g_free(label);
      xmlNewProp(xml_node,XML_CHAR_PTR("time"),XML_CHAR_PTR(strfmt_ulong(i)));
    }
  }
  g_object_try_unref(sequence);
  
  return(TRUE);
}

static gboolean bt_song_io_native_save_sequence_tracks(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  xmlNodePtr xml_node,xml_child_node;
  BtSequence *sequence;
  BtMachine *machine;
  BtPattern *pattern;
  gchar *machine_id,*pattern_id;
  gulong i,j,length,tracks;
  
  g_object_get(G_OBJECT(song),"sequence",&sequence,NULL);
  g_object_get(G_OBJECT(sequence),"length",&length,"tracks",&tracks,NULL);

  // iterate over tracks
  for(j=0;j<tracks;j++) {
    xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("track"),NULL);
    machine=bt_sequence_get_machine(sequence,j);
    g_object_get(G_OBJECT(machine),"id",&machine_id,NULL);
    xmlNewProp(xml_node,XML_CHAR_PTR("index"),XML_CHAR_PTR(strfmt_ulong(j)));
    xmlNewProp(xml_node,XML_CHAR_PTR("machine"),XML_CHAR_PTR(machine_id));g_free(machine_id);
    g_object_unref(machine);
    // iterate over timelines
    for(i=0;i<length;i++) {
      // get pattern
      if((pattern=bt_sequence_get_pattern(sequence,i,j))) {
        g_object_get(G_OBJECT(pattern),"id",&pattern_id,NULL);
        xml_child_node=xmlNewChild(xml_node,NULL,XML_CHAR_PTR("position"),NULL);
        xmlNewProp(xml_child_node,XML_CHAR_PTR("time"),XML_CHAR_PTR(strfmt_ulong(i)));
        xmlNewProp(xml_child_node,XML_CHAR_PTR("pattern"),XML_CHAR_PTR(pattern_id));g_free(pattern_id);
        g_object_unref(pattern);
      }
    }
  }
  g_object_try_unref(sequence);
  
  return(TRUE);
}

static gboolean bt_song_io_native_save_sequence(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  xmlNodePtr xml_node,xml_child_node;
  BtSequence *sequence;
  gboolean loop;
  glong loop_start,loop_end;
  
  g_object_get(G_OBJECT(song),"sequence",&sequence,NULL);
  g_object_get(sequence,"loop",&loop,"loop-start",&loop_start,"loop-end",&loop_end,NULL);
  g_object_try_unref(sequence);

  xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("sequence"),NULL);
  if(loop) {
    xmlNewProp(xml_node,XML_CHAR_PTR("loop"),XML_CHAR_PTR("on"));
    xmlNewProp(xml_node,XML_CHAR_PTR("loop-start"),XML_CHAR_PTR(strfmt_long(loop_start)));
    xmlNewProp(xml_node,XML_CHAR_PTR("loop-end"),XML_CHAR_PTR(strfmt_long(loop_end)));
  }
  
  xml_child_node=xmlNewChild(xml_node,NULL,XML_CHAR_PTR("labels"),NULL);
  bt_song_io_native_save_sequence_labels(self,song,song_doc,xml_child_node);

  xml_child_node=xmlNewChild(xml_node,NULL,XML_CHAR_PTR("tracks"),NULL);
  bt_song_io_native_save_sequence_tracks(self,song,song_doc,xml_child_node);

  return(TRUE);
}

static gboolean bt_song_io_native_save_wavetable(const BtSongIONative *self, const BtSong *song, const xmlDocPtr song_doc,xmlNodePtr root_node) {
  BtWavetable *wavetable;
  BtWave *wave;
  xmlNodePtr xml_node;
  GList *waves,*node;
  gulong index;
  gchar *name,*file_name;

  xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("wavetable"),NULL);
  g_object_get(G_OBJECT(song),"wavetable",&wavetable,NULL);
  g_object_get(G_OBJECT(wavetable),"waves",&waves,NULL);
  
  for(node=waves;node;node=g_list_next(node)) {
    wave=BT_WAVE(node->data);
    
    xml_node=xmlNewChild(root_node,NULL,XML_CHAR_PTR("wave"),NULL);
    g_object_get(G_OBJECT(wave),"index",&index,"name",&name,"file-name",&file_name,NULL);
    xmlNewProp(xml_node,XML_CHAR_PTR("index"),XML_CHAR_PTR(strfmt_ulong(index)));
    xmlNewProp(xml_node,XML_CHAR_PTR("name"),XML_CHAR_PTR(name));g_free(name);
    xmlNewProp(xml_node,XML_CHAR_PTR("url"),XML_CHAR_PTR(file_name));g_free(file_name);
    
    // @todo save wavelevels
  }
  g_list_free(waves);
  g_object_unref(wavetable);

  return(TRUE);
}

#endif

//-- saver method

gboolean bt_song_io_native_real_save(const gpointer _self, const BtSong *song) {
  const BtSongIONative *self=BT_SONG_IO_NATIVE(_self);
  gboolean result=FALSE;
  xmlDocPtr song_doc=NULL;
  gchar *file_name,*status,*msg;
  
  g_object_get(G_OBJECT(self),"file-name",&file_name,NULL);
  GST_INFO("native io will now save song to \"%s\"",file_name);

  msg=_("Saving file \"%s\"");
  status=g_alloca(strlen(msg)+strlen(file_name));
  g_sprintf(status,msg,file_name);
  g_object_set(G_OBJECT(self),"status",status,NULL);

  if((song_doc=xmlNewDoc(XML_CHAR_PTR("1.0")))) {
#ifdef USE_OLD_SAVER
    xmlNodePtr root_node=NULL;

    // create the root-node
    root_node=xmlNewNode(NULL,XML_CHAR_PTR("buzztard"));
    xmlNewProp(root_node,XML_CHAR_PTR("xmlns"),(const xmlChar *)BT_NS_URL);
    xmlNewProp(root_node,XML_CHAR_PTR("xmlns:xsd"),XML_CHAR_PTR("http://www.w3.org/2001/XMLSchema-instance"));
    xmlNewProp(root_node,XML_CHAR_PTR("xsd:noNamespaceSchemaLocation"),XML_CHAR_PTR("buzztard.xsd"));
    xmlDocSetRootElement(song_doc,root_node);
    // build the xml document tree
    if(bt_song_io_native_save_song_info(self,song,song_doc,root_node) &&
      bt_song_io_native_save_setup(    self,song,song_doc,root_node) &&
      bt_song_io_native_save_patterns( self,song,song_doc,root_node) &&
      bt_song_io_native_save_sequence( self,song,song_doc,root_node) &&
      bt_song_io_native_save_wavetable(self,song,song_doc,root_node)
    ) {
      if(xmlSaveFile(file_name,song_doc)!=-1) {
        result=TRUE;
      }
      else GST_ERROR("failed to write song file \"%s\"",file_name);
    }
#else
    if(bt_persistence_save(BT_PERSISTENCE(song),song_doc,NULL,NULL)) {
      if(xmlSaveFile(file_name,song_doc)!=-1) {
        result=TRUE;
      }
      else GST_ERROR("failed to write song file \"%s\"",file_name);
    }
#endif
  }
  
  g_free(file_name);

  g_object_set(G_OBJECT(self),"status",NULL,NULL);
  return(result);
}

//-- wrapper

//-- class internals

/* returns a property for the given property_id for this object */
static void bt_song_io_native_get_property(GObject      *object,
                               guint         property_id,
                               GValue       *value,
                               GParamSpec   *pspec)
{
  BtSongIONative *self = BT_SONG_IO_NATIVE(object);
  return_if_disposed();
  switch (property_id) {
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

/* sets the given properties for this object */
static void bt_song_io_native_set_property(GObject      *object,
                              guint         property_id,
                              const GValue *value,
                              GParamSpec   *pspec)
{
  BtSongIONative *self = BT_SONG_IO_NATIVE(object);
  return_if_disposed();
  switch (property_id) {
    default: {
      G_OBJECT_WARN_INVALID_PROPERTY_ID(object,property_id,pspec);
    } break;
  }
}

static void bt_song_io_native_dispose(GObject *object) {
  BtSongIONative *self = BT_SONG_IO_NATIVE(object);

  return_if_disposed();
  self->priv->dispose_has_run = TRUE;

  GST_DEBUG("!!!! self=%p",self);
  if(G_OBJECT_CLASS(parent_class)->dispose) {
    (G_OBJECT_CLASS(parent_class)->dispose)(object);
  }
}

static void bt_song_io_native_finalize(GObject *object) {
  BtSongIONative *self = BT_SONG_IO_NATIVE(object);

  GST_DEBUG("!!!! self=%p",self);

  if(G_OBJECT_CLASS(parent_class)->finalize) {
    (G_OBJECT_CLASS(parent_class)->finalize)(object);
  }
}

static void bt_song_io_native_init(GTypeInstance *instance, gpointer g_class) {
  BtSongIONative *self = BT_SONG_IO_NATIVE(instance);
  
  self->priv = G_TYPE_INSTANCE_GET_PRIVATE(self, BT_TYPE_SONG_IO_NATIVE, BtSongIONativePrivate);
}

static void bt_song_io_native_class_init(BtSongIONativeClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
  BtSongIOClass *base_class = BT_SONG_IO_CLASS(klass);

  error_domain=g_quark_from_static_string("BtSongIONative");
  g_type_class_add_private(klass,sizeof(BtSongIONativePrivate));

  parent_class=g_type_class_ref(BT_TYPE_SONG_IO);
  
  gobject_class->set_property = bt_song_io_native_set_property;
  gobject_class->get_property = bt_song_io_native_get_property;
  gobject_class->dispose      = bt_song_io_native_dispose;
  gobject_class->finalize     = bt_song_io_native_finalize;
  
  /* implement virtual class function. */
  base_class->load       = bt_song_io_native_real_load;
  base_class->save       = bt_song_io_native_real_save;
  
  /* compile xpath-expressions */
  klass->xpath_get_meta = xmlXPathCompile(XML_CHAR_PTR("/"BT_NS_PREFIX":buzztard/"BT_NS_PREFIX":meta/"BT_NS_PREFIX":*"));
  g_assert(klass->xpath_get_meta);
  klass->xpath_get_setup = xmlXPathCompile(XML_CHAR_PTR("/"BT_NS_PREFIX":buzztard/"BT_NS_PREFIX":setup/"BT_NS_PREFIX":*"));
  g_assert(klass->xpath_get_setup);
  klass->xpath_get_patterns = xmlXPathCompile(XML_CHAR_PTR("/"BT_NS_PREFIX":buzztard/"BT_NS_PREFIX":patterns/"BT_NS_PREFIX":*"));
  g_assert(klass->xpath_get_patterns);
  klass->xpath_get_sequence = xmlXPathCompile(XML_CHAR_PTR("/"BT_NS_PREFIX":buzztard/"BT_NS_PREFIX":sequence"));
  g_assert(klass->xpath_get_sequence);
  klass->xpath_get_sequence_labels = xmlXPathCompile(XML_CHAR_PTR("./"BT_NS_PREFIX":labels/"BT_NS_PREFIX":*"));
  g_assert(klass->xpath_get_sequence);
  klass->xpath_get_sequence_tracks = xmlXPathCompile(XML_CHAR_PTR("./"BT_NS_PREFIX":tracks/"BT_NS_PREFIX":*"));
  g_assert(klass->xpath_get_sequence);
  klass->xpath_get_sequence_length = xmlXPathCompile(XML_CHAR_PTR("./"BT_NS_PREFIX":labels/"BT_NS_PREFIX":label/@time|./"BT_NS_PREFIX":tracks/"BT_NS_PREFIX":track/"BT_NS_PREFIX":position/@time"));
  g_assert(klass->xpath_get_sequence_length);
  //klass->xpath_count_sequence_tracks = xmlXPathCompile(XML_CHAR_PTR("count(./"BT_NS_PREFIX":tracks/"BT_NS_PREFIX":track)"));
  //g_assert(klass->xpath_count_sequence_tracks);
  klass->xpath_get_wavetable = xmlXPathCompile(XML_CHAR_PTR("/"BT_NS_PREFIX":buzztard/"BT_NS_PREFIX":wavetable/"BT_NS_PREFIX":*"));
  g_assert(klass->xpath_get_wavetable);
}

/* as of gobject documentation, static types are keept alive until the program ends.
   therefore we do not free shared class-data
static void bt_song_io_native_class_finalize(BtSongIOClass *klass) {
  GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

  xmlXPathFreeCompExpr(gobject_class->xpath_get_meta);
  gobject_class->xpath_get_meta=NULL;
}
*/

GType bt_song_io_native_get_type(void) {
  static GType type = 0;
  if (G_UNLIKELY(type == 0)) {
    static const GTypeInfo info = {
      G_STRUCT_SIZE(BtSongIONativeClass),
      NULL, // base_init
      NULL, // base_finalize
      (GClassInitFunc)bt_song_io_native_class_init, // class_init
      NULL, // class_finalize
      //(GClassFinalizeFunc)bt_song_io_native_class_finalize,
      NULL, // class_data
      G_STRUCT_SIZE(BtSongIONative),
      0,   // n_preallocs
      (GInstanceInitFunc)bt_song_io_native_init, // instance_init
      NULL // value_table
    };
    type = g_type_register_static(BT_TYPE_SONG_IO,"BtSongIONative",&info,0);
  }
  return type;
}
