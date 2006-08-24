/* $Id: t-settings.c,v 1.10 2006-08-24 20:00:55 ensonic Exp $
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

#include "m-bt-core.h"

//-- globals

// this counts the number of runs, to provide different implementations for each
static int variant=0;

BtSettings *get_settings(void) {
  BtSettings *settings=NULL;
  
  switch(variant) {
    case 0:
      settings=BT_SETTINGS(bt_gconf_settings_new());
      break;
    case 1:
      settings=BT_SETTINGS(bt_plainfile_settings_new());
      break;
  }
  return(settings);
}

//-- fixtures

static void test_setup(void) {
  bt_core_setup();
  GST_INFO("================================================================================");
}

static void test_teardown(void) {
	bt_core_teardown();
  //puts(__FILE__":teardown");
  variant++;
}

//-- tests
BT_START_TEST(test_btsettings_get_audiosink1) {
  BtSettings *settings=get_settings();
  gchar *saved_audiosink_name,*test_audiosink_name;
  
  g_object_get(settings,"audiosink",&saved_audiosink_name,NULL);
  
  g_object_set(settings,"audiosink","fakesink",NULL);
  
  g_object_get(settings,"audiosink",&test_audiosink_name,NULL);
  
  fail_unless(!strcmp(test_audiosink_name,"fakesink"),"sink is %s",test_audiosink_name);
  
  g_object_set(settings,"audiosink",saved_audiosink_name,NULL);
  
  /* clean up */
  g_object_unref(settings);
  g_free(saved_audiosink_name);
  g_free(test_audiosink_name);
}
BT_END_TEST;


TCase *bt_gconf_settings_test_case(void) {
  TCase *tc = tcase_create("BtSettingsTests");

  tcase_add_test(tc,test_btsettings_get_audiosink1);
  tcase_add_unchecked_fixture(tc, test_setup, test_teardown);
  return(tc);
}
