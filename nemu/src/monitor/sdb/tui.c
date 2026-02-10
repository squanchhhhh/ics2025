/* --- tui.c --- */
#include <common.h>
#include <isa.h>
#include "sdb.h"
#include "trace/tui.h"
#include <memory/vaddr.h>
#include "trace/elf.h"
#include <ncurses.h>
#include <stdlib.h>

/* --- 函数原型声明 (Function Prototypes) --- */

// 调用外部工具 addr2line，根据当前的 PC 值获取对应的源码文件名和行号
void get_pc_source(uint32_t pc, char *filename, int *line);

// 将指定的源码文件读取并缓存到内存中，避免频繁的磁盘 IO 操作
void load_source_file(const char *filename);

// 刷新左侧的汇编窗口，显示当前 PC 附近的指令、函数名标签及高亮当前行
void refresh_code_window();

// 刷新右侧的源码窗口，根据 get_pc_source 的结果同步显示对应的代码上下文
void refresh_src_window();

// 初始化 ncurses 环境，配置颜色对，并分配汇编、源码、命令三个子窗口的布局
void init_tui();

// 关闭 ncurses 模式，释放所有窗口句柄并恢复终端的标准显示设置
void deinit_tui();

// SDB 命令处理函数：进入 TUI 交互模式，循环接管输入直到用户退出
int cmd_layout(char *args);

// 引用外部定义：反汇编接口，用于将机器码转为人类可读的汇编字符串
void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);


/* --- 变量定义 --- */

bool is_tui_mode = false;
WINDOW *tui_win = NULL;   // 底部：命令交互窗口
WINDOW *code_win = NULL;  // 左侧：汇编显示窗口
WINDOW *src_win = NULL;   // 右侧：源码显示窗口

static char current_src_file[256] = ""; // 当前缓存的源码文件名
static char *src_lines[4096];           // 源码行缓存数组
static int line_count = 0;              // 当前文件的总行数

// --- 工具函数：获取源码位置 ---
void get_pc_source(uint32_t pc, char *filename, int *line) {
    char cmd[512];
    sprintf(cmd, "riscv64-unknown-elf-addr2line -e %s %08x", elf_file, pc);
    
    FILE *fp = popen(cmd, "r");
    if (fp) {
        char buffer[512];
        if (fgets(buffer, sizeof(buffer), fp)) {
            char *colon = strrchr(buffer, ':');
            if (colon && *(colon + 1) != '?') {
                *colon = '\0';
                strcpy(filename, buffer);
                *line = atoi(colon + 1);
            } else {
                strcpy(filename, "unknown");
                *line = -1;
            }
        }
        pclose(fp);
    }
}
// --- 工具函数：加载源码到内存 ---
void load_source_file(const char *filename) {
    if (strcmp(current_src_file, filename) == 0) return;

    // 释放旧缓存
    for (int i = 0; i < line_count; i++) free(src_lines[i]);
    line_count = 0;

    FILE *fp = fopen(filename, "r");
    if (!fp) {
        strcpy(current_src_file, "File Not Found");
        return;
    }

    char buf[512];
    while (fgets(buf, sizeof(buf), fp) && line_count < 4096) {
        src_lines[line_count++] = strdup(buf);
    }
    fclose(fp);
    strcpy(current_src_file, filename);
}

// --- 窗口刷新：汇编 ---
void refresh_code_window() {
    if (!is_tui_mode || code_win == NULL) return;
    werase(code_win);
    box(code_win, 0, 0);
    int rows, cols;
    getmaxyx(code_win, rows, cols);
    (void)cols;
    mvwprintw(code_win, 0, 2, "[ Assembly ]");

    vaddr_t pc = cpu.pc;
    vaddr_t start_pc = (pc >= 0x8000000c) ? (pc - 12) : 0x80000000;

    int current_row = 1;
    for (int i = 0; current_row < rows - 1; i++) {
        vaddr_t cur_pc = start_pc + (i * 4);
        if (cur_pc < 0x80000000 || cur_pc >= 0x88000000) break;

        // 函数名标注
        int func_id = elf_find_func_by_addr(cur_pc);
        if (func_id != -1) {
            Func *f = elf_get_func_by_id(func_id);
            if (cur_pc == f->begin) {
                wattron(code_win, COLOR_PAIR(4) | A_BOLD);
                mvwprintw(code_win, current_row++, 2, "<%s>:", f->name);
                wattroff(code_win, COLOR_PAIR(4) | A_BOLD);
                if (current_row >= rows - 1) break;
            }
        }

        uint32_t inst = vaddr_read(cur_pc, 4);
        char asm_buf[128];
        void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
        disassemble(asm_buf, sizeof(asm_buf), cur_pc, (uint8_t *)&inst, 4);

        if (cur_pc == pc) wattron(code_win, A_REVERSE | COLOR_PAIR(2));
        mvwprintw(code_win, current_row++, 1, "%s 0x%08x: %s", 
                  (cur_pc == pc ? "->" : "  "), cur_pc, asm_buf);
        if (cur_pc == pc) wattroff(code_win, A_REVERSE | COLOR_PAIR(2));
    }
    wrefresh(code_win);
}

// --- 窗口刷新：源码 ---
void refresh_src_window() {
    if (!is_tui_mode || src_win == NULL) return;
    werase(src_win);
    box(src_win, 0, 0);
    
    char filename[256];
    int line_num = -1;
    get_pc_source(cpu.pc, filename, &line_num);

    if (line_num != -1) {
        load_source_file(filename);
        int rows, cols;
        getmaxyx(src_win, rows, cols);
        mvwprintw(src_win, 0, 2, "[ Source: %s ]", strrchr(filename, '/') ? strrchr(filename, '/') + 1 : filename);

        int start_line = (line_num > rows / 2) ? (line_num - rows / 2) : 1;
        for (int i = 0; i < rows - 2; i++) {
            int cur_l = start_line + i;
            if (cur_l > line_count) break;
            if (cur_l == line_num) wattron(src_win, A_REVERSE | COLOR_PAIR(2));
            mvwprintw(src_win, i + 1, 1, "%4d | %.*s", cur_l, cols - 10, src_lines[cur_l - 1]);
            if (cur_l == line_num) wattroff(src_win, A_REVERSE | COLOR_PAIR(2));
        }
    } else {
        mvwprintw(src_win, 0, 2, "[ Source: Unknown ]");
        mvwprintw(src_win, 2, 2, "No debug info at PC 0x%08x", cpu.pc);
    }
    wrefresh(src_win);
}

void init_tui() {
    initscr();
    start_color();
    init_pair(2, COLOR_GREEN, COLOR_BLACK); // 高亮行
    init_pair(4, COLOR_YELLOW, COLOR_BLACK); // 函数名
    cbreak();
    noecho();

    int rows, cols;
    getmaxyx(stdscr, rows, cols);

    int cmd_h = 10;
    int body_h = rows - cmd_h;
    int asm_w = cols * 0.45;

    code_win = newwin(body_h, asm_w, 0, 0);
    src_win = newwin(body_h, cols - asm_w, 0, asm_w);
    tui_win = newwin(cmd_h, cols, body_h, 0);

    scrollok(tui_win, TRUE);
    keypad(tui_win, TRUE);
    is_tui_mode = true;
}

void deinit_tui() {
    is_tui_mode = false;
    if (code_win) delwin(code_win);
    if (src_win) delwin(src_win);
    if (tui_win) delwin(tui_win);
    endwin();
}

int cmd_layout(char *args) {
    init_tui();
    char buf[128];
    while (is_tui_mode) {
        refresh_code_window();
        refresh_src_window();

        wattron(tui_win, A_BOLD | COLOR_PAIR(2));
        int cur_y, cur_x;
        getyx(tui_win, cur_y, cur_x); (void)cur_x; // 获取当前行
        mvwprintw(tui_win, cur_y, 0, "(nemu-tui) ");
        wattroff(tui_win, A_BOLD | COLOR_PAIR(2));
        wrefresh(tui_win);
        
        echo();
        if (wgetnstr(tui_win, buf, sizeof(buf) - 1) == ERR) break;
        noecho();

        if (strcmp(buf, "q") == 0) { is_tui_mode = false; break; }
        sdb_execute(buf); 
    }
    deinit_tui();
    return 0;
}