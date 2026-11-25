/* settings.c: Options window settings saved between runs.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "settings.h"
#include "mode.h"
#include "lcd.h"

#define GROUP "Options"

typedef struct { const char *name; int value; } Choice;
static const Choice bases[]={{"Binary",BINARY},{"Octal",OCTAL},{"Decimal",DECIMAL},{"Hexadecimal",HEXIDECIMAL},{NULL,0}};
static const Choice radixes[]={{"Degrees",DEGREES},{"Radians",RADIANS},{NULL,0}};
static const Choice polars[]={{"Rectangular",RECTANGULAR},{"Polar",POLAR},{NULL,0}};

/* the keys we know about, and their values as read from the file */
enum { K_BASE, K_RADIX, K_COORDINATES, K_SEPARATORS, K_ENG, K_DIGITS, K_FONT_SIZE, K_COUNT };
static const char *const keys[K_COUNT]={"base","radix","coordinates","thousands_separators",
   "engineering_notation","display_digits","font_size"};

/* $XDG_CONFIG_HOME/grpn, or ~/.config/grpn; returns a malloc'd string or NULL */
char *settingsDir(void){
   const char *config=getenv("XDG_CONFIG_HOME"), *home; char *dir; size_t len;
   if(config && *config){
      len=strlen(config)+sizeof("/grpn"); dir=malloc(len);
      if(dir) snprintf(dir,len,"%s/grpn",config);
   } else {
      home=getenv("HOME");
      if(!home || !*home) return NULL;
      len=strlen(home)+sizeof("/.config/grpn"); dir=malloc(len);
      if(dir) snprintf(dir,len,"%s/.config/grpn",home);
   }
   return dir;
}
static char *settings_file(void){
   char *dir=settingsDir(), *path; size_t len;
   if(!dir) return NULL;
   len=strlen(dir)+sizeof("/grpn.conf"); path=malloc(len);
   if(path) snprintf(path,len,"%s/grpn.conf",dir);
   free(dir); return path;
}

/* create dir and any missing parents */
int makeDirs(char *dir){
   char *p;
   for(p=dir+1;*p;p++){
      if(*p=='/'){
         *p='\0';
         if(mkdir(dir,0700)!=0 && errno!=EEXIST){ *p='/'; return -1; }
         *p='/';
      }
   }
   if(mkdir(dir,0700)!=0 && errno!=EEXIST) return -1;
   return 0;
}

static char *strip(char *s){
   char *e;
   while(isspace((unsigned char)*s)) s++;
   e=s+strlen(s);
   while(e>s && isspace((unsigned char)e[-1])) *--e='\0';
   return s;
}

/* look up s in the choices; returns 1 and sets *value if it names one */
static int get_choice(const char *s,const Choice *c,int *value){
   if(!s) return 0;
   for(;c->name;c++) if(!strcasecmp(s,c->name)){ *value=c->value; return 1; }
   return 0;
}
static int get_bool(const char *s,int *value){
   static const char *const yes[]={"true","1","yes","on",NULL}, *const no[]={"false","0","no","off",NULL};
   int i;
   if(!s) return 0;
   for(i=0;yes[i];i++) if(!strcasecmp(s,yes[i])){ *value=1; return 1; }
   for(i=0;no[i];i++) if(!strcasecmp(s,no[i])){ *value=0; return 1; }
   return 0;
}
static int get_int(const char *s,int *value){
   char *end; long n;
   if(!s || !*s) return 0;
   errno=0; n=strtol(s,&end,10);
   if(*end || errno || n<-2147483647L || n>2147483647L) return 0;
   *value=(int)n; return 1;
}

/* Apply saved settings.  A missing file or a missing or invalid value
   leaves the default in place. */
void loadSettings(void){
   char *path=settings_file(), line[1024], *values[K_COUNT]={NULL}, *p, *eq, *key;
   int inGroup=0, i, v; FILE *f;
   if(!path) return;
   f=fopen(path,"r");
   if(!f){
      if(errno!=ENOENT) fprintf(stderr,"grpn: could not read %s: %s\n",path,strerror(errno));
      free(path); return;
   }
   while(fgets(line,sizeof(line),f)){
      p=strip(line);
      if(*p=='\0' || *p=='#' || *p==';') continue;
      if(*p=='['){
         eq=strchr(p,']');
         if(eq){ *eq='\0'; inGroup=!strcmp(strip(p+1),GROUP); }
         continue;
      }
      if(!inGroup || !(eq=strchr(p,'='))) continue;
      *eq='\0'; key=strip(p);
      for(i=0;i<K_COUNT;i++){
         if(!strcasecmp(key,keys[i])){ free(values[i]); values[i]=strdup(strip(eq+1)); break; }
      }
   }
   fclose(f);

   if(get_choice(values[K_BASE],bases,&v)) setBaseMode(v);
   if(get_choice(values[K_RADIX],radixes,&v)) setRadixMode(v);
   if(get_choice(values[K_COORDINATES],polars,&v)) setPolarMode(v);
   if(get_bool(values[K_SEPARATORS],&v)) setThousandsSeparators(v);
   if(get_bool(values[K_ENG],&v)) setEngNotation(v);
   if(get_int(values[K_DIGITS],&v)) setDisplayDigits(v);
   if(get_int(values[K_FONT_SIZE],&v)) setLCDFontSize(v);

   for(i=0;i<K_COUNT;i++) free(values[i]);
   free(path);
}

static const char *choice_name(const Choice *c,int value){
   for(;c->name;c++) if(c->value==value) return c->name;
   return NULL;
}

void saveSettings(void){
   char *dir=settingsDir(), *path=settings_file(); const char *s; FILE *f; int ok;
   if(!dir || !path){
      fprintf(stderr,"grpn: could not save settings: no home directory\n");
      free(dir); free(path); return;
   }
   if(makeDirs(dir)!=0 || !(f=fopen(path,"w"))){
      fprintf(stderr,"grpn: could not save settings to %s: %s\n",path,strerror(errno));
      free(dir); free(path); return;
   }
   fprintf(f,"[%s]\n",GROUP);
   if((s=choice_name(bases,getBaseMode()))) fprintf(f,"base=%s\n",s);
   if((s=choice_name(radixes,getRadixMode()))) fprintf(f,"radix=%s\n",s);
   if((s=choice_name(polars,getPolarMode()))) fprintf(f,"coordinates=%s\n",s);
   fprintf(f,"thousands_separators=%s\n",getThousandsSeparators()?"true":"false");
   fprintf(f,"engineering_notation=%s\n",getEngNotation()?"true":"false");
   fprintf(f,"display_digits=%d\n",getDisplayDigits());
   fprintf(f,"font_size=%d\n",getLCDFontSize());
   ok=!ferror(f); if(fclose(f)!=0) ok=0;
   if(!ok) fprintf(stderr,"grpn: could not save settings to %s: %s\n",path,strerror(errno));
   free(dir); free(path);
}
