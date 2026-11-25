/* variables.h: named variables (Sto, Rcl, Purge), saved between runs.

The variables live in $XDG_CONFIG_HOME/grpn/variables (normally
~/.config/grpn/variables), a file shared with grpn_py.py.  It is
rewritten whenever a variable is stored or purged.  Each line is a
name followed by its value:

   value := r REAL | c REAL REAL | m ROWS COLS value...
   REAL  := a decimal number such as -1.25e-3, or nan or inf

"c" is a complex number (real part, imaginary part); "m" is a matrix
whose ROWS*COLS entries follow in row order.  Lines that are blank or
start with # are ignored.
*/

#ifndef GRPN_VARIABLES_H
#define GRPN_VARIABLES_H

#ifdef __cplusplus
extern "C" {
#endif

void StoreVariableStack(void);
void RecallVariableStack(void);
void PurgeVariableStack(void);
int RecallVariableNamed(const char *name);

/* the variables, oldest first */
int variableCount(void);
const char *variableName(int i);

/* called after a variable is added or purged */
typedef void (*VariablesChangedCallback)(void *data);
void setVariablesChangedCallback(VariablesChangedCallback callback, void *data);

/* read the saved variables; a missing file means there are none */
void loadVariables(void);

#ifdef __cplusplus
}
#endif

#endif
