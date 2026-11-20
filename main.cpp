/* main.cpp: start up GRPN and build the main window.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <clocale>

#include <QApplication>
#include <QCloseEvent>
#include <QFont>
#include <QLabel>
#include <QMainWindow>
#include <QMenuBar>
#include <QStringList>
#include <QVBoxLayout>
#include <QWidget>

#include "gui.h"
#include "mode.h"
#include "settings.h"

static int rows=8, cols=30, draw_buttons=1;
static char *button_font=NULL, *display_font=NULL;

static QMainWindow *window=NULL;
static QLabel *modeDisplay=NULL;

static void usage(const char *s){
   fprintf(stderr,"Error: %s\nUsage: grpn [-fn font] [-btn-fn font] [-disp-fn font] [-rows n] [-cols n] [-b]\n",s);
}
static bool parse_args(int argc,char **argv){
   int n;
   for(n=1;n<argc;n++){
      if(!strcmp(argv[n],"-b")) draw_buttons=0;
      else if(!strcmp(argv[n],"-fn") || !strcmp(argv[n],"-btn-fn") || !strcmp(argv[n],"-disp-fn")){
         const char *opt=argv[n]; if(++n>=argc){ usage("Missing font argument."); return false; }
         if(!strcmp(opt,"-fn") || !strcmp(opt,"-btn-fn")) button_font=argv[n];
         if(!strcmp(opt,"-fn") || !strcmp(opt,"-disp-fn")) display_font=argv[n];
      } else if(!strcmp(argv[n],"-rows") || !strcmp(argv[n],"-cols")){
         int *out=!strcmp(argv[n],"-rows")?&rows:&cols;
         if(++n>=argc || sscanf(argv[n],"%d",out)!=1 || *out<1){ usage("Invalid display size."); return false; }
      } else { usage("Unknown argument."); return false; }
   }
   return true;
}

QFont parseFontSpec(const char *spec, double defaultSize){
   QStringList words=QString::fromLocal8Bit(spec).simplified().split(' ');
   QFont font; double size=defaultSize; bool ok=false;
   words.removeAll(QString());
   if(!words.isEmpty()){
      double n=words.last().toDouble(&ok);
      if(ok && n>0){ size=n; words.removeLast(); }
   }
   while(!words.isEmpty()){
      QString style=words.last().toLower();
      if(style=="bold") font.setBold(true);
      else if(style=="italic" || style=="oblique") font.setItalic(true);
      else break;
      words.removeLast();
   }
   if(!words.isEmpty()) font.setFamily(words.join(' '));
   if(size>0) font.setPointSizeF(size);
   return font;
}

class MainWindow : public QMainWindow {
protected:
   void closeEvent(QCloseEvent *event) override {
      QMainWindow::closeEvent(event);
      quitApplication();      /* also closes any open dialogs */
   }
};

QMainWindow *mainWindow(void){ return window; }

QWidget *setupModeDisplay(void){
   modeDisplay=new QLabel(" ");
   modeDisplay->setAlignment(Qt::AlignCenter);
   refreshModeDisplay();
   return modeDisplay;
}

extern "C" void refreshModeDisplay(void){
   if(modeDisplay) modeDisplay->setText(getModeDisplayString());
   syncOptionsWindow();
}

extern "C" void quitApplication(void){
   QApplication::quit();
}

int main(int argc,char **argv){
   /* Qt's own command line options are not accepted */
   static int qtArgc=1;
   static char *qtArgv[]={argv[0],NULL};
   int status;

   if(!parse_args(argc,argv)) return EXIT_FAILURE;
   QApplication app(qtArgc,qtArgv);
   app.setApplicationName("grpn");
   /* QApplication sets the locale from the environment; numbers are
      always read and printed with a '.' radix point */
   setlocale(LC_NUMERIC,"C");

   loadSettings();
   if(!initCalculator()){ fprintf(stderr,"Error: Could not initialize data structures.\n"); return EXIT_FAILURE; }

   window=new MainWindow;
   window->setWindowTitle("grpn");
   setupMenu(window);
   QWidget *central=new QWidget;
   QVBoxLayout *vbox=new QVBoxLayout(central);
   vbox->setContentsMargins(0,0,0,0); vbox->setSpacing(0);
   vbox->addWidget(setupModeDisplay());
   QWidget *lcd=setupLCD(rows,cols,display_font);
   vbox->addWidget(lcd,1);
   if(draw_buttons) vbox->addWidget(setupButtons(button_font));
   if(button_font) window->menuBar()->setFont(parseFontSpec(button_font,0));
   window->setCentralWidget(central);
   window->show();
   lcd->setFocus();

   status=app.exec();
   saveSettings();
   delete window; window=NULL; modeDisplay=NULL;
   return status;
}
