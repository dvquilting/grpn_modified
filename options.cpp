/* options.cpp: the Options window.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialog>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMainWindow>
#include <QPointer>
#include <QPushButton>
#include <QRadioButton>
#include <QSpinBox>
#include <QVBoxLayout>

#include <functional>

#include "gui.h"
#include "mode.h"
#include "lcd.h"

class OptionsWindow : public QDialog {
public:
   OptionsWindow(QWidget *parent);
   void sync(void);

private:
   QHBoxLayout *section(const char *title);
   QButtonGroup *choices(const char *title, const char *const labels[], const int values[], int count,
                         std::function<void(int)> set);

   QVBoxLayout *vbox;
   QButtonGroup *base, *radix, *cmode;
   QCheckBox *separators, *eng;
   QSpinBox *digits, *fontSize;
};

static QPointer<OptionsWindow> optWindow;

QHBoxLayout *OptionsWindow::section(const char *title){
   QGroupBox *frame=new QGroupBox(title);
   QHBoxLayout *box=new QHBoxLayout(frame);
   box->setSpacing(8);
   vbox->addWidget(frame);
   return box;
}

/* a row of radio buttons; set() is called with the value of the one chosen */
QButtonGroup *OptionsWindow::choices(const char *title, const char *const labels[], const int values[], int count,
                                     std::function<void(int)> set){
   QHBoxLayout *box=section(title);
   QButtonGroup *group=new QButtonGroup(this);
   for(int i=0;i<count;i++){
      QRadioButton *b=new QRadioButton(labels[i]);
      group->addButton(b,values[i]);
      box->addWidget(b);
   }
   box->addStretch();
   connect(group,QOverload<QAbstractButton *,bool>::of(&QButtonGroup::buttonToggled),
           [group,set](QAbstractButton *b,bool on){ if(on) set(group->id(b)); });
   return group;
}

OptionsWindow::OptionsWindow(QWidget *parent) : QDialog(parent)
{
   static const char *const baseLabels[]={"Bin","Oct","Dec","Hex"};
   static const int baseValues[]={BINARY,OCTAL,DECIMAL,HEXIDECIMAL};
   static const char *const radixLabels[]={"Degrees","Radians"};
   static const int radixValues[]={DEGREES,RADIANS};
   static const char *const cmodeLabels[]={"Rectangular","Polar"};
   static const int cmodeValues[]={RECTANGULAR,POLAR};
   QHBoxLayout *box;
   QPushButton *dismiss;

   setWindowTitle("Options");
   setAttribute(Qt::WA_DeleteOnClose);
   vbox=new QVBoxLayout(this);
   vbox->setSizeConstraint(QLayout::SetFixedSize);

   base=choices("Base",baseLabels,baseValues,4,baseCB);
   radix=choices("Radix",radixLabels,radixValues,2,radixCB);
   cmode=choices("Coordinate Mode",cmodeLabels,cmodeValues,2,cmodeCB);

   box=section("Number Formatting");
   separators=new QCheckBox("Thousands separators");
   connect(separators,&QCheckBox::toggled,[](bool on){ setThousandsSeparators(on); redrawLCD(); });
   box->addWidget(separators);
   eng=new QCheckBox("Engineering notation");
   connect(eng,&QCheckBox::toggled,[](bool on){ setEngNotation(on); redrawLCD(); });
   box->addWidget(eng);

   box=section("Maximum Displayed Digits");
   digits=new QSpinBox;
   digits->setRange(MIN_DISPLAY_DIGITS,MAX_DISPLAY_DIGITS);
   connect(digits,QOverload<int>::of(&QSpinBox::valueChanged),[](int n){ setDisplayDigits(n); redrawLCD(); });
   box->addWidget(digits);
   box->addStretch();

   box=section("Display Font Size");
   fontSize=new QSpinBox;
   fontSize->setRange(8,36);
   connect(fontSize,QOverload<int>::of(&QSpinBox::valueChanged),[](int n){ setLCDFontSize(n); });
   box->addWidget(fontSize);
   box->addStretch();

   dismiss=new QPushButton("Dismiss");
   connect(dismiss,&QPushButton::clicked,this,&QDialog::close);
   vbox->addWidget(dismiss);

   sync();
}

/* show the current settings without acting on them */
void OptionsWindow::sync(void){
   QWidget *widgets[]={separators,eng,digits,fontSize};
   QButtonGroup *groups[]={base,radix,cmode};
   for(QWidget *w : widgets) w->blockSignals(true);
   for(QButtonGroup *g : groups) g->blockSignals(true);
   if(QAbstractButton *b=base->button(getBaseMode())) b->setChecked(true);
   if(QAbstractButton *b=radix->button(getRadixMode())) b->setChecked(true);
   if(QAbstractButton *b=cmode->button(getPolarMode())) b->setChecked(true);
   separators->setChecked(getThousandsSeparators());
   eng->setChecked(getEngNotation());
   digits->setValue(getDisplayDigits());
   fontSize->setValue(getLCDFontSize());
   for(QWidget *w : widgets) w->blockSignals(false);
   for(QButtonGroup *g : groups) g->blockSignals(false);
}

/* pops up a window with all the options in it */
void optionsPopup(void){
   if(!optWindow) optWindow=new OptionsWindow(mainWindow());
   optWindow->sync();
   optWindow->show();
   optWindow->raise();
   optWindow->activateWindow();
}

void syncOptionsWindow(void){
   if(optWindow) optWindow->sync();
}
