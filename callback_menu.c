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
#include <stdlib.h>
#include <stdint.h>

#include "lcd.h"
#include "mode.h"
#include "funcs.h"
#include "buttons.h"
#include "number.h"
#include "constant.h"
#include "stack.h"
#include "undo.h"
#include "gui.h"

/* set up the calculator core */
int initCalculator(void){
   setup_constant();
   initUndoSystem();
   return setup_stack();
}

/* change the base we use to display the numbers */
void baseCB(int base){

   setBaseMode(base);

   /* refresh the display */
   redrawLCD();
   refreshModeDisplay();
}

/* the hex, dec, oct and bin commands */
void baseCmdCB(void *w, void *clientData){
   FuncInfo *fi = (FuncInfo *)clientData;
   (void)w;

   /* store the mode; refreshModeDisplay() also updates the
      Options window if it is open */
   baseCB((int)(intptr_t)fi->data);
}

/* change the radix mode we use to display the numbers */
void radixCB(int radix){
   
   setRadixMode(radix);

   /* refresh the display */
   redrawLCD();
   refreshModeDisplay();
}

/* change the coordinate mode we use to display the numbers */
void cmodeCB(int cmode){
   
   switch(cmode){
      case RECTANGULAR:
         setPolarMode(RECTANGULAR);
         break;
      case POLAR:
         setPolarMode(POLAR);
         break;
      default:
         fprintf(stderr, "Error: cmodeCB: i shouldn't get here\n");
         exit(0);
   }

   /* refresh the display */
   redrawLCD();
   refreshModeDisplay();
}
