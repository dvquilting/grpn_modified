/* variables.c: named variables (Sto, Rcl, Purge), saved between runs.
   See variables.h for the file format. */

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variables.h"
#include "editor.h"
#include "error.h"
#include "number.h"
#include "settings.h"
#include "stack.h"
#include "undo.h"

typedef struct {
   char *name;
   Number *value;
} Variable;

/* in the order they were first stored */
static Variable *variables;
static int nvariables, capacity;
static VariablesChangedCallback changed_callback;
static void *changed_data;

/* a matrix nested deeper than this in the file is rejected */
#define MAX_NESTING 16

void setVariablesChangedCallback(VariablesChangedCallback callback, void *data)
{
   changed_callback=callback;
   changed_data=data;
}

int variableCount(void){ return nvariables; }
const char *variableName(int i){ return i>=0 && i<nvariables ? variables[i].name : NULL; }

static void changed(void)
{
   if(changed_callback) changed_callback(changed_data);
}

static int valid_name(const char *name)
{
   const unsigned char *p;
   if(!*name) return 0;
   for(p=(const unsigned char *)name;*p;p++) if(isspace(*p) || iscntrl(*p)) return 0;
   return 1;
}

static const char *variable_name(void)
{
   const char *text=getLineEditor();

   if(text[0]!='\'' || text[1]=='\0'){
      setStringError("Variable Error: Enter a name beginning with '.");
      return NULL;
   }
   if(!valid_name(text+1)){
      setStringError("Variable Error: Names cannot contain whitespace.");
      return NULL;
   }
   return text+1;
}

static int find_variable(const char *name)
{
   int i;
   for(i=0;i<nvariables;i++)
      if(!strcmp(variables[i].name,name)) return i;
   return -1;
}

/* set name to a copy of value; returns 1 if the name is new */
static int set_variable(const char *name, Number *value)
{
   int i=find_variable(name);
   if(i>=0){
      freeNumber(variables[i].value);
      variables[i].value=setNumberNumber(newNumber(),value);
      return 0;
   }
   if(nvariables==capacity){
      int n=capacity?capacity*2:16;
      Variable *v=realloc(variables,n*sizeof(*v));
      if(!v){perror("realloc");exit(EXIT_FAILURE);}
      variables=v; capacity=n;
   }
   variables[nvariables].name=strdup(name);
   if(!variables[nvariables].name){perror("strdup");exit(EXIT_FAILURE);}
   variables[nvariables].value=setNumberNumber(newNumber(),value);
   nvariables++;
   return 1;
}

/* --- the saved file ---------------------------------------------------- */

static char *variables_file(void)
{
   char *dir=settingsDir(), *path; size_t len;
   if(!dir) return NULL;
   len=strlen(dir)+sizeof("/variables"); path=malloc(len);
   if(path) snprintf(path,len,"%s/variables",dir);
   free(dir); return path;
}

static void write_real(FILE *f, Real *r)
{
   mpfr_exp_t exp; char *digits;
   if(r->ok==REAL_NAN){ fputs(" nan",f); return; }
   if(r->ok==REAL_INF){ fputs(" inf",f); return; }
   /* enough digits to read back exactly */
   digits=mpfr_get_str(NULL,&exp,10,0,r->num,MPFR_RNDN);
   if(!digits){ fputs(" nan",f); return; }
   {  /* trailing zeros add nothing */
      char *mantissa=digits[0]=='-'?digits+1:digits, *end=mantissa+strlen(mantissa);
      while(end>mantissa+1 && end[-1]=='0') *--end='\0';
      if(mpfr_zero_p(r->num)) exp=0;
      fprintf(f," %s0.%se%ld",digits[0]=='-'?"-":"",mantissa,(long)exp);
   }
   mpfr_free_str(digits);
}

static void write_number(FILE *f, Number *n)
{
   Cmplx *c; Matrix *m; int i;
   switch(n->type){
      case REAL:
         fputs(" r",f); write_real(f,n->data); break;
      case COMPLEX:
         c=n->data; fputs(" c",f); write_real(f,c->re); write_real(f,c->im); break;
      case MATRIX:
         m=n->data; fprintf(f," m %d %d",m->rows,m->cols);
         for(i=0;i<m->rows*m->cols;i++) write_number(f,m->data[i]);
         break;
   }
}

/* Rewrite the file.  It is written beside the old one and renamed over
   it, so a failed write leaves the old file in place. */
static void save_variables(void)
{
   char *dir=settingsDir(), *path=variables_file(), *tmp=NULL; FILE *f=NULL; int i, ok=0;
   size_t len;

   if(!dir || !path){
      fprintf(stderr,"grpn: could not save variables: no home directory\n");
      goto done;
   }
   len=strlen(path)+sizeof(".tmp"); tmp=malloc(len);
   if(!tmp) goto fail;
   snprintf(tmp,len,"%s.tmp",path);
   if(makeDirs(dir)!=0 || !(f=fopen(tmp,"w"))) goto fail;
   fputs("# GRPN variables (see variables.h in the grpn sources)\n",f);
   for(i=0;i<nvariables;i++){
      fputs(variables[i].name,f);
      write_number(f,variables[i].value);
      fputc('\n',f);
   }
   ok=!ferror(f);
   if(fclose(f)!=0) ok=0;
   if(ok && rename(tmp,path)!=0) ok=0;
   if(ok) goto done;
fail:
   fprintf(stderr,"grpn: could not save variables to %s: %s\n",path,strerror(errno));
   if(tmp) remove(tmp);
done:
   free(dir); free(path); free(tmp);
}

/* next whitespace-separated token from *s, or NULL */
static char *token(char **s)
{
   char *p=*s, *start;
   while(isspace((unsigned char)*p)) p++;
   if(!*p){ *s=p; return NULL; }
   start=p;
   while(*p && !isspace((unsigned char)*p)) p++;
   if(*p) *p++='\0';
   *s=p;
   return start;
}

static Real *read_real(char **s)
{
   char *t=token(s), *end;
   Real *r;
   if(!t) return NULL;
   r=setRealString(newReal(),t,10,&end);
   /* NaN and Inf only when written as such, not from an overflow */
   if(*end || (r->ok!=REAL_OK && strcmp(t,"nan") && strcmp(t,"inf"))){ freeReal(r); return NULL; }
   return r;
}

static Number *read_number(char **s, int depth)
{
   char *t=token(s), *end;
   Number *n=NULL;

   if(!t || depth>MAX_NESTING) return NULL;
   if(!strcmp(t,"r")){
      Real *r=read_real(s);
      if(r){ n=setNumberReal(newNumber(),r); freeReal(r); }
   } else if(!strcmp(t,"c")){
      Real *re=read_real(s), *im=re?read_real(s):NULL;
      if(re && im){
         Cmplx *c=setCmplxReal(newCmplx(),re,im);
         n=setNumberCmplx(newNumber(),c);
         freeCmplx(c);
      }
      freeReal(re); freeReal(im);
   } else if(!strcmp(t,"m")){
      char *rt=token(s), *ct=rt?token(s):NULL;
      long rows, cols; int i, j;
      Matrix *m;
      if(!ct) return NULL;
      rows=strtol(rt,&end,10); if(*end) return NULL;
      cols=strtol(ct,&end,10); if(*end) return NULL;
      if(rows<1 || cols<1 || rows>1000000 || cols>1000000/rows) return NULL;
      m=newMatrix();
      for(i=0;i<rows;i++){
         for(j=0;j<cols;j++){
            Number *e=read_number(s,depth+1);
            if(!e){ freeMatrix(m); return NULL; }
            setMatrix(m,e,i,j);
            freeNumber(e);
         }
      }
      n=setNumberMatrix(newNumber(),m);
      freeMatrix(m);
   }
   return n;
}

void loadVariables(void)
{
   char *path=variables_file(), *line=NULL, *p, *name;
   size_t size=0; FILE *f; int lineno=0;
   Number *value;

   if(!path) return;
   f=fopen(path,"r");
   if(!f){
      if(errno!=ENOENT) fprintf(stderr,"grpn: could not read %s: %s\n",path,strerror(errno));
      free(path); return;
   }
   while(getline(&line,&size,f)>=0){
      lineno++;
      p=line;
      while(isspace((unsigned char)*p)) p++;
      if(!*p || *p=='#') continue;
      name=token(&p);
      value=read_number(&p,0);
      if(!value || token(&p) || !valid_name(name)){
         fprintf(stderr,"grpn: %s:%d: ignoring unreadable variable\n",path,lineno);
         if(value) freeNumber(value);
         continue;
      }
      set_variable(name,value);
      freeNumber(value);
   }
   free(line);
   fclose(f);
   free(path);
   changed();
}

/* --- the commands ---------------------------------------------------- */

void StoreVariableStack(void)
{
   const char *name=variable_name();

   if(!name) return;
   if(stackLen()<1){ setStringError("Sto Error: Too few Arguments."); return; }
   if(set_variable(name,getStackEle(0))) changed();
   save_variables();
   cancelEditor();
}

void RecallVariableStack(void)
{
   const char *name=variable_name();
   if(!name) return;
   if(RecallVariableNamed(name)) cancelEditor();
}

int RecallVariableNamed(const char *name)
{
   int i=find_variable(name);
   if(i<0){ setStringError("Rcl Error: Unknown variable."); return 0; }
   SaveStackState(0);
   Push(setNumberNumber(newNumber(),variables[i].value));
   UndoStackState(1);
   return 1;
}

void PurgeVariableStack(void)
{
   const char *name=variable_name();
   int i;

   if(!name) return;
   i=find_variable(name);
   if(i<0){ setStringError("Purge Error: Unknown variable."); return; }
   free(variables[i].name);
   freeNumber(variables[i].value);
   memmove(variables+i,variables+i+1,(nvariables-i-1)*sizeof(*variables));
   nvariables--;
   changed();
   save_variables();
   cancelEditor();
}
