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
#include <QDialog>
#include <QFontDatabase>
#include <QMainWindow>
#include <QPlainTextEdit>
#include <QPointer>
#include <QPushButton>
#include <QString>
#include <QVBoxLayout>

#include "gui.h"
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

/* a window showing txt; *dialog is cleared when it is closed */
static void popup_window(QPointer<QDialog> &dialog, const QString &txt, const char *title){
   QVBoxLayout *vbox;
   QPlainTextEdit *view;
   QPushButton *button;

   if(!dialog){
      dialog=new QDialog(mainWindow());
      dialog->setWindowTitle(title);
      dialog->setAttribute(Qt::WA_DeleteOnClose);
      dialog->resize(470,470);
      vbox=new QVBoxLayout(dialog);
      vbox->setContentsMargins(5,5,5,5);
      vbox->setSpacing(5);
      view=new QPlainTextEdit(txt);
      view->setReadOnly(true);
      view->setLineWrapMode(QPlainTextEdit::NoWrap);
      view->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
      vbox->addWidget(view);
      button=new QPushButton("Dismiss");
      QObject::connect(button,&QPushButton::clicked,dialog.data(),&QDialog::close);
      vbox->addWidget(button);
   }
   dialog->show();
   dialog->raise();
   dialog->activateWindow();
}

void licensePopup(void){
   static QPointer<QDialog> dialog;
   popup_window(dialog, LICENSE_TXT, "License");
}

void helpPopup(void){
   static QPointer<QDialog> dialog;
   QString text=HELP_TXT;
   int i, j;

   for(i=0; i<NumFunctionRows; i++){
      for(j=0; j<rowinf[i].numBtns; j++){
         const char *cmd=rowinf[i].fi[j].cmd;
         if(cmd) text+=QString::asprintf("%-12s%s\n",cmd,rowinf[i].fi[j].help);
      }
   }
   popup_window(dialog, text, "Help");
}
