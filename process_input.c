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

#include "process_input.h"
#include "funcs.h"
#include "lcd.h"
#include "editor.h"
#include "error.h"
#include "gui.h"



void processInput(int ksym, int isCtrl, char bb, char chr){
   int keysym;
   char c;

   if(ksym == 0){
      /* typed or pasted text: whitespace pushes, any other
         character is handled as the key that types it */
      switch(chr){
         case ' ':
         case '\n':
         case '\r':
         case '\t':
            keysym = ' ';
            break;
         default:
            keysym = (unsigned char)chr;
            break;
      }
   } else {
      keysym = ksym;
   }

   if(isCtrl){
      switch(keysym){
         case 'c':
            /* quit */
            quitApplication();
            break;
      }

      /* ignore the ctrl key */
      return;
   }


   /* this will clear any error string */
   resetError();

   /* process the line */
   switch(keysym){

      case ' ':              /* if there is something in the buffer, then */
      case GRPN_KEY_Return:  /* push it onto the stack, else duplicate the */
      case GRPN_KEY_Tab:     /* element on the bottom of the stack */
         PushStack();
         break;

      case GRPN_KEY_Home:      /* used when editing the entry buffer */
      case GRPN_KEY_Up:
         homeEditor();
         break;

      case GRPN_KEY_End:      /* used when editing the entry buffer */
      case GRPN_KEY_Down:
         endEditor();
         break;

      case GRPN_KEY_Right:      /* used when editing the entry buffer */
         rightEditor();
         break;

      case GRPN_KEY_Left:      /* used when editing the entry buffer */
         leftEditor();
         break;

      case GRPN_KEY_BackSpace:  /* delete the last entry in the stack */
      case GRPN_KEY_Delete:     /* OR backspace over the last char typed */
         if(isEditingEditor()){
            deleteEditor();
         } else {
            PopStack();
         }
         break;

      case GRPN_KEY_Escape:  /* clear the entry buffer */
         cancelEditor();
         break;

      case '+':
         c = getPrevEditor();
         if(c == 'e' || c == 'E'){
            insertEditor(bb);
         } else {
	    AddStack();
         }
         break;

      case '-':
         c = getPrevEditor();
         if(c == 'e' || c == 'E'){
            insertEditor(bb);
         } else {
	    SubStack();
         }
         break;

      case '*':
         MulStack();
         break;

      case '/':
         DivStack();
         break;

      case '^':
         PowStack();
         break;

      case '!':
         FactStack();
         break;

      default:  /* catch everything that's left over */

         /* ascii characters */
         if(keysym >= ' ' && keysym <= '~'){

            insertEditor(bb);
         }

         
         break;
   }

   /* repaint the screen */
   redrawLCD();

}
