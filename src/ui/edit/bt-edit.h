/* $Id: bt-edit.h,v 1.53 2007-07-23 18:49:24 ensonic Exp $
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

#ifndef BT_EDIT_H
#define BT_EDIT_H

#include <math.h>
#include <stdio.h>

//-- libbtcore & libbtic
#include <libbtcore/core.h>
#include <libbtic/ic.h>
//-- gstreamer
#include <gst/musicenums/musicenums.h>
#include <gst/help/help.h>
#include <gst/preset/preset.h>
//-- gtk+
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>
//-- libgnomecanvas
#include <libgnomecanvas/libgnomecanvas.h>
//-- libgnome
#ifdef USE_GNOME
#include <libgnome/gnome-init.h>
#include <libgnome/gnome-help.h>
#include <libgnome/gnome-url.h>
#endif
//-- hildon
#ifdef USE_HILDON
#include <hildon-widgets/hildon-program.h>
#endif

#include "about-dialog-methods.h"
#include "edit-application-methods.h"
#include "interaction-controller-menu-methods.h"
#include "machine-canvas-item-methods.h"
#include "machine-menu-methods.h"
#include "machine-actions.h"
#include "machine-preset-properties-dialog-methods.h"
#include "machine-properties-dialog-methods.h"
#include "machine-preferences-dialog-methods.h"
#include "machine-rename-dialog-methods.h"
#include "main-menu-methods.h"
#include "main-pages-methods.h"
#include "main-page-machines-methods.h"
#include "main-page-patterns-methods.h"
#include "main-page-sequence-methods.h"
#include "main-page-waves-methods.h"
#include "main-page-info-methods.h"
#include "main-statusbar-methods.h"
#include "main-toolbar-methods.h"
#include "main-window-methods.h"
#include "missing-framework-elements-dialog-methods.h"
#include "missing-song-elements-dialog-methods.h"
#include "pattern-properties-dialog-methods.h"
#include "pattern-view-methods.h"
#include "playback-controller-socket-methods.h"
#include "render-dialog-methods.h"
#include "render-progress-methods.h"
#include "sequence-view-methods.h"
#include "settings-dialog-methods.h"
#include "settings-page-audiodevices-methods.h"
#include "settings-page-interaction-controller-methods.h"
#include "settings-page-playback-controller-methods.h"
#include "tools.h"
#include "ui-ressources-methods.h"
#include "volume-popup.h"
#include "wire-analysis-dialog-methods.h"
#include "wire-canvas-item-methods.h"

//-- misc
#ifndef GST_CAT_DEFAULT
  #define GST_CAT_DEFAULT bt_edit_debug
#endif
#if defined(BT_EDIT) && !defined(BT_EDIT_APPLICATION_C)
  GST_DEBUG_CATEGORY_EXTERN(GST_CAT_DEFAULT);
#endif

/**
 * GNOME_CANVAS_BROKEN_PROPERTIES:
 *
 * gnome canvas has a broken design,
 * it does not allow derived classes to have G_PARAM_CONSTRUCT_ONLY properties
 */
#define GNOME_CANVAS_BROKEN_PROPERTIES 1

#endif // BT_EDIT_H
