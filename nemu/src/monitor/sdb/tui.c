/* tui.c */
#include <common.h>
#include "sdb.h" // 为了调用 sdb_execute

bool is_tui_mode = false;
WINDOW *tui_win = NULL;

void init_tui() {
    initscr();
    cbreak();
    noecho();
    // 创建一个位于屏幕下方的命令窗口（例如占 10 行）
    int rows, cols;
    getmaxyx(stdscr, rows, cols);
    tui_win = newwin(10, cols, rows - 10, 0);
    scrollok(tui_win, TRUE);
    keypad(tui_win, TRUE);
    is_tui_mode = true;
}

void deinit_tui() {
    is_tui_mode = false;
    if (tui_win) delwin(tui_win);
    endwin();
}

int cmd_layout(char *args) {
    init_tui();
    
    char buf[128];
    while (is_tui_mode) {
        wprintw(tui_win, "(nemu-tui) ");
        wrefresh(tui_win);
        echo();
        if (wgetnstr(tui_win, buf, sizeof(buf) - 1) == ERR) break;
        noecho();
        if (sdb_execute(buf) < 0) break; 
    }

    deinit_tui();
    return 0;
}