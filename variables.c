#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "variables.h"
#include "editor.h"
#include "error.h"
#include "number.h"
#include "stack.h"
#include "undo.h"

typedef struct Variable Variable;
struct Variable {
   char *name;
   Number *value;
   Variable *next;
};

static Variable *variables;
static VariableAddedCallback variable_added_callback;
static void *variable_added_data;

void setVariableAddedCallback(VariableAddedCallback callback, void *data)
{
   variable_added_callback=callback;
   variable_added_data=data;
}

static const char *variable_name(void)
{
   const char *text=getLineEditor();
   const unsigned char *p;

   if(text[0]!='\'' || text[1]=='\0'){
      setStringError("Variable Error: Enter a name beginning with '.");
      return NULL;
   }
   for(p=(const unsigned char *)text+1;*p;p++){
      if(isspace(*p)){
         setStringError("Variable Error: Names cannot contain whitespace.");
         return NULL;
      }
   }
   return text+1;
}

static Variable *find_variable(const char *name)
{
   Variable *variable;
   for(variable=variables;variable;variable=variable->next)
      if(!strcmp(variable->name,name)) return variable;
   return NULL;
}

void StoreVariableStack(void)
{
   const char *name=variable_name();
   Number *top;
   Variable *variable;
   int is_new=0;

   if(!name) return;
   if(stackLen()<1){ setStringError("Sto Error: Too few Arguments."); return; }
   top=getStackEle(0);
   variable=find_variable(name);
   if(!variable){
      variable=calloc(1,sizeof(*variable));
      if(!variable){perror("calloc");exit(EXIT_FAILURE);}
      variable->name=strdup(name);
      if(!variable->name){perror("strdup");exit(EXIT_FAILURE);}
      variable->next=variables; variables=variable;
      is_new=1;
   } else {
      freeNumber(variable->value);
   }
   variable->value=setNumberNumber(newNumber(),top);
   if(is_new && variable_added_callback)
      variable_added_callback(variable->name,variable_added_data);
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
   Variable *variable=find_variable(name);
   if(!variable){ setStringError("Rcl Error: Unknown variable."); return 0; }
   SaveStackState(0);
   Push(setNumberNumber(newNumber(),variable->value));
   UndoStackState(1);
   return 1;
}
