#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <locale.h>
#include <gtk/gtk.h>

#include "stack.h"
#include "menu.h"
#include "mode.h"
#include "lcd.h"
#include "undo.h"
#include "buttons.h"
#include "constant.h"

static int rows=8, cols=30, draw_buttons=1;
static char *button_font=NULL, *display_font=NULL;

static void usage(const char *s){
   fprintf(stderr,"Error: %s\nUsage: grpn [-fn font] [-btn-fn font] [-disp-fn font] [-rows n] [-cols n] [-b]\n",s);
}
static gboolean parse_args(int argc,char **argv){
   int n;
   for(n=1;n<argc;n++){
      if(!strcmp(argv[n],"-b")) draw_buttons=0;
      else if(!strcmp(argv[n],"-fn") || !strcmp(argv[n],"-btn-fn") || !strcmp(argv[n],"-disp-fn")){
         const char *opt=argv[n]; if(++n>=argc){ usage("Missing font argument."); return FALSE; }
         if(!strcmp(opt,"-fn") || !strcmp(opt,"-btn-fn")) button_font=argv[n];
         if(!strcmp(opt,"-fn") || !strcmp(opt,"-disp-fn")) display_font=argv[n];
      } else if(!strcmp(argv[n],"-rows") || !strcmp(argv[n],"-cols")){
         int *out=!strcmp(argv[n],"-rows")?&rows:&cols;
         if(++n>=argc || sscanf(argv[n],"%d",out)!=1 || *out<1){ usage("Invalid display size."); return FALSE; }
      } else { usage("Unknown argument."); return FALSE; }
   }
   return TRUE;
}
static void activate(GtkApplication *app,gpointer data){
   GtkWidget *window=gtk_application_window_new(app), *vbox=gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
   (void)data; gtk_window_set_title(GTK_WINDOW(window),"grpn"); gtk_window_set_child(GTK_WINDOW(window),vbox);
   setup_menu(vbox);
   setupModeDisplay(vbox);
   setupLCD(vbox,rows,cols,display_font);
   if(draw_buttons) setupButtons(vbox);
   if(button_font){
      GtkCssProvider *css=gtk_css_provider_new(); char *rule=g_strdup_printf("button, menubar { font: %s; }",button_font);
      gtk_css_provider_load_from_string(css,rule); gtk_style_context_add_provider_for_display(gtk_widget_get_display(window),GTK_STYLE_PROVIDER(css),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_free(rule); g_object_unref(css);
   }
   gtk_window_present(GTK_WINDOW(window));
}
int main(int argc,char **argv){
   GtkApplication *app; int status;
   setlocale(LC_NUMERIC,"C"); if(!parse_args(argc,argv)) return EXIT_FAILURE;
   setup_constant(); initUndoSystem();
   if(!setup_stack()){ fprintf(stderr,"Error: Could not initialize data structures.\n"); return EXIT_FAILURE; }
   app=gtk_application_new("org.grpn.calculator",G_APPLICATION_DEFAULT_FLAGS);
   g_signal_connect(app,"activate",G_CALLBACK(activate),NULL);
   status=g_application_run(G_APPLICATION(app),0,NULL); g_object_unref(app); return status;
}
