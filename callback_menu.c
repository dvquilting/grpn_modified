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
/* callback_menu.h  by Paul Wilkins 3/15/97 */

#include <stdio.h>
#include <gtk/gtk.h>

#include "menu.h"
#include "lcd.h"
#include "mode.h"
#include "funcs.h"
#include "help.h"
#include "complex.h"
#include "number.h"
#include "options.h"

/* quit the program */
void quitCB(gpointer data){
   (void)data;
   GApplication *app = g_application_get_default();
   if(app) g_application_quit(app);
}

/* clear the stack */
void clearCB(gpointer data){
   (void)data;

   clearLCD();

   /* refresh the display */
   redrawLCD();

}


void licenseCB(gpointer data){
   (void)data;
   license_popup();
}

void helpCB(gpointer data){
   (void)data;
   help_popup();
}


/* change the base we use to display the numbers */
void baseCB(gpointer clientData){

   setBaseMode(GPOINTER_TO_INT(clientData));

   /* refresh the display */
   redrawLCD();
   refreshModeDisplay();
}

void baseCmdCB(GtkWidget *w, gpointer clientData){
   (void)w;
   FuncInfo *fi = (FuncInfo *)clientData;

   /* store the mode */
   baseCB(fi->data);

   /* TODO: set the toggles in the buttons */
   if(optWindow){
      gtk_check_button_set_active(GTK_CHECK_BUTTON(optHexBtn), 
	 getBaseMode() == HEXIDECIMAL ? TRUE : FALSE);
      gtk_check_button_set_active(GTK_CHECK_BUTTON(optDecBtn), 
	 getBaseMode() == DECIMAL ? TRUE : FALSE);
      gtk_check_button_set_active(GTK_CHECK_BUTTON(optOctBtn), 
	 getBaseMode() == OCTAL ? TRUE : FALSE);
      gtk_check_button_set_active(GTK_CHECK_BUTTON(optBinBtn), 
	 getBaseMode() == BINARY ? TRUE : FALSE);
   }
}

void baseToggleCB(GtkWidget *widget, gpointer data){
   if(gtk_check_button_get_active(GTK_CHECK_BUTTON(widget))){
      baseCB(data);
   }
}

/* change the radix mode we use to display the numbers */
void radixCB(gpointer clientData){
   
   setRadixMode(GPOINTER_TO_INT(clientData));

   /* refresh the display */
   redrawLCD();
   refreshModeDisplay();
}

void radixToggleCB(GtkWidget *widget, gpointer data){
   if(gtk_check_button_get_active(GTK_CHECK_BUTTON(widget))){
      radixCB(data);
   }
}

/* change the coordinate mode we use to display the numbers */
void cmodeCB(gpointer clientData){
   
   switch(GPOINTER_TO_INT(clientData)){
      case RECTANGULAR:
         setPolarMode(RECTANGULAR);
         break;
      case POLAR:
         setPolarMode(POLAR);
         break;
      default:
         fprintf(stderr, "Error: radixCB: i shouldn't get here\n");
         exit(0);
   }

   /* refresh the display */
   redrawLCD();
   refreshModeDisplay();
}

void cmodeToggleCB(GtkWidget *widget, gpointer data){
   if(gtk_check_button_get_active(GTK_CHECK_BUTTON(widget))){
      cmodeCB(data);
   }
}

void engNotationToggleCB(GtkWidget *widget, gpointer data){
   (void)data;
   setEngNotation(gtk_check_button_get_active(GTK_CHECK_BUTTON(widget)));
   redrawLCD();
}

void separatorsToggleCB(GtkWidget *widget, gpointer data){
   (void)data;
   setThousandsSeparators(gtk_check_button_get_active(GTK_CHECK_BUTTON(widget)));
   redrawLCD();
}
