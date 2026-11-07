#include <gtk/gtk.h>
#include "options.h"
#include "mode.h"
#include "lcd.h"

extern void baseToggleCB(GtkWidget *, gpointer); extern void radixToggleCB(GtkWidget *, gpointer);
extern void cmodeToggleCB(GtkWidget *, gpointer); extern void separatorsToggleCB(GtkWidget *, gpointer);
extern void engNotationToggleCB(GtkWidget *, gpointer);
GtkWidget *optBinBtn, *optOctBtn, *optDecBtn, *optHexBtn, *optWindow = NULL;

static void destroyed(GtkWidget *w, gpointer d){ (void)w; (void)d; optWindow=NULL; }
static void dismiss(GtkButton *b, gpointer d){ (void)b; gtk_window_destroy(GTK_WINDOW(d)); }
static void font_size_changed(GtkSpinButton *button, gpointer data){
   (void)data; setLCDFontSize(gtk_spin_button_get_value_as_int(button));
}
static void digits_changed(GtkSpinButton *button, gpointer data){
   (void)data; setDisplayDigits(gtk_spin_button_get_value_as_int(button)); redrawLCD();
}
static GtkWidget *choice(GtkWidget *box, GtkWidget *group, const char *label, gboolean active, GCallback cb, gpointer data){
   GtkWidget *b=gtk_check_button_new_with_label(label);
   if(group) gtk_check_button_set_group(GTK_CHECK_BUTTON(b), GTK_CHECK_BUTTON(group));
   g_signal_connect(b,"toggled",cb,data); gtk_check_button_set_active(GTK_CHECK_BUTTON(b),active);
   gtk_box_append(GTK_BOX(box),b); return b;
}
static GtkWidget *section(GtkWidget *vbox,const char *title){
   GtkWidget *f=gtk_frame_new(title), *box=gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
   gtk_widget_set_margin_start(f,5); gtk_widget_set_margin_end(f,5); gtk_widget_set_margin_top(f,5); gtk_widget_set_margin_bottom(f,5);
   gtk_frame_set_child(GTK_FRAME(f),box); gtk_box_append(GTK_BOX(vbox),f); return box;
}
void optionsCB(gpointer data){
   GtkWidget *vbox,*box,*first,*button; (void)data;
   if(optWindow){ gtk_window_present(GTK_WINDOW(optWindow)); return; }
   optWindow=gtk_window_new(); gtk_window_set_title(GTK_WINDOW(optWindow),"Options"); gtk_window_set_resizable(GTK_WINDOW(optWindow),FALSE);
   g_signal_connect(optWindow,"destroy",G_CALLBACK(destroyed),NULL);
   vbox=gtk_box_new(GTK_ORIENTATION_VERTICAL,0); gtk_window_set_child(GTK_WINDOW(optWindow),vbox);
   box=section(vbox,"Base");
   first=optBinBtn=choice(box,NULL,"Bin",getBaseMode()==BINARY,G_CALLBACK(baseToggleCB),GINT_TO_POINTER(BINARY));
   optOctBtn=choice(box,first,"Oct",getBaseMode()==OCTAL,G_CALLBACK(baseToggleCB),GINT_TO_POINTER(OCTAL));
   optDecBtn=choice(box,first,"Dec",getBaseMode()==DECIMAL,G_CALLBACK(baseToggleCB),GINT_TO_POINTER(DECIMAL));
   optHexBtn=choice(box,first,"Hex",getBaseMode()==HEXIDECIMAL,G_CALLBACK(baseToggleCB),GINT_TO_POINTER(HEXIDECIMAL));
   box=section(vbox,"Radix"); first=choice(box,NULL,"Degrees",getRadixMode()==DEGREES,G_CALLBACK(radixToggleCB),GINT_TO_POINTER(DEGREES));
   choice(box,first,"Radians",getRadixMode()==RADIANS,G_CALLBACK(radixToggleCB),GINT_TO_POINTER(RADIANS));
   box=section(vbox,"Coordinate Mode"); first=choice(box,NULL,"Rectangular",getPolarMode()==RECTANGULAR,G_CALLBACK(cmodeToggleCB),GINT_TO_POINTER(RECTANGULAR));
   choice(box,first,"Polar",getPolarMode()==POLAR,G_CALLBACK(cmodeToggleCB),GINT_TO_POINTER(POLAR));
   box=section(vbox,"Number Formatting");
   choice(box,NULL,"Thousands separators",getThousandsSeparators(),G_CALLBACK(separatorsToggleCB),NULL);
   choice(box,NULL,"Engineering notation",getEngNotation(),G_CALLBACK(engNotationToggleCB),NULL);
   box=section(vbox,"Maximum Displayed Digits");
   button=gtk_spin_button_new_with_range(MIN_DISPLAY_DIGITS,MAX_DISPLAY_DIGITS,1);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(button),getDisplayDigits());
   g_signal_connect(button,"value-changed",G_CALLBACK(digits_changed),NULL);
   gtk_box_append(GTK_BOX(box),button);
   box=section(vbox,"Display Font Size");
   button=gtk_spin_button_new_with_range(8,36,1);
   gtk_spin_button_set_value(GTK_SPIN_BUTTON(button),getLCDFontSize());
   g_signal_connect(button,"value-changed",G_CALLBACK(font_size_changed),NULL);
   gtk_box_append(GTK_BOX(box),button);
   button=gtk_button_new_with_label("Dismiss"); g_signal_connect(button,"clicked",G_CALLBACK(dismiss),optWindow); gtk_box_append(GTK_BOX(vbox),button);
   gtk_window_present(GTK_WINDOW(optWindow));
}
