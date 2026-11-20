/* settings.c: Options window settings saved between runs.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <errno.h>
#include <glib.h>
#include <glib/gstdio.h>

#include "settings.h"
#include "mode.h"
#include "lcd.h"

#define GROUP "Options"

typedef struct { const char *name; int value; } Choice;
static const Choice bases[]={{"Binary",BINARY},{"Octal",OCTAL},{"Decimal",DECIMAL},{"Hexadecimal",HEXIDECIMAL},{NULL,0}};
static const Choice radixes[]={{"Degrees",DEGREES},{"Radians",RADIANS},{NULL,0}};
static const Choice polars[]={{"Rectangular",RECTANGULAR},{"Polar",POLAR},{NULL,0}};

static char *settings_dir(void){ return g_build_filename(g_get_user_config_dir(),"grpn",NULL); }
static char *settings_file(void){ return g_build_filename(g_get_user_config_dir(),"grpn","grpn.conf",NULL); }

static void set_choice(GKeyFile *kf,const char *key,const Choice *c,int value){
   for(;c->name;c++) if(c->value==value){ g_key_file_set_string(kf,GROUP,key,c->name); return; }
}
/* look up key in the choices; returns 1 and sets *value if it names one */
static int get_choice(GKeyFile *kf,const char *key,const Choice *c,int *value){
   char *s=g_key_file_get_string(kf,GROUP,key,NULL); int found=0;
   if(s){
      g_strstrip(s);
      for(;c->name;c++) if(!g_ascii_strcasecmp(s,c->name)){ *value=c->value; found=1; break; }
      g_free(s);
   }
   return found;
}
static int get_bool(GKeyFile *kf,const char *key,int *value){
   GError *err=NULL; gboolean b=g_key_file_get_boolean(kf,GROUP,key,&err);
   if(err){ g_error_free(err); return 0; }
   *value=b; return 1;
}
static int get_int(GKeyFile *kf,const char *key,int *value){
   GError *err=NULL; int n=g_key_file_get_integer(kf,GROUP,key,&err);
   if(err){ g_error_free(err); return 0; }
   *value=n; return 1;
}

/* Apply saved settings.  A missing file or a missing or invalid value
   leaves the default in place. */
void loadSettings(void){
   GKeyFile *kf=g_key_file_new(); GError *err=NULL; char *path=settings_file(); int v;
   if(!g_key_file_load_from_file(kf,path,G_KEY_FILE_NONE,&err)){
      if(!g_error_matches(err,G_FILE_ERROR,G_FILE_ERROR_NOENT))
         fprintf(stderr,"grpn: could not read %s: %s\n",path,err->message);
      g_error_free(err);
   } else {
      if(get_choice(kf,"base",bases,&v)) setBaseMode(v);
      if(get_choice(kf,"radix",radixes,&v)) setRadixMode(v);
      if(get_choice(kf,"coordinates",polars,&v)) setPolarMode(v);
      if(get_bool(kf,"thousands_separators",&v)) setThousandsSeparators(v);
      if(get_bool(kf,"engineering_notation",&v)) setEngNotation(v);
      if(get_int(kf,"display_digits",&v)) setDisplayDigits(v);
      if(get_int(kf,"font_size",&v)) setLCDFontSize(v);
   }
   g_free(path); g_key_file_free(kf);
}

void saveSettings(void){
   GKeyFile *kf=g_key_file_new(); GError *err=NULL; char *dir=settings_dir(), *path=settings_file();
   set_choice(kf,"base",bases,getBaseMode());
   set_choice(kf,"radix",radixes,getRadixMode());
   set_choice(kf,"coordinates",polars,getPolarMode());
   g_key_file_set_boolean(kf,GROUP,"thousands_separators",getThousandsSeparators());
   g_key_file_set_boolean(kf,GROUP,"engineering_notation",getEngNotation());
   g_key_file_set_integer(kf,GROUP,"display_digits",getDisplayDigits());
   g_key_file_set_integer(kf,GROUP,"font_size",getLCDFontSize());
   if(g_mkdir_with_parents(dir,0700)!=0 || !g_key_file_save_to_file(kf,path,&err)){
      fprintf(stderr,"grpn: could not save settings to %s: %s\n",path,err?err->message:g_strerror(errno));
      if(err) g_error_free(err);
   }
   g_free(dir); g_free(path); g_key_file_free(kf);
}
