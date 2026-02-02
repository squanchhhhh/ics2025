#ifndef __LAYOUT_H__
#define __LAYOUT_H__
#include <ncurses.h>
// 枚举成员应该是整数，用于 switch-case 逻辑
enum {
    LAYOUT_ASM,
    LAYOUT_SPLIT,
    LAYOUT_SRC,
    LAYOUT_NONE  // 默认状态，比如普通的命令行模式
};

// 全局变量，记录当前处于哪种布局（定义在对应的 .c 文件中）
extern int current_layout;

// 布局切换与绘制函数
void ui_set_layout(int mode);
void ui_draw_asm_view();
void ui_draw_split_view();
void ui_draw_source_view();

// 基础管理函数
void ui_init();        // 初始化 ncurses
void ui_clear_all();   // 清除所有窗口
void ui_refresh_all(); // 刷新所有窗口

#endif // __LAYOUT_H__