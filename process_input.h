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
/* process_input.h  by Paul Wilkins  3/21/97 */

#ifndef __PROCESS_INPUT_H
#define __PROCESS_INPUT_H


#ifdef __cplusplus
extern "C" {
#endif

/* Key codes for processInput().  Printable ASCII keys use their
   character code; the others use these values. */
#define GRPN_KEY_BackSpace 0xff08
#define GRPN_KEY_Tab       0xff09
#define GRPN_KEY_Return    0xff0d
#define GRPN_KEY_Escape    0xff1b
#define GRPN_KEY_Home      0xff50
#define GRPN_KEY_Left      0xff51
#define GRPN_KEY_Up        0xff52
#define GRPN_KEY_Right     0xff53
#define GRPN_KEY_Down      0xff54
#define GRPN_KEY_End       0xff57
#define GRPN_KEY_Delete    0xffff

/* process keyboard input: key code (0 to derive it from chr),
   whether Control is held, the character to insert, and the
   character typed */
void processInput(int, int, char, char);

#ifdef __cplusplus
}
#endif


#endif

