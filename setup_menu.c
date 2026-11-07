#include <gtk/gtk.h>
#include "menu.h"

extern void quitCB(gpointer); extern void clearCB(gpointer);
extern void optionsCB(gpointer); extern void licenseCB(gpointer); extern void helpCB(gpointer);

static GtkWidget *menu_button(const char *label, const char *const items[], GCallback callbacks[], guint count)
{
   GtkWidget *button = gtk_menu_button_new(), *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   GtkWidget *popover = gtk_popover_new(); guint i;
   gtk_menu_button_set_label(GTK_MENU_BUTTON(button), label);
   gtk_popover_set_child(GTK_POPOVER(popover), box);
   gtk_menu_button_set_popover(GTK_MENU_BUTTON(button), popover);
   for(i=0; i<count; i++){
      GtkWidget *item = gtk_button_new_with_label(items[i]);
      gtk_widget_add_css_class(item, "flat");
      g_signal_connect_swapped(item, "clicked", callbacks[i], NULL);
      gtk_box_append(GTK_BOX(box), item);
   }
   return button;
}

GtkWidget *setup_menu(GtkWidget *parent)
{
   static const char *const file_items[] = {"Clear", "Options", "Quit"};
   static GCallback file_callbacks[] = {G_CALLBACK(clearCB), G_CALLBACK(optionsCB), G_CALLBACK(quitCB)};
   static const char *const help_items[] = {"Help", "License"};
   static GCallback help_callbacks[] = {G_CALLBACK(helpCB), G_CALLBACK(licenseCB)};
   GtkWidget *bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
   GtkWidget *spacer = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
   gtk_widget_add_css_class(bar, "menubar");
   gtk_box_append(GTK_BOX(bar), menu_button("File", file_items, file_callbacks, 3));
   gtk_widget_set_hexpand(spacer, TRUE); gtk_box_append(GTK_BOX(bar), spacer);
   gtk_box_append(GTK_BOX(bar), menu_button("Help", help_items, help_callbacks, 2));
   gtk_box_append(GTK_BOX(parent), bar); return bar;
}
