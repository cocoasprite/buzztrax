/* $Id: tools.c,v 1.13 2004-12-06 20:18:47 waffel Exp $
 */
 
#define BT_CORE
#define BT_TOOLS_C
#include <libbtcore/core.h>

/**
 * bt_gst_registry_get_element_names_by_class:
 * @class_filter: path for filtering (e.g. "Sink/Audio")
 *
 * Iterates over all available plugins and filters by class-name.
 * The matching uses right-truncation, e.g. "Sink/" matches all sorts of sinks.
 *
 * Returns: list of element names, g_list_free after use.
 */
GList *bt_gst_registry_get_element_names_by_class(gchar *class_filter) {
  GList *elements,*node;
  GList *res=NULL;
  GstElementFactory *element;
  gsize sl;

  g_assert(class_filter);
  
  sl=(gsize)(strlen(class_filter));
  
  elements=node=gst_registry_pool_feature_list(GST_TYPE_ELEMENT_FACTORY);
  while(node) {
    element=GST_ELEMENT_FACTORY(node->data);

    //GST_DEBUG("  %s: %s", GST_OBJECT_NAME(element),gst_element_factory_get_klass(element));
    if(!g_ascii_strncasecmp(gst_element_factory_get_klass(element),class_filter,sl)) {
      // @todo: consider  gst_plugin_feature_get_rank() for sorting
      res=g_list_append(res,(gchar *)gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(element)));
    }
    node=g_list_next(node);
  }
  g_list_free(elements);
  return(res);
}
