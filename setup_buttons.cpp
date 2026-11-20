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
/* setup_buttons.cpp  by Paul Wilkins */

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QPushButton>
#include <QSize>
#include <QString>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include "gui.h"
#include "buttons.h"
#include "funcs.h"
#include "lcd.h"
#include "variables.h"

#define MEMORY_BUTTON_COUNT 10

/* A keypad button only as wide as its label plus a small margin, so
   rows of many buttons stay narrow.  It never takes the keyboard
   focus away from the stack display. */
class KeyButton : public QPushButton {
public:
   explicit KeyButton(const QString &text=QString()) : QPushButton(text) {
      setFocusPolicy(Qt::NoFocus);
      setSizePolicy(QSizePolicy::Ignored,QSizePolicy::Fixed);
   }
   int naturalWidth(void) const {
      QFontMetrics fm=fontMetrics(); int w=0;
      for(const QString &line : text().split('\n')) w=qMax(w,fm.horizontalAdvance(line));
      return w+10;
   }
   QSize sizeHint(void) const override {
      return QSize(naturalWidth(),QPushButton::sizeHint().height());
   }
   QSize minimumSizeHint(void) const override { return sizeHint(); }
};

typedef QList<KeyButton *> KeyRow;

static KeyRow memoryButtons;
static int memoryButtonCount;

/* every button in a row gets the same width, enough for its widest label */
static void equalizeRow(const KeyRow &row){
   int width=0;
   for(KeyButton *b : row) width=qMax(width,b->naturalWidth());
   for(KeyButton *b : row) b->setMinimumWidth(width);
}

static void variableAdded(const char *name, void *data){
   KeyButton *button;
   QString label=QString::fromUtf8(name);
   (void)data;
   if(memoryButtonCount>=MEMORY_BUTTON_COUNT || memoryButtons.isEmpty()) return;
   button=memoryButtons[memoryButtonCount++];
   button->setText(label);
   button->setEnabled(true);
   QObject::connect(button,&QPushButton::clicked,[label]{
      if(RecallVariableNamed(label.toUtf8().constData())) redrawLCD();
   });
   equalizeRow(memoryButtons);
}

/* create the button widgets */
static KeyButton *createButton(FuncInfo *fi){
   KeyButton *btn=new KeyButton(QString::fromLatin1(fi->name));

   /* when the button is clicked, we call the "callback" function */
   QObject::connect(btn,&QPushButton::clicked,[fi]{ fi->CB(NULL,fi); });
   return btn;
}

/* setup the buttons */
QWidget *setupButtons(const char *font){
   int i, j, height=0;
   QList<KeyRow> rows;
   QWidget *keypad=new QWidget;
   QVBoxLayout *layout=new QVBoxLayout(keypad);

   if(font) keypad->setFont(parseFontSpec(font,0));
   layout->setContentsMargins(0,0,0,0);
   layout->setSpacing(0);

   /* the first row holds buttons for variables saved with Sto */
   memoryButtons.clear(); memoryButtonCount=0;
   for(i=0;i<MEMORY_BUTTON_COUNT;i++){
      KeyButton *b=new KeyButton;
      b->setEnabled(false);
      memoryButtons.append(b);
   }
   rows.append(memoryButtons);

   for(i=0; i<NumButtonRows; i++){
      KeyRow row;
      for(j=0; j<rowinf[i].numBtns; j++){
         if(rowinf[i].fi[j].name != NULL)
            row.append(createButton(&(rowinf[i].fi[j])));
      }
      rows.append(row);
   }

   /* Each keypad row is its own equal-width box, so rows may hold
      different numbers of buttons; all rows share the same height. */
   for(const KeyRow &row : rows){
      for(KeyButton *b : row){
         b->setParent(keypad);     /* use the keypad's font when measuring */
         height=qMax(height,b->sizeHint().height());
      }
   }
   for(const KeyRow &row : rows){
      QHBoxLayout *box=new QHBoxLayout;
      box->setSpacing(0);
      for(KeyButton *b : row){
         b->setFixedHeight(height);
         box->addWidget(b,1);
      }
      layout->addLayout(box);
      equalizeRow(row);
   }

   setVariableAddedCallback(variableAdded,NULL);
   return keypad;
}
