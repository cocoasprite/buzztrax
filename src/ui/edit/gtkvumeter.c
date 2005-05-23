/***************************************************************************
 *            gtkvumeter.c
 *
 *  Fri Jan 10 20:06:23 2003
 *  Copyright  2003  Todd Goyen
 *  wettoad@knighthoodofbuh.org
 ****************************************************************************/

#include <math.h>
#include <gtk/gtk.h>
#include "gtkvumeter.h"

#define MIN_HORIZONTAL_VUMETER_WIDTH   400
#define HORIZONTAL_VUMETER_HEIGHT  8
#define VERTICAL_VUMETER_WIDTH     8
#define MIN_VERTICAL_VUMETER_HEIGHT    400

static void gtk_vumeter_init (GtkVUMeter *vumeter);
static void gtk_vumeter_class_init (GtkVUMeterClass *class);
static void gtk_vumeter_destroy (GtkObject *object);
static void gtk_vumeter_realize (GtkWidget *widget);
static void gtk_vumeter_size_request (GtkWidget *widget, GtkRequisition *requisition);
static void gtk_vumeter_size_allocate (GtkWidget *widget, GtkAllocation *allocation);
static gint gtk_vumeter_expose (GtkWidget *widget, GdkEventExpose *event);
static void gtk_vumeter_free_colors (GtkVUMeter *vumeter);
static void gtk_vumeter_setup_colors (GtkVUMeter *vumeter);
static gint gtk_vumeter_sound_level_to_draw_level (GtkVUMeter *vumeter, gint level);

static GtkWidgetClass *parent_class = NULL;

GtkType gtk_vumeter_get_type (void)
{
    static GType vumeter_type = 0;
    
    if (!vumeter_type) {
        static const GTypeInfo vumeter_info = {
            sizeof (GtkVUMeterClass),
            NULL, NULL,
            (GClassInitFunc) gtk_vumeter_class_init,
            NULL, NULL, sizeof (GtkVUMeter), 0,
            (GInstanceInitFunc) gtk_vumeter_init,
        };
        vumeter_type = g_type_register_static (GTK_TYPE_WIDGET, "GtkVUMeter", &vumeter_info, 0);
    }

    return vumeter_type;
}

GtkWidget* gtk_vumeter_new (gboolean vertical)
{
    GtkVUMeter *vumeter;
    vumeter = GTK_VUMETER( g_object_new (GTK_TYPE_VUMETER,NULL));
    vumeter->vertical = vertical;
    return GTK_WIDGET (vumeter);
}

static void gtk_vumeter_init (GtkVUMeter *vumeter)
{
    vumeter->colormap = NULL;
    vumeter->colors = 0;
    vumeter->f_gc = NULL;
    vumeter->b_gc = NULL;
    vumeter->f_colors = NULL;
    vumeter->b_colors = NULL;

    vumeter->rms_level = 0;
    vumeter->min = 0;
    vumeter->max = 32767;
    vumeter->peaks_falloff = GTK_VUMETER_PEAKS_FALLOFF_MEDIUM;
    vumeter->peak_level = 0;
    vumeter->delay_peak_level = 0;

    vumeter->scale = GTK_VUMETER_SCALE_LINEAR;
}

static void gtk_vumeter_class_init (GtkVUMeterClass *class)
{
    GtkObjectClass *object_class;
    GtkWidgetClass *widget_class;

    object_class = (GtkObjectClass*) class;
    widget_class = (GtkWidgetClass*) class;
    parent_class = gtk_type_class(gtk_widget_get_type());

    object_class->destroy = gtk_vumeter_destroy;
    
    widget_class->realize = gtk_vumeter_realize;
    widget_class->expose_event = gtk_vumeter_expose;
    widget_class->size_request = gtk_vumeter_size_request;
    widget_class->size_allocate = gtk_vumeter_size_allocate;    
}

static void gtk_vumeter_destroy (GtkObject *object)
{
    GtkVUMeter *vumeter = GTK_VUMETER (object);

    gtk_vumeter_free_colors (vumeter);
    
    GTK_OBJECT_CLASS (parent_class)->destroy (object);
}

static void gtk_vumeter_realize (GtkWidget *widget)
{
    GtkVUMeter *vumeter;
    GdkWindowAttr attributes;
    gint attributes_mask;
    gint i;
    
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_VUMETER (widget));

    GTK_WIDGET_SET_FLAGS (widget, GTK_REALIZED);
    vumeter = GTK_VUMETER (widget);

    attributes.x = widget->allocation.x;
    attributes.y = widget->allocation.y;
    attributes.width = widget->allocation.width;
    attributes.height = widget->allocation.height;
    attributes.wclass = GDK_INPUT_OUTPUT;
    attributes.window_type = GDK_WINDOW_CHILD;
    attributes.event_mask = gtk_widget_get_events (widget) | GDK_EXPOSURE_MASK;
    attributes.visual = gtk_widget_get_visual (widget);
    attributes.colormap = gtk_widget_get_colormap (widget);
    attributes_mask = GDK_WA_X | GDK_WA_Y | GDK_WA_VISUAL | GDK_WA_COLORMAP;
    widget->window = gdk_window_new (widget->parent->window, &attributes, attributes_mask);

    widget->style = gtk_style_attach (widget->style, widget->window);

    gdk_window_set_user_data (widget->window, widget);
    gtk_style_set_background (widget->style, widget->window,  GTK_STATE_NORMAL);
    
    /* colors */
    vumeter->colormap = gdk_colormap_get_system ();
    gtk_vumeter_setup_colors (vumeter);
}

static void gtk_vumeter_size_request (GtkWidget *widget, GtkRequisition *requisition)
{
    GtkVUMeter *vumeter;

    g_return_if_fail (GTK_IS_VUMETER (widget));
    g_return_if_fail (requisition != NULL);

    vumeter = GTK_VUMETER (widget);

    if (vumeter->vertical == TRUE) {
        requisition->width = VERTICAL_VUMETER_WIDTH;
        requisition->height = MIN_VERTICAL_VUMETER_HEIGHT;
    } else {
        requisition->width = MIN_HORIZONTAL_VUMETER_WIDTH;
        requisition->height = HORIZONTAL_VUMETER_HEIGHT;        
    }
}

static void gtk_vumeter_size_allocate (GtkWidget *widget, GtkAllocation *allocation)
{
    GtkVUMeter *vumeter;
    gint red, green;
    gint f_step;
    gint b_step;
    
    g_return_if_fail (widget != NULL);
    g_return_if_fail (GTK_IS_VUMETER (widget));
    g_return_if_fail (allocation != NULL);

    widget->allocation = *allocation;
    vumeter = GTK_VUMETER (widget);

    if (GTK_WIDGET_REALIZED (widget)) {
        if (vumeter->vertical == TRUE) { /* veritcal */
            gdk_window_move_resize (widget->window, allocation->x, allocation->y,
                MIN(allocation->width,VERTICAL_VUMETER_WIDTH), MAX(allocation->height, MIN_VERTICAL_VUMETER_HEIGHT));
        } else { /* horizontal */
            gdk_window_move_resize (widget->window, allocation->x, allocation->y,
                MAX(allocation->width, MIN_HORIZONTAL_VUMETER_WIDTH), MIN(allocation->height,HORIZONTAL_VUMETER_HEIGHT));
        }
        /* Fix the colours */
        gtk_vumeter_setup_colors (vumeter);
    }
}

static gint gtk_vumeter_expose (GtkWidget *widget, GdkEventExpose *event)
{
    GtkVUMeter *vumeter;
    gint index, rms_level, peak_level;
    gint width, height;
    
    g_return_val_if_fail (widget != NULL, FALSE);
    g_return_val_if_fail (GTK_IS_VUMETER (widget), FALSE);
    g_return_val_if_fail (event != NULL, FALSE);

    if (event->count > 0)
        return FALSE;

    vumeter = GTK_VUMETER (widget);
    rms_level = gtk_vumeter_sound_level_to_draw_level (vumeter, 
						       vumeter->rms_level);
    peak_level = gtk_vumeter_sound_level_to_draw_level (vumeter, 
						       vumeter->peak_level);
    if (vumeter->vertical == TRUE) {
        width = widget->allocation.width - 2;
        height = widget->allocation.height;

        /* draw border */
        gtk_paint_box (widget->style, widget->window, GTK_STATE_NORMAL, GTK_SHADOW_IN, 
            NULL, widget, "trough", 0, 0, widget->allocation.width, height);
        /* draw background gradient */
        for (index = rms_level; index < peak_level; index++) {
            gdk_draw_line (widget->window, vumeter->f_gc[index], 1, index + 1, width, index + 1);   
        }
        /* draw foreground gradient */
        for (index = 0; index < rms_level; index++) {
            gdk_draw_line (widget->window, vumeter->b_gc[index], 1, index + 1, width, index + 1);            
        }
    } else { /* Horizontal */
        width = widget->allocation.width;
        height = widget->allocation.height - 2;

        /* draw border */
        gtk_paint_box (widget->style, widget->window, GTK_STATE_NORMAL, GTK_SHADOW_IN, 
            NULL, widget, "trough", 0, 0, width, widget->allocation.height);
        /* draw background gradient */
        for (index = rms_level; index < peak_level; index++) {
	  
	  gdk_draw_line (widget->window, vumeter->b_gc[index], width - index - 1, 1, width - index - 1, height);   
        }
        /* draw foreground gradient */
        for (index = peak_level; index < width - 2; index++) {
	  gdk_draw_line (widget->window, vumeter->f_gc[index], width - index - 1, 1, width - index - 1, height);            
        }        
    }
}

static void gtk_vumeter_free_colors (GtkVUMeter *vumeter)
{
    gint index;

    /* Free old gc's */
    if (vumeter->f_gc && vumeter->b_gc) {
        for (index = 0; index < vumeter->colors; index++) {    
            if (vumeter->f_gc[index]) {
                g_object_unref (G_OBJECT(vumeter->f_gc[index]));
            }
            if (vumeter->b_gc[index]) {        
                g_object_unref (G_OBJECT(vumeter->b_gc[index]));
            }
        }
        g_free(vumeter->f_gc);
        g_free(vumeter->b_gc);
        vumeter->f_gc = NULL;
        vumeter->b_gc = NULL;
    }
    
    /* Free old Colors */
    if (vumeter->f_colors) {
        gdk_colormap_free_colors (vumeter->colormap, vumeter->f_colors, vumeter->colors);
        g_free (vumeter->f_colors);
        vumeter->f_colors = NULL;
    }
    if (vumeter->b_colors) {
        gdk_colormap_free_colors (vumeter->colormap, vumeter->b_colors, vumeter->colors);
        g_free (vumeter->b_colors);
        vumeter->b_colors = NULL;
    }
}

static void gtk_vumeter_setup_colors (GtkVUMeter *vumeter)
{
    gint index;
    gint f_step, b_step;
    gint first, second;
    gint max = 0, min = 0, log_max = 0;
    
    g_return_if_fail (vumeter->colormap != NULL);
    
    gtk_vumeter_free_colors (vumeter);
    
    /* Set new size */
    if (vumeter->vertical == TRUE) {
        vumeter->colors = MAX(GTK_WIDGET(vumeter)->allocation.height - 2, 0);
    } else {
        vumeter->colors = MAX(GTK_WIDGET(vumeter)->allocation.width - 2, 0);
    }
   
    if (vumeter->colors == 0)
      return;
    /* allocate new memory */
    vumeter->f_colors = g_malloc (vumeter->colors * sizeof(GdkColor));
    vumeter->b_colors = g_malloc (vumeter->colors * sizeof(GdkColor));    
    vumeter->f_gc = g_malloc (vumeter->colors * sizeof(GdkGC *));
    vumeter->b_gc = g_malloc (vumeter->colors * sizeof(GdkGC *));    
    
    /* Initialize stuff */
    if (vumeter->scale == GTK_VUMETER_SCALE_LINEAR) {
        first = vumeter->colors / 2;
        second = vumeter->colors;      
    } else {
        max = (gdouble)vumeter->max;
        min = (gdouble)vumeter->min;
        log_max = - 20.0 * log10(1.0/(max - min + 1.0));
        first = (gint)((gdouble)vumeter->colors * 6.0 / log_max);
        second = (gint)((gdouble)vumeter->colors * 18.0 / log_max);
    }

    vumeter->f_colors[0].red = 65535;
    vumeter->f_colors[0].green = 0;
    vumeter->f_colors[0].blue = 0;

    vumeter->b_colors[0].red = 49151;
    vumeter->b_colors[0].green = 0;
    vumeter->b_colors[0].blue = 0;

    /* Allocate from Red to Yellow */
    f_step = 65535 / (first - 1);
    b_step = 49151 / (first - 1);
    for (index = 1; index < first; index++) {
        /* foreground */
        vumeter->f_colors[index].red = 65535;
        vumeter->f_colors[index].green = vumeter->f_colors[index - 1].green + f_step;
        vumeter->f_colors[index].blue = 0;
        /* background */
        vumeter->b_colors[index].red = 49151;
        vumeter->b_colors[index].green = vumeter->b_colors[index - 1].green + b_step;
        vumeter->b_colors[index].blue = 0;
    }
    /* Allocate from Yellow to Green */    
    f_step = 65535 / (second - first);
    b_step = 49151 / (second - first);        
    for (index = first; index < second; index++) {
        /* foreground */
        vumeter->f_colors[index].red = vumeter->f_colors[index - 1].red - f_step;
        vumeter->f_colors[index].green = vumeter->f_colors[index - 1].green;
        vumeter->f_colors[index].blue = 0;
        /* background */
        vumeter->b_colors[index].red = vumeter->b_colors[index - 1].red - b_step;
        vumeter->b_colors[index].green = vumeter->b_colors[index - 1].green;
        vumeter->b_colors[index].blue = 0;     
    }
    if (vumeter->scale == GTK_VUMETER_SCALE_LOG) {
        /* Allocate from Green to Dark Green */
        f_step = 32767 / (vumeter->colors - second);
        b_step = 32767 / (vumeter->colors - second);     
        for (index = second; index < vumeter->colors; index++) {
            /* foreground */
            vumeter->f_colors[index].red = 0;
            vumeter->f_colors[index].green = vumeter->f_colors[index - 1].green - f_step;
            vumeter->f_colors[index].blue = 0;
            /* background */
            vumeter->b_colors[index].red = 0;
            vumeter->b_colors[index].green = vumeter->b_colors[index - 1].green - b_step;
            vumeter->b_colors[index].blue = 0;      
        }
    }
    /* Allocate the Colours */
    for (index = 0; index < vumeter->colors; index++) {
        /* foreground */
        gdk_colormap_alloc_color (vumeter->colormap, &vumeter->f_colors[index], FALSE, TRUE);
        vumeter->f_gc[index] = gdk_gc_new(GTK_WIDGET(vumeter)->window);
        gdk_gc_set_foreground(vumeter->f_gc[index], &vumeter->f_colors[index]);
        /* background */
        gdk_colormap_alloc_color (vumeter->colormap, &vumeter->b_colors[index], FALSE, TRUE);
        vumeter->b_gc[index] = gdk_gc_new(GTK_WIDGET(vumeter)->window);
        gdk_gc_set_foreground(vumeter->b_gc[index], &vumeter->b_colors[index]);        
    }
}

static gint gtk_vumeter_sound_level_to_draw_level (GtkVUMeter *vumeter, 
						   gint sound_level)
{
    gdouble draw_level;
    gdouble level, min, max, height;
    gdouble log_level, log_max;
    
    level = (gdouble)sound_level;
    min = (gdouble)vumeter->min;
    max = (gdouble)vumeter->max;
    height = (gdouble)vumeter->colors;
    
    if (vumeter->scale == GTK_VUMETER_SCALE_LINEAR) {
        draw_level = (1.0 - (level - min)/(max - min)) * height;
    } else {
        log_level = log10((level - min + 1)/(max - min + 1));
        log_max = log10(1/(max - min + 1));
        draw_level = log_level/log_max * height;
    }
    
    return ((gint)draw_level);
}

void gtk_vumeter_set_min_max (GtkVUMeter *vumeter, gint min, gint max)
{
    g_return_if_fail (vumeter != NULL);
    g_return_if_fail (GTK_IS_VUMETER (vumeter));

    vumeter->max = MAX(max, min);
    vumeter->min = MIN(min, max);
    if (vumeter->max == vumeter->min) {
	    vumeter->max++;
    }
    vumeter->rms_level = CLAMP (vumeter->rms_level, vumeter->min, vumeter->max);
    vumeter->peak_level = CLAMP (vumeter->peak_level, vumeter->min, vumeter->max);
    gtk_widget_queue_draw (GTK_WIDGET(vumeter)); 
}

void gtk_vumeter_set_levels (GtkVUMeter *vumeter, gint rms, gint peak)
{
    g_return_if_fail (vumeter != NULL);
    g_return_if_fail (GTK_IS_VUMETER (vumeter));

    vumeter->rms_level = CLAMP (rms, vumeter->min, vumeter->max);
    vumeter->peak_level = CLAMP (peak, vumeter->min, vumeter->max);

    gtk_widget_queue_draw (GTK_WIDGET(vumeter));    
}

void gtk_vumeter_set_peaks_falloff (GtkVUMeter *vumeter, gint peaks_falloff)
{
    g_return_if_fail (vumeter != NULL);
    g_return_if_fail (GTK_IS_VUMETER (vumeter));  
}

void gtk_vumeter_set_scale (GtkVUMeter *vumeter, gint scale)
{
    g_return_if_fail (vumeter != NULL);
    g_return_if_fail (GTK_IS_VUMETER (vumeter));  
    
    if (scale != vumeter->scale) {
        vumeter->scale = CLAMP(scale, GTK_VUMETER_SCALE_LINEAR, GTK_VUMETER_SCALE_LAST - 1);
        if (GTK_WIDGET_REALIZED(vumeter)) {
            gtk_vumeter_setup_colors (vumeter);
            gtk_widget_queue_draw (GTK_WIDGET(vumeter));
        }            
    }
}
