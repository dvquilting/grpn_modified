/* lcd_qt.cpp: the Qt widget that shows the stack.

The text it shows comes from lcd.c.  Typing goes to processInput();
dragging with the left button selects text and copies it to the
primary selection, and the middle button pastes it as typed input.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#include <QApplication>
#include <QClipboard>
#include <QEvent>
#include <QFont>
#include <QFontMetrics>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QWidget>

#include "gui.h"
#include "lcd.h"
#include "editor.h"
#include "process_input.h"

#define BORDER 4
#define DEFAULT_LCD_FONT "DejaVu Sans Mono"

class LcdWidget : public QWidget {
public:
   LcdWidget(int rows, int cols, const char *fontSpec);
   void fontChanged(void);

protected:
   bool event(QEvent *event) override;
   void paintEvent(QPaintEvent *event) override;
   void keyPressEvent(QKeyEvent *event) override;
   void mousePressEvent(QMouseEvent *event) override;
   void mouseMoveEvent(QMouseEvent *event) override;
   void mouseReleaseEvent(QMouseEvent *event) override;

private:
   enum Selection { NONE_SELECTED, START_SELECTING, DATA_SELECTED };

   void measureFont(void);
   void setMinimumGrid(int cols, int rows);
   void pointToCell(const QPoint &p, int *x, int *y) const;
   bool clampedSelection(int *x1, int *y1, int *x2, int *y2) const;
   void copySelection(void);
   static QClipboard::Mode clipboardMode(void);

   QFont lcdFont;
   int fontW, fontH, fontAscent;
   Selection dataSelected;
   bool dragging;     /* the left button is down on the display */
   int hiX0, hiY0, hiX1, hiY1, hiX2, hiY2;
};

static LcdWidget *lcd=NULL;

LcdWidget::LcdWidget(int rows, int cols, const char *fontSpec)
   : dataSelected(NONE_SELECTED), dragging(false), hiX0(0), hiY0(0), hiX1(0), hiY1(0), hiX2(0), hiY2(0)
{
   /* the font size always comes from the Options window setting */
   lcdFont=parseFontSpec(fontSpec?fontSpec:DEFAULT_LCD_FONT,0);
   lcdFont.setStyleHint(QFont::TypeWriter);
   lcdFont.setFixedPitch(true);
   lcdFont.setPointSize(getLCDFontSize());
   measureFont();
   setFocusPolicy(Qt::StrongFocus);
   setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
   setAttribute(Qt::WA_OpaquePaintEvent);
   setMinimumGrid(cols,rows);
}

void LcdWidget::measureFont(void){
   QFontMetrics fm(lcdFont);
   fontW=fm.horizontalAdvance(QLatin1Char('M'));
   fontH=fm.height();
   fontAscent=fm.ascent();
   if(fontW<=0) fontW=9;
   if(fontH<=0){ fontH=18; fontAscent=14; }
}

void LcdWidget::setMinimumGrid(int cols, int rows){
   setMinimumSize(2*BORDER+cols*fontW,2*BORDER+rows*fontH);
}

/* keep the same number of rows and columns in the new font */
void LcdWidget::fontChanged(void){
   int cols=lcdWidth, rows=lcdHeight;
   lcdFont.setPointSize(getLCDFontSize());
   measureFont();
   if(cols>0 && rows>0) setMinimumGrid(cols,rows);
   updateGeometry();
   update();
}

void LcdWidget::paintEvent(QPaintEvent *){
   QPainter p(this);
   int i, j, x1, y1, x2, y2, cur;
   bool selected;

   resizeLCD((width()-2*BORDER)/fontW,(height()-2*BORDER)/fontH);
   updateLCD();
   selected=clampedSelection(&x1,&y1,&x2,&y2);

   p.fillRect(rect(),Qt::white);
   p.setFont(lcdFont);
   for(i=0;i<lcdHeight;i++){
      int top=BORDER+i*fontH;
      if(selected && i>=y1 && i<=y2)
         p.fillRect(BORDER+x1*fontW,top,(x2-x1+1)*fontW,fontH,QColor::fromRgbF(0.15,0.25,0.45));
      /* one cell per character, so any font lines up with the grid */
      for(j=0;j<lcdWidth;j++){
         char c=lcdText[i][j];
         if(c==' ' || c=='\0') continue;
         p.setPen(selected && i>=y1 && i<=y2 && j>=x1 && j<=x2 ? Qt::white : Qt::black);
         p.drawText(BORDER+j*fontW,top+fontAscent,QString(QLatin1Char(c)));
      }
   }
   if(isEditingEditor() && lcdHeight>0){
      cur=cursorPosEditor();
      if(cur>lcdWidth) cur=lcdWidth;
      p.setPen(Qt::black);
      p.drawLine(BORDER+cur*fontW-1,BORDER+(lcdHeight-1)*fontH,BORDER+cur*fontW-1,BORDER+lcdHeight*fontH-1);
   }
}

/* keep Tab for ourselves instead of moving the focus */
bool LcdWidget::event(QEvent *event){
   if(event->type()==QEvent::KeyPress){
      QKeyEvent *key=static_cast<QKeyEvent *>(event);
      if(key->key()==Qt::Key_Tab || key->key()==Qt::Key_Backtab){
         keyPressEvent(key);
         return true;
      }
   }
   return QWidget::event(event);
}

void LcdWidget::keyPressEvent(QKeyEvent *event){
   int key=0;
   QString text;

   if(event->modifiers() & Qt::ControlModifier){
      if(event->key()==Qt::Key_C) processInput('c',1,'c','c');   /* Control-C quits */
      return;
   }

   switch(event->key()){
      case Qt::Key_Return: case Qt::Key_Enter: key=GRPN_KEY_Return; break;
      case Qt::Key_Tab:                        key=GRPN_KEY_Tab; break;
      case Qt::Key_Home:                       key=GRPN_KEY_Home; break;
      case Qt::Key_End:                        key=GRPN_KEY_End; break;
      case Qt::Key_Up:                         key=GRPN_KEY_Up; break;
      case Qt::Key_Down:                       key=GRPN_KEY_Down; break;
      case Qt::Key_Left:                       key=GRPN_KEY_Left; break;
      case Qt::Key_Right:                      key=GRPN_KEY_Right; break;
      case Qt::Key_Backspace:                  key=GRPN_KEY_BackSpace; break;
      case Qt::Key_Delete:                     key=GRPN_KEY_Delete; break;
      case Qt::Key_Escape:                     key=GRPN_KEY_Escape; break;
      case Qt::Key_Insert:     /* keypad 0 with Num Lock off */
         if(event->modifiers() & Qt::KeypadModifier) key=GRPN_KEY_Return;
         break;
   }
   if(key){
      processInput(key,0,'\0','\0');
      return;
   }

   text=event->text();
   if(text.size()==1 && text[0]>=QLatin1Char(' ') && text[0]<=QLatin1Char('~')){
      char c=text[0].toLatin1();
      processInput((unsigned char)c,0,c,c);
      return;
   }
   event->ignore();
}

void LcdWidget::pointToCell(const QPoint &p, int *x, int *y) const {
   *x=(p.x()-BORDER)/fontW; *y=(p.y()-BORDER)/fontH;
   if(*x>=lcdWidth) *x=lcdWidth-1;
   if(*y>=lcdHeight) *y=lcdHeight-1;
   if(*x<0) *x=0;
   if(*y<0) *y=0;
}

/* the selection limited to the current display size */
bool LcdWidget::clampedSelection(int *x1, int *y1, int *x2, int *y2) const {
   if(dataSelected!=DATA_SELECTED || !lcdText || hiX1>=lcdWidth || hiY1>=lcdHeight) return false;
   *x1=hiX1; *y1=hiY1;
   *x2=hiX2<lcdWidth?hiX2:lcdWidth-1;
   *y2=hiY2<lcdHeight?hiY2:lcdHeight-1;
   return true;
}

QClipboard::Mode LcdWidget::clipboardMode(void){
   return QApplication::clipboard()->supportsSelection()?QClipboard::Selection:QClipboard::Clipboard;
}

void LcdWidget::copySelection(void){
   int x1, y1, x2, y2, y;
   QString s;
   if(!clampedSelection(&x1,&y1,&x2,&y2)) return;
   for(y=y1;y<=y2;y++){
      s+=QString::fromLatin1(&lcdText[y][x1],x2-x1+1);
      if(y<y2) s+='\n';
   }
   QApplication::clipboard()->setText(s,clipboardMode());
}

void LcdWidget::mousePressEvent(QMouseEvent *event){
   setFocus();
   if(event->button()==Qt::LeftButton){
      pointToCell(event->pos(),&hiX0,&hiY0);
      hiX1=hiX2=hiX0; hiY1=hiY2=hiY0;
      dataSelected=START_SELECTING;
      dragging=true;
      update();
   } else if(event->button()==Qt::MiddleButton){
      QByteArray text=QApplication::clipboard()->text(clipboardMode()).toLatin1();
      for(char c : text) processInput(0,0,c,c);
   }
}

void LcdWidget::mouseMoveEvent(QMouseEvent *event){
   if(!dragging) return;
   pointToCell(event->pos(),&hiX2,&hiY2);
   hiX1=hiX0; hiY1=hiY0;
   if(hiX1>hiX2){ int t=hiX1; hiX1=hiX2; hiX2=t; }
   if(hiY1>hiY2){ int t=hiY1; hiY1=hiY2; hiY2=t; }
   dataSelected=DATA_SELECTED;
   update();
}

void LcdWidget::mouseReleaseEvent(QMouseEvent *event){
   if(event->button()!=Qt::LeftButton || !dragging) return;
   dragging=false;
   if(dataSelected==DATA_SELECTED) copySelection();
   else dataSelected=NONE_SELECTED;
   update();
}

QWidget *setupLCD(int rows, int cols, const char *font){
   lcd=new LcdWidget(rows,cols,font);
   QObject::connect(lcd,&QObject::destroyed,[]{ lcd=NULL; });
   return lcd;
}

extern "C" void lcdQueueDraw(void){
   if(lcd) lcd->update();
}

extern "C" void lcdFontChanged(void){
   if(lcd) lcd->fontChanged();
}
