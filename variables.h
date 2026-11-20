#ifndef GRPN_VARIABLES_H
#define GRPN_VARIABLES_H

#ifdef __cplusplus
extern "C" {
#endif

void StoreVariableStack(void);
void RecallVariableStack(void);
int RecallVariableNamed(const char *name);

typedef void (*VariableAddedCallback)(const char *name, void *data);
void setVariableAddedCallback(VariableAddedCallback callback, void *data);

#ifdef __cplusplus
}
#endif

#endif
