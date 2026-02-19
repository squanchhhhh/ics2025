#ifndef __LAYOUT_H__
#define __LAYOUT_H__
#include <common.h>
int cmd_layout(char *args);

void init_tui();
void deinit_tui();
void refresh_code_window();
void get_pc_source(uint32_t pc, char *filename, int *line);
char* get_src(const char *filename, int line) ;
#endif // __LAYOUT_H__