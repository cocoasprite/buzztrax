/* $Id: processor-machine.h,v 1.16 2006-08-24 20:00:53 ensonic Exp $
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

#ifndef BT_PROCESSOR_MACHINE_H
#define BT_PROCESSOR_MACHINE_H

#include <glib.h>
#include <glib-object.h>

#define BT_TYPE_PROCESSOR_MACHINE             (bt_processor_machine_get_type ())
#define BT_PROCESSOR_MACHINE(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), BT_TYPE_PROCESSOR_MACHINE, BtProcessorMachine))
#define BT_PROCESSOR_MACHINE_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), BT_TYPE_PROCESSOR_MACHINE, BtProcessorMachineClass))
#define BT_IS_PROCESSOR_MACHINE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), BT_TYPE_PROCESSOR_MACHINE))
#define BT_IS_PROCESSOR_MACHINE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), BT_TYPE_PROCESSOR_MACHINE))
#define BT_PROCESSOR_MACHINE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), BT_TYPE_PROCESSOR_MACHINE, BtProcessorMachineClass))

/* type macros */

typedef struct _BtProcessorMachine BtProcessorMachine;
typedef struct _BtProcessorMachineClass BtProcessorMachineClass;
typedef struct _BtProcessorMachinePrivate BtProcessorMachinePrivate;

/**
 * BtProcessorMachine:
 *
 * Sub-class of a #BtMachine that implements an effect-processor
 * (a machine with in and outputs).
 */
struct _BtProcessorMachine {
  BtMachine parent;
  
  /*< private >*/
  BtProcessorMachinePrivate *priv;
};
/* structure of the processor_machine class */
struct _BtProcessorMachineClass {
  BtMachineClass parent;
};

/* used by PROCESSOR_MACHINE_TYPE */
GType bt_processor_machine_get_type(void) G_GNUC_CONST;

/**
 * BtProcessorMachinePatternIndex:
 * @BT_PROCESSOR_MACHINE_PATTERN_INDEX_BREAK: stop the pattern
 * @BT_PROCESSOR_MACHINE_PATTERN_INDEX_MUTE: mute the machine
 * @BT_PROCESSOR_MACHINE_PATTERN_INDEX_BYPASS: bypass the machine
 * @BT_PROCESSOR_MACHINE_PATTERN_INDEX_OFFSET: offset for real pattern ids
 *
 * Use this with bt_machine_get_pattern_by_index() to get the command patterns.
 */
typedef enum {
  BT_PROCESSOR_MACHINE_PATTERN_INDEX_BREAK=0,
  BT_PROCESSOR_MACHINE_PATTERN_INDEX_MUTE,
  BT_PROCESSOR_MACHINE_PATTERN_INDEX_BYPASS,
  BT_PROCESSOR_MACHINE_PATTERN_INDEX_OFFSET
} BtProcessorMachinePatternIndex;

#endif // BT_PROCESSOR_MACHINE_H
