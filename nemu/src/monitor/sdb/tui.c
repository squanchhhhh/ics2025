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

    werase(code_win); // 清空窗口
    box(code_win, 0, 0); // 画个边框显得专业
    
    vaddr_t pc = cpu.pc;
    char buf[128];
    int rows = getmaxy(code_win);

    // 绘制标题
    mvwprintw(code_win, 0, 2, "[ Assembly Code ]");

    // 连续反汇编接下来的指令（假设每条 4 字节）
    for (int i = 0; i < rows - 2; i++) {
        vaddr_t cur_pc = pc + (i * 4);
        uint32_t inst = vaddr_read(cur_pc, 4);
        
        // 调用 NEMU 原有的 disassemble
        // 这里的 p 传入 buf，最后 4 是指令长度
        extern void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
        disassemble(buf, sizeof(buf), cur_pc, (uint8_t *)&inst, 4);

        if (i == 0) wattron(code_win, A_REVERSE | COLOR_PAIR(2)); // 高亮当前正在执行的行
        
        // 打印地址和反汇编结果
        mvwprintw(code_win, i + 1, 2, "0x%08x: %s", cur_pc, buf);
        
        if (i == 0) wattroff(code_win, A_REVERSE | COLOR_PAIR(2));
    }

    wrefresh(code_win);
}