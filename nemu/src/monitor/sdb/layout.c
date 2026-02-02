
#include <stdio.h>
#include "layout.h"
WINDOW *win_src = NULL;
WINDOW *win_asm = NULL;
WINDOW *win_cmd = NULL;
bool is_ui_initialized = false;

void ui_init() {
    if (is_ui_initialized) return;
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    is_ui_initialized = true;
}
void ui_clear_all() {
    // 安全地销毁窗口并释放资源
    if (win_src != NULL) {
        delwin(win_src);
        win_src = NULL;
    }
    if (win_asm != NULL) {
        delwin(win_asm);
        win_asm = NULL;
    }
    if (win_cmd != NULL) {
        delwin(win_cmd);
        win_cmd = NULL;
    }
    
    // 清除标准屏幕（stdscr）的内容并准备刷新
    erase(); 
    refresh();
}
void ui_refresh_all() {
    if (win_src != NULL) {
        box(win_src, 0, 0);
        mvwprintw(win_src, 0, 2, " Source ");
        wnoutrefresh(win_src);
    }
    
    if (win_asm != NULL) {
        box(win_asm, 0, 0);
        mvwprintw(win_asm, 0, 2, " Assembly ");
        // 这里可以调用你即将实现的：fill_asm_content(win_asm);
        wnoutrefresh(win_asm);
    }
    
    if (win_cmd != NULL) {
        int screen_width = getmaxx(stdscr);
        (void)mvwhline(win_cmd, 0, 0, ACS_HLINE, screen_width);
        wnoutrefresh(win_cmd);
    }

    doupdate();
}
void ui_draw_split_view() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x); // 获取当前终端总尺寸

    ui_clear_all(); // 切换布局前先销毁旧窗口

    // 1. 源代码窗口 (占上方 40%)
    win_src = newwin(max_y * 0.4, max_x, 0, 0);
    box(win_src, 0, 0);
    mvwprintw(win_src, 0, 2, " Source Code ");

    // 2. 汇编窗口 (占中间 40%)
    win_asm = newwin(max_y * 0.4, max_x, max_y * 0.4, 0);
    box(win_asm, 0, 0);
    mvwprintw(win_asm, 0, 2, " Assembly ");

    // 3. 命令行窗口 (占下方 20%)
    win_cmd = newwin(max_y * 0.2, max_x, max_y * 0.8, 0);
    // 命令行通常不需要 box，或者只需要在上方画一条线
    mvwhline(win_cmd, 0, 0, ACS_HLINE, max_x); 

    ui_refresh_all();
}

int current_layout = LAYOUT_NONE; 

void ui_set_layout(int mode) {
    ui_init();
    current_layout = mode;
    ui_clear_all();
    switch(mode) {
        case LAYOUT_ASM:   ui_draw_asm_view(); break;
        case LAYOUT_SPLIT: ui_draw_split_view(); break;
        case LAYOUT_SRC:   ui_draw_source_view(); break;
    }
    ui_refresh_all();
}
void ui_draw_asm_view() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // 计算分界线：假设下方留 5 行给命令行
    int cmd_height = 5;
    int asm_height = max_y - cmd_height;

    // 创建上方窗口 (高度为 asm_height, 宽度全屏, 从 0,0 开始)
    win_asm = newwin(asm_height, max_x, 0, 0);
    
    // 画个漂亮的边框
    box(win_asm, 0, 0);
    mvwprintw(win_asm, 0, 2, " Assembly View (PC: 0x%08x) ", 0x80000000);

    // 在窗口内显示点测试代码（稍后换成真正的反汇编）
    for (int i = 1; i < asm_height - 1; i++) {
        mvwprintw(win_asm, i, 2, "0x%08x:  addi x0, x0, 0", 0x80000000+ (i-1)*4);
    }

    wnoutrefresh(win_asm);
    // 注意：这里不要在下方创建 win_cmd，
    // 因为 readline 会直接在屏幕最后几行输出。
}
void ui_draw_source_view() {
    // 暂时先留空
}
