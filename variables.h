#ifndef GRPN_VARIABLES_H
#define GRPN_VARIABLES_H

void StoreVariableStack(void);
void RecallVariableStack(void);
int RecallVariableNamed(const char *name);

typedef void (*VariableAddedCallback)(const char *name, void *data);
void setVariableAddedCallback(VariableAddedCallback callback, void *data);

#endif
