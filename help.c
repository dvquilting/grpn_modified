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
#include <stdio.h>
#include <stdlib.h>
#include <gtk/gtk.h>

#include "help.h"
#include "funcs.h"
#include "buttons.h"
#include "license.h"

#define HELP_TXT \
"\n\
GRPN  v1.1.2\n\
\n\
By: Paul Wilkins\n\
    paul.wilkins@analog.com\n\
\n\
GRPN is a graphical reverse polish notation (RPN) calculator.\n\
\n\
GRPN works with real numbers, complex numbers, matrices, and\n\
complex matrices.  Numbers can be displayed in 4 different\n\
radix modes, and complex numbers can be displayed in either\n\
Cartesian or polar form.\n\
\n\
GRPN uses a stack and reverse polish notation to evaluate\n\
expressions.  The stack in GRPN is infinite and limited only\n\
by available memory.\n\
\n\
Expressions are evaluated in reverse polish notation.  Unlike\n\
conventional (prefix) notation, RPN requires that all arguments\n\
to a command are entered prior to execution of the command.  For\n\
example to add 6.7 to 3.2 you would push both numbers onto the\n\
stack, then press the add (+) button:\n\
\n\
3.2<enter>\n\
6.7<enter>\n\
+\n\
\n\
The result is left on the stack.\n\
\n\
Note that GRPN uses a shorthand for all commands that causes\n\
an automatic <enter> before executing a command.  The previous\n\
example could then be done as:\n\
\n\
3.2<enter>\n\
6.7+\n\
\n\
Input is accepted from both the keyboard and the mouse.  When\n\
entering a command from the keyboard, the command may be\n\
abbreviated to the shortest set of letters which uniquely\n\
identifies the command.\n\
\n\
Input may be forced to be interpreted as a command by prefixing\n\
the command with double or single quotes.  This is useful, for\n\
example when in Hexadecimal mode and you would like to switch to\n\
decimal mode by typing:\n\
dec\n\
\n\
Mouse input simply requires pressing the button with the desired\n\
command.\n\
\n\
Available commands:\n\
\n\
+           Add.\n\
-           Subtract.\n\
*           Multiply.\n\
/           Divide.\n\
^           Power.\n\
<enter>     Push a number onto the top of the stack.\n\
<space>     Push a number onto the top of the stack.\n\
"

static void popup_destroyed(GtkWidget *widget, gpointer data){
   (void)widget;
   *(GtkWidget **)data = NULL;
}

void popup_window(GtkWidget **dialog, char *txt, char *title){
   GtkWidget *vbox, *scrolled, *view, *button;
   if(*dialog){ gtk_window_present(GTK_WINDOW(*dialog)); return; }
   *dialog=gtk_window_new(); gtk_window_set_title(GTK_WINDOW(*dialog),title);
   gtk_window_set_default_size(GTK_WINDOW(*dialog),470,470);
   g_signal_connect(*dialog,"destroy",G_CALLBACK(popup_destroyed),dialog);
   vbox=gtk_box_new(GTK_ORIENTATION_VERTICAL,5); gtk_widget_set_margin_start(vbox,5);
   gtk_widget_set_margin_end(vbox,5); gtk_widget_set_margin_top(vbox,5); gtk_widget_set_margin_bottom(vbox,5);
   gtk_window_set_child(GTK_WINDOW(*dialog),vbox);
   scrolled=gtk_scrolled_window_new(); gtk_widget_set_vexpand(scrolled,TRUE); gtk_box_append(GTK_BOX(vbox),scrolled);
   view=gtk_text_view_new(); gtk_text_view_set_editable(GTK_TEXT_VIEW(view),FALSE); gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(view),FALSE);
   gtk_text_buffer_set_text(gtk_text_view_get_buffer(GTK_TEXT_VIEW(view)),txt,-1); gtk_scrolled_window_set_child(GTK_SCROLLED_WINDOW(scrolled),view);
   button=gtk_button_new_with_label("Dismiss"); g_signal_connect_swapped(button,"clicked",G_CALLBACK(gtk_window_destroy),*dialog);
   gtk_box_append(GTK_BOX(vbox),button); gtk_window_present(GTK_WINDOW(*dialog));
}
 

void license_popup(){
   static GtkWidget *dialog = NULL;
 
   popup_window(&dialog, LICENSE_TXT, "License");
}
 
 

void help_popup(){
 
   int i, j;
   char *htxt;
   char *cmd;
   GString *text;
   static GtkWidget *dialog = NULL;

   text=g_string_new(HELP_TXT);
   for(i=0; i<NumFunctionRows; i++){
      for(j=0; j<rowinf[i].numBtns; j++){
         cmd=rowinf[i].fi[j].cmd;
         if(cmd){
            g_string_append_printf(text,"%-12s%s\n",cmd,rowinf[i].fi[j].help);
         }
      }
   }
   htxt=strdup(text->str); g_string_free(text,TRUE);

   popup_window(&dialog, htxt, "Help");

   free(htxt);
}
