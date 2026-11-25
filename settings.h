/* settings.h: Options window settings saved between runs.

The settings live in $XDG_CONFIG_HOME/grpn/grpn.conf (normally
~/.config/grpn/grpn.conf), a key file shared with grpn_py.py.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.
*/

#ifndef __SETTINGS_H
#define __SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

void loadSettings(void);
void saveSettings(void);

/* $XDG_CONFIG_HOME/grpn, or ~/.config/grpn, as a malloc'd string;
   NULL if there is no home directory */
char *settingsDir(void);
/* create dir and any missing parents; 0 on success */
int makeDirs(char *dir);

#ifdef __cplusplus
}
#endif

#endif
