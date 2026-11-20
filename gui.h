/* gui.h: the interface between the C calculator core and the Qt 5
   user interface.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#ifndef GRPN_GUI_H
#define GRPN_GUI_H

#ifdef __cplusplus
extern "C" {
#endif

/* Implemented by the C core, called by the GUI. */

/* set up the constants, undo system, and stack; returns 0 on failure */
int initCalculator(void);

/* Implemented by the GUI, called by the C core. */

/* schedule a repaint of the stack display */
void lcdQueueDraw(void);
/* the display font size changed */
void lcdFontChanged(void);
/* leave the event loop; settings are saved on the way out */
void quitApplication(void);

#ifdef __cplusplus
}

/* Qt-only parts of the GUI. */
class QFont;
class QMainWindow;
class QWidget;

/* a QFont from a Pango-style name such as "DejaVu Sans Mono Bold 12";
   defaultSize (points) is used when the name has no size */
QFont parseFontSpec(const char *spec, double defaultSize);

QWidget *setupModeDisplay(void);
QWidget *setupLCD(int rows, int cols, const char *font);
/* the keypad, in the given font (NULL for the default) */
QWidget *setupButtons(const char *font);
void setupMenu(QMainWindow *window);

/* the main window, the parent of the Options, Help and License windows */
QMainWindow *mainWindow(void);

void optionsPopup(void);
/* show the current settings in the Options window, if it is open */
void syncOptionsWindow(void);
void helpPopup(void);
void licensePopup(void);
#endif

#endif
