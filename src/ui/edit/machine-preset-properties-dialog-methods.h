/* $Id: machine-preset-properties-dialog-methods.h,v 1.2 2007-01-15 21:43:50 ensonic Exp $
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

#ifndef BT_MACHINE_PRESET_PROPERTIES_DIALOG_METHODS_H
#define BT_MACHINE_PRESET_PROPERTIES_DIALOG_METHODS_H

#include "machine-preset-properties-dialog.h"
#include "edit-application.h"

extern BtMachinePresetPropertiesDialog *bt_machine_preset_properties_dialog_new(const BtEditApplication *app,GstElement *machine,gchar **name,gchar **comment);
extern void bt_machine_preset_properties_dialog_apply(const BtMachinePresetPropertiesDialog *self);

#endif // BT_MACHINE_PRESET_PROPERTIES_DIALOG_METHDOS_H
