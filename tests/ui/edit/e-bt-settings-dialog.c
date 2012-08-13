/* Buzztard
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

#include "m-bt-edit.h"

//-- globals

//-- fixtures

static void
test_setup (void)
{
  bt_edit_setup ();
}

static void
test_teardown (void)
{
  bt_edit_teardown ();
}

//-- tests

// create app and then unconditionally destroy window
static void
test_create_dialog (BT_TEST_ARGS)
{
  BT_TEST_START;
  BtEditApplication *app;
  BtMainWindow *main_window;
  GtkWidget *dialog;
  GEnumClass *enum_class;
  GEnumValue *enum_value;
  guint i;

  app = bt_edit_application_new ();
  GST_INFO ("back in test app=%p, app->ref_ct=%d", app,
      G_OBJECT_REF_COUNT (app));
  fail_unless (app != NULL, NULL);

  // get window
  g_object_get (app, "main-window", &main_window, NULL);
  fail_unless (main_window != NULL, NULL);

  // create, show and destroy dialog
  dialog = GTK_WIDGET (bt_settings_dialog_new ());
  fail_unless (dialog != NULL, NULL);
  gtk_widget_show_all (dialog);
  // leave out that line! (modal dialog)
  //gtk_dialog_run(GTK_DIALOG(dialog));

  // snapshot all dialog pages
  enum_class = g_type_class_peek_static (BT_TYPE_SETTINGS_PAGE);
  for (i = enum_class->minimum; i <= enum_class->maximum; i++) {
    if ((enum_value = g_enum_get_value (enum_class, i))) {
      g_object_set (G_OBJECT (dialog), "page", i, NULL);

      // make screenshot
      check_make_widget_screenshot (GTK_WIDGET (dialog),
          enum_value->value_nick);
      while (gtk_events_pending ())
        gtk_main_iteration ();
    }
  }

  gtk_widget_destroy (dialog);

  // close window
  gtk_widget_destroy (GTK_WIDGET (main_window));
  while (gtk_events_pending ())
    gtk_main_iteration ();

  // free application
  GST_INFO ("app->ref_ct=%d", G_OBJECT_REF_COUNT (app));
  g_object_checked_unref (app);
  BT_TEST_END;
}

TCase *
bt_settings_dialog_example_case (void)
{
  TCase *tc = tcase_create ("BtSettingsDialogExamples");

  tcase_add_test (tc, test_create_dialog);
  // we *must* use a checked fixture, as only this runs in the same context
  tcase_add_checked_fixture (tc, test_setup, test_teardown);
  return (tc);
}
