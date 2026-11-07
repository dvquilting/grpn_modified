/*

Copyright (C) 2002  Paul Wilkins

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
/* setup_buttons.c  by Paul Wilkins */

#include <stdio.h>
#include <gtk/gtk.h>

#include "buttons.h"
#include "funcs.h"
#include "editor.h"
#include "lcd.h"
#include "variables.h"

#define MEMORY_BUTTON_COUNT 10
static GtkWidget *memoryButtons[MEMORY_BUTTON_COUNT];
static int memoryButtonCount;

static void memoryButtonCB(GtkButton *button, gpointer data)
{
   (void)button;
   if(RecallVariableNamed(data)) redrawLCD();
}

static void freeSignalData(gpointer data, GClosure *closure)
{
   (void)closure;
   g_free(data);
}

static void variableAdded(const char *name, void *data)
{
   GtkWidget *button;
   (void)data;
   if(memoryButtonCount>=MEMORY_BUTTON_COUNT) return;
   button=memoryButtons[memoryButtonCount++];
   gtk_button_set_label(GTK_BUTTON(button),name);
   gtk_widget_set_sensitive(button,TRUE);
   g_signal_connect_data(button,"clicked",G_CALLBACK(memoryButtonCB),
                         g_strdup(name),freeSignalData,0);
}


/* create the button widgets */
GtkWidget *createButton(GtkWidget *row, FuncInfo *fi)
{
   GtkWidget *btn;
   void (*callBack)(GtkWidget *, gpointer);

   btn = gtk_button_new_with_label(fi->name);

   /* when the button is clicked, we call the "callback" function */
   callBack = fi->CB;
   g_signal_connect(btn, "clicked", G_CALLBACK(callBack), fi);

   gtk_widget_set_hexpand(btn, TRUE);
   gtk_widget_set_can_focus(btn, FALSE);
   gtk_box_append(GTK_BOX(row), btn);

   return btn;
}

static void applyKeypadCss(GtkWidget *keypad)
{
   static GtkCssProvider *css = NULL;
   gtk_widget_add_css_class(keypad, "grpn-keypad");
   if(css == NULL){
      css = gtk_css_provider_new();
      gtk_css_provider_load_from_string(css,
         ".grpn-keypad button { padding-left: 2px; padding-right: 2px; min-width: 0; }");
      gtk_style_context_add_provider_for_display(gtk_widget_get_display(keypad),
         GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
   }
}

/* Each keypad row is its own equal-width box, so rows may hold
   different numbers of buttons; all rows share the same height. */
static GtkWidget *newKeypadRow(GtkWidget *keypad)
{
   GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
   gtk_box_set_homogeneous(GTK_BOX(row), TRUE);
   gtk_box_append(GTK_BOX(keypad), row);
   return row;
}

/* setup the buttons */
GtkWidget *setupButtons(GtkWidget *parent){
   int i, j;
   GtkWidget *keypad, *row;

   keypad = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_box_set_homogeneous(GTK_BOX(keypad), TRUE);
   applyKeypadCss(keypad);
   gtk_box_append(GTK_BOX(parent), keypad);

   row = newKeypadRow(keypad);
   for(i=0;i<MEMORY_BUTTON_COUNT;i++){
      memoryButtons[i]=gtk_button_new_with_label("");
      gtk_widget_set_sensitive(memoryButtons[i],FALSE);
      gtk_widget_set_hexpand(memoryButtons[i],TRUE);
      gtk_box_append(GTK_BOX(row),memoryButtons[i]);
   }
   setVariableAddedCallback(variableAdded,NULL);

   for(i=0; i<NumButtonRows; i++){
      row = newKeypadRow(keypad);
      for(j=0; j<rowinf[i].numBtns; j++){
         if(rowinf[i].fi[j].name != NULL)
            createButton(row, &(rowinf[i].fi[j]));
      }
   }

   return keypad;
}
