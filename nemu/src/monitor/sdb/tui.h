#ifndef __LAYOUT_H__
#define __LAYOUT_H__
#include <common.h>
int cmd_layout(char *args);

void init_tui();
void deinit_tui();
void refresh_code_window();
#endif // __LAYOUT_H__