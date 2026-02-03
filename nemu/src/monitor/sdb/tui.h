#ifndef __LAYOUT_H__
#define __LAYOUT_H__
#include <common.h>
int cmd_layout(char *args);

// 如果需要手动刷新 UI 或处理特殊输入，可以在这里加声明
void init_tui();
void deinit_tui();

#endif // __LAYOUT_H__