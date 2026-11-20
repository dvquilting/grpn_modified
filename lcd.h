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
/* lcd.h  by Paul Wilkins 3/21/97 */

#ifndef __LCD_H
#define __LCD_H

#ifdef __cplusplus
extern "C" {
#endif

/* display area in characters */
extern int lcdWidth;
extern int lcdHeight;
/* lcdHeight rows of lcdWidth characters each (not NUL terminated) */
extern char **lcdText;

/* set the display size in characters, reallocating the text if needed */
void resizeLCD(int cols, int rows);
/* recompute lcdText from the stack if it is out of date */
void updateLCD(void);
void redrawLCD(void);
void clearLCD(void);

void setThousandsSeparators(int);
int getThousandsSeparators(void);
#define MIN_DISPLAY_DIGITS 1
#define MAX_DISPLAY_DIGITS 70
#define DEFAULT_DISPLAY_DIGITS 12
void setDisplayDigits(int);
int getDisplayDigits(void);
void setEngNotation(int);
int getEngNotation(void);
void setLCDFontSize(int);
int getLCDFontSize(void);

#ifdef __cplusplus
}
#endif


#endif
