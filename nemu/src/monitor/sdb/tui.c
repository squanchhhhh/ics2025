/* tui.c */
#include <common.h>
#include <isa.h>
#include "sdb.h"
#include "tui.h"
#include <memory/vaddr.h>
bool is_tui_mode = false;
WINDOW *tui_win = NULL;  // 下半部分：命令窗口
WINDOW *code_win = NULL; // 上半部分：汇编窗口

void init_tui() {
    initscr();
    start_color(); // 开启颜色
    init_pair(2, COLOR_GREEN, COLOR_BLACK); // 对应你代码里的 COLOR_PAIR(2)
    
    cbreak();
    noecho();
    
    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    // 1. 创建上半部分的汇编窗口（占据上方 rows-10 行）
    code_win = newwin(rows - 10, cols, 0, 0);
    
    // 2. 创建下半部分的命令窗口（占据下方 10 行）
    tui_win = newwin(10, cols, rows - 10, 0);
    scrollok(tui_win, TRUE);
    keypad(tui_win, TRUE);

    is_tui_mode = true;
    
    // 初始化时刷一次汇编
    refresh_code_window();
}

void deinit_tui() {
    is_tui_mode = false;
    if (code_win) delwin(code_win);
    if (tui_win) delwin(tui_win);
    endwin();
}
int cmd_layout(char *args) {
    init_tui();
    
    char buf[128];
    while (is_tui_mode) {
        // 每次循环开始前，强行刷新一次汇编窗口，确保它是最新的
        refresh_code_window(); 

        wprintw(tui_win, "(nemu-tui) ");
        wrefresh(tui_win);
        
        echo();
        if (wgetnstr(tui_win, buf, sizeof(buf) - 1) == ERR) break;
        noecho();

        if (sdb_execute(buf) < 0) break; 
        
        // 执行完命令（比如 si）后，这里会进入下一轮循环并刷新 refresh_code_window
    }

    deinit_tui();
    return 0;
}
void refresh_code_window() {
    if (!is_tui_mode || code_win == NULL) return;

    werase(code_win);
    box(code_win, 0, 0);
    
    int rows, cols;
    getmaxyx(code_win, rows, cols);
    (void)cols;
    mvwprintw(code_win, 0, 2, "[ Assembly Code ]");

    vaddr_t pc = cpu.pc;
    for (int i = 0; i < rows - 2; i++) {
        vaddr_t cur_pc = pc + i * 4;
        
        // 增加安全检查：确保不越界物理内存
        if (cur_pc < CONFIG_MBASE || cur_pc >= CONFIG_MBASE + CONFIG_MSIZE) break;

        uint32_t inst = vaddr_read(cur_pc, 4);
        char buf[128] = "";

        // 如果 disassemble 崩溃，先用 16 进制代替调试
        // disassemble(buf, sizeof(buf), cur_pc, (uint8_t *)&inst, 4);
        snprintf(buf, sizeof(buf), "0x%08x (asm hidden for debug)", inst);

        if (i == 0) wattron(code_win, A_REVERSE);
        mvwprintw(code_win, i + 1, 2, "0x%08x: %s", cur_pc, buf);
        if (i == 0) wattroff(code_win, A_REVERSE);
    }
    wrefresh(code_win);
}