/* $Id$
 *
 * Buzztard
 * Copyright (C) 2010 Buzztard team <buzztard-devel@lists.sf.net>
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

#include "m-bt-edit.h"

//-- globals

//-- fixtures

static void test_setup(void) {
  bt_edit_setup();
}

static void test_teardown(void) {
  bt_edit_teardown();
}

//-- helper

//-- tests

// show pattern page with empty pattern and emit key-presses
BT_START_TEST(test_editing1) {
  BtEditApplication *app;
  BtMainWindow *main_window;
  BtMainPages *pages;
  BtMainPagePatterns *pattern_page;
  BtSong *song;
  BtSetup *setup;
  BtMachine *src_machine;
  GdkEventKey *e;
  GError *err=NULL;

  app=bt_edit_application_new();
  GST_INFO("back in test app=%p, app->ref_ct=%d",app,G_OBJECT(app)->ref_count);
  fail_unless(app != NULL, NULL);
  
  // create a new song
  bt_edit_application_new_song(app);
  
  // get window && song
  g_object_get(app,"song",&song,"main-window",&main_window,NULL);
  fail_unless(main_window != NULL, NULL);
  fail_unless(song != NULL, NULL);
  g_object_get(song,"setup",&setup,NULL);
  
  // create a source machine
  src_machine=BT_MACHINE(bt_source_machine_new(song,"gen","fakesrc",0,&err));
  fail_unless(src_machine!=NULL, NULL);
  fail_unless(err==NULL, NULL);

  // make sure the pattern view shows something
  g_object_get(G_OBJECT(main_window),"pages",&pages,NULL);
  g_object_get(G_OBJECT(pages),"patterns-page",&pattern_page,NULL);
  bt_main_page_patterns_show_machine(pattern_page,src_machine);

  // show page
  gtk_notebook_set_current_page(GTK_NOTEBOOK(pages),BT_MAIN_PAGES_PATTERNS_PAGE);
  while(gtk_events_pending()) gtk_main_iteration();
  
  GST_INFO("sending events");

  // send a '.' key-press
  e=(GdkEventKey *)gdk_event_new(GDK_KEY_PRESS);
  e->window=((GtkWidget *)pattern_page)->window;
  e->keyval='.';
  gtk_main_do_event((GdkEvent *)e);
  gdk_event_free((GdkEvent *)e);
  while(gtk_events_pending()) gtk_main_iteration();

  // send a '0' key-press
  e=(GdkEventKey *)gdk_event_new(GDK_KEY_PRESS);
  e->window=((GtkWidget *)pattern_page)->window;
  e->keyval='0';
  gtk_main_do_event((GdkEvent *)e);
  gdk_event_free((GdkEvent *)e);
  while(gtk_events_pending()) gtk_main_iteration();
  
  GST_INFO("test done");

  g_object_unref(pattern_page);
  g_object_unref(src_machine);
  g_object_unref(setup);
  g_object_unref(pages);

  // close window
  gtk_widget_destroy(GTK_WIDGET(main_window));
  while(gtk_events_pending()) gtk_main_iteration();
  //GST_INFO("mainlevel is %d",gtk_main_level());
  //while(g_main_context_pending(NULL)) g_main_context_iteration(/*context=*/NULL,/*may_block=*/FALSE);

  // free application
  g_object_unref(song);
  GST_INFO("app->ref_ct=%d",G_OBJECT(app)->ref_count);
  g_object_checked_unref(app);

}
BT_END_TEST

// show pattern page with empty pattern and emit mouse klicks
BT_START_TEST(test_editing2) {
  BtEditApplication *app;
  BtMainWindow *main_window;
  BtMainPages *pages;
  BtMainPagePatterns *pattern_page;
  GtkWidget *pattern_editor;
  BtSong *song;
  BtSetup *setup;
  BtMachine *src_machine;
  GdkEventButton *e;
  GError *err=NULL;
  GList *list;

  app=bt_edit_application_new();
  GST_INFO("back in test app=%p, app->ref_ct=%d",app,G_OBJECT(app)->ref_count);
  fail_unless(app != NULL, NULL);
  
  // create a new song
  bt_edit_application_new_song(app);
  
  // get window && song
  g_object_get(app,"song",&song,"main-window",&main_window,NULL);
  fail_unless(main_window != NULL, NULL);
  fail_unless(song != NULL, NULL);
  g_object_get(song,"setup",&setup,NULL);
  
  // create a source machine
  src_machine=BT_MACHINE(bt_source_machine_new(song,"gen","fakesrc",0,&err));
  fail_unless(src_machine!=NULL, NULL);
  fail_unless(err==NULL, NULL);

  // make sure the pattern view shows something
  g_object_get(G_OBJECT(main_window),"pages",&pages,NULL);
  g_object_get(G_OBJECT(pages),"patterns-page",&pattern_page,NULL);
  bt_main_page_patterns_show_machine(pattern_page,src_machine);
  
  // show page
  gtk_notebook_set_current_page(GTK_NOTEBOOK(pages),BT_MAIN_PAGES_PATTERNS_PAGE);
  while(gtk_events_pending()) gtk_main_iteration();

  GST_INFO("object types: %s",G_OBJECT_TYPE_NAME(pattern_page));

  list=gtk_container_get_children(GTK_CONTAINER(pattern_page));
  // 1st is toolbat, 2nd is scrollable window
  pattern_editor=gtk_bin_get_child(GTK_BIN(g_list_nth_data(list,1)));
  g_list_free(list);
  
  GST_INFO("object types: %s",G_OBJECT_TYPE_NAME(pattern_editor));

  GST_INFO("sending events");
  
  // send a left mouse button press (hopefully on the tick number column)
  e=(GdkEventButton *)gdk_event_new(GDK_BUTTON_PRESS);
  e->window=((GtkWidget *)pattern_editor)->window;
  e->button=1; // left-button
  e->x=10.0;
  e->y=100.0;
  e->state=GDK_BUTTON1_MASK;
  gtk_main_do_event((GdkEvent *)e);
  gdk_event_free((GdkEvent *)e);
  while(gtk_events_pending()) gtk_main_iteration();

  GST_INFO("test done");

  g_object_unref(pattern_page);
  g_object_unref(src_machine);
  g_object_unref(setup);
  g_object_unref(pages);

  // close window
  gtk_widget_destroy(GTK_WIDGET(main_window));
  while(gtk_events_pending()) gtk_main_iteration();
  //GST_INFO("mainlevel is %d",gtk_main_level());
  //while(g_main_context_pending(NULL)) g_main_context_iteration(/*context=*/NULL,/*may_block=*/FALSE);

  // free application
  g_object_unref(song);
  GST_INFO("app->ref_ct=%d",G_OBJECT(app)->ref_count);
  g_object_checked_unref(app);

}
BT_END_TEST

TCase *bt_pattern_page_test_case(void) {
  TCase *tc = tcase_create("BtPatternPageTests");

  tcase_add_test(tc,test_editing1);
  tcase_add_test(tc,test_editing2);
  // we *must* use a checked fixture, as only this runs in the same context
  tcase_add_checked_fixture(tc, test_setup, test_teardown);
  return(tc);
}