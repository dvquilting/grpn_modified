/* setup_menu.cpp: the File and Help menus.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#include <QMainWindow>
#include <QMenu>
#include <QMenuBar>

#include "gui.h"
#include "lcd.h"

void setupMenu(QMainWindow *window)
{
   QMenuBar *bar=window->menuBar();
   QMenu *file=bar->addMenu("&File"), *help=bar->addMenu("&Help");

   file->addAction("Clear",[]{ clearLCD(); });
   file->addAction("Options",[]{ optionsPopup(); });
   file->addAction("Quit",[]{ quitApplication(); });
   help->addAction("Help",[]{ helpPopup(); });
   help->addAction("License",[]{ licensePopup(); });
}
