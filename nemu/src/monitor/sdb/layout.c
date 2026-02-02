
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

#include <common.h> // 包含 vaddr_t 等定义

// 外部函数声明
extern void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
extern uint8_t* guest_to_host(paddr_t paddr); 

#define CMD_AREA_HEIGHT 5 // 底部留给命令行的空行数

void ui_draw_asm_view() {
    int max_y, max_x;
    getmaxyx(stdscr, max_y, max_x);

    // 1. 计算窗口尺寸
    int asm_height = max_y - CMD_AREA_HEIGHT;
    if (asm_height < 3) return; // 屏幕太小就不画了

    // 2. 创建/重置窗口
    if (win_asm != NULL) delwin(win_asm);
    win_asm = newwin(asm_height, max_x, 0, 0);

    // 3. 画框和标题
    box(win_asm, 0, 0);
    wattron(win_asm, A_BOLD);
    mvwaddstr(win_asm, 0, 2, " Assembly (Static PC: 0x80000000) ");
    wattroff(win_asm, A_BOLD);

    // 4. 反汇编逻辑
    vaddr_t pc = 0x80000000; // 临时硬编码
    char buf[128];
    int rows = asm_height - 2; // 去掉上下边框可用的行数

    for (int i = 0; i < rows; i++) {
        vaddr_t cur_pc = pc + i * 4;
        
        // 安全读取内存：直接转换物理地址获取指令字节
        // 注意：这里假设 0x80000000 在你的物理内存映射范围内
        uint8_t *code = guest_to_host(cur_pc); 
        
        // 执行反汇编
        disassemble(buf, sizeof(buf), cur_pc, code, 4);

        // 5. 渲染到窗口
        if (i == 0) {
            wattron(win_asm, A_REVERSE); // 第一行高亮
            mvwprintw(win_asm, i + 1, 1, "=> %s", buf);
            wattroff(win_asm, A_REVERSE);
        } else {
            mvwprintw(win_asm, i + 1, 1, "   %s", buf);
        }
    }

    // 6. 刷新窗口缓冲区
    wnoutrefresh(win_asm);
    
    // 7. 【物理隔离关键】强制移动光标到窗口下方
    // 这样下次输入命令时，(nemu) 提示符会出现在窗口下方的空白区
    move(asm_height + 1, 0); 
    refresh();
}
void ui_draw_source_view() {
    // 暂时先留空
}
