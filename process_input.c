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
/* process_input.c  by Paul Wilkins 3/21/97 */

#include <stdio.h>
#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "process_input.h"
#include "funcs.h"
#include "lcd.h"
#include "editor.h"
#include "error.h"



void processInput(int ksym, int isCtrl, char bb, char chr){
   int keysym;
   char c;

   if(ksym == 0){
      switch(chr){
         case ' ':
         case '\n':
         case '\t':
	    keysym = GDK_KEY_space;
	    break;
         case '+':
	    keysym = GDK_KEY_plus;
	    break;
         case '-':
	    keysym = GDK_KEY_minus;
	    break;
         case '/':
	    keysym = GDK_KEY_slash;
	    break;
         case '*':
	    keysym = GDK_KEY_asterisk;
	    break;
         case '^':
	    keysym = GDK_KEY_asciicircum;
	    break;
         case '!':
	    keysym = GDK_KEY_exclam;
	    break;
         default:
            keysym = GDK_KEY_4;  /* just pick one */
            break;
      }
   } else {
      keysym = ksym;
   }

   if(isCtrl){
      switch(keysym){
         case GDK_KEY_c:
            /* quit */
            exit(0);
            break;
      }

      /* ignore the ctrl key */
      return;
   }


   /* this will clear any error string */
   resetError();

   /* process the line */
   switch(keysym){

      case GDK_KEY_space:     /* if there is something in the buffer, then */
      case GDK_KEY_KP_Space:  /* push it onto the stack, else duplicate the */
      case GDK_KEY_Return:    /* element on the bottom of the stack */
      case GDK_KEY_Tab:
      case GDK_KEY_KP_Tab:
      case GDK_KEY_KP_Enter:
      case GDK_KEY_KP_Insert:
      case GDK_KEY_Linefeed:
         PushStack();
         break;

      case GDK_KEY_Begin:      /* used when editing the entry buffer */
      case GDK_KEY_Home:
      case GDK_KEY_Up:
      case GDK_KEY_KP_Home:
      case GDK_KEY_KP_Up:
         homeEditor();
         break;

      case GDK_KEY_End:      /* used when editing the entry buffer */
      case GDK_KEY_Down:
      case GDK_KEY_KP_End:
      case GDK_KEY_KP_Down:
         endEditor();
         break;

      case GDK_KEY_Right:      /* used when editing the entry buffer */
      case GDK_KEY_KP_Right:
         rightEditor();
         break;

      case GDK_KEY_Left:      /* used when editing the entry buffer */
      case GDK_KEY_KP_Left:
         leftEditor();
         break;

      case GDK_KEY_BackSpace:  /* delete the last entry in the stack */
      case GDK_KEY_Delete:     /* OR backspace over the last char typed */
      case GDK_KEY_KP_Delete:
         if(isEditingEditor()){
            deleteEditor();
         } else {
            PopStack();
         }
         break;

      case GDK_KEY_Escape:  /* clear the entry buffer */
         cancelEditor();
         break;

      case GDK_KEY_plus: 
      case GDK_KEY_KP_Add:
         c = getPrevEditor();
         if(c == 'e' || c == 'E'){
            insertEditor(bb);
         } else {
	    AddStack();
         }
         break;

      case GDK_KEY_minus: 
      case GDK_KEY_KP_Subtract:
         c = getPrevEditor();
         if(c == 'e' || c == 'E'){
            insertEditor(bb);
         } else {
	    SubStack();
         }
         break;

      case GDK_KEY_asterisk: 
      case GDK_KEY_KP_Multiply:
         MulStack();
         break;

      case GDK_KEY_slash: 
      case GDK_KEY_KP_Divide:
         DivStack();
         break;

      case GDK_KEY_asciicircum:
         PowStack();
         break;

      case GDK_KEY_exclam:
         FactStack();
         break;

      default:  /* catch everything that's left over */

         /* ascii characters */
         if((keysym >= GDK_KEY_KP_Space && keysym <= GDK_KEY_KP_9) ||
            (keysym >= GDK_KEY_space && keysym <= GDK_KEY_asciitilde)){

            insertEditor(bb);
         }

         
         break;
   }

   /* repaint the screen */
   redrawLCD();

}
