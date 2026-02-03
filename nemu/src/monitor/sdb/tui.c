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
#include <trace/elf.h> 

void refresh_code_window() {
    if (!is_tui_mode || code_win == NULL) return;

    werase(code_win);
    box(code_win, 0, 0);
    
    int rows, cols;
    getmaxyx(code_win, rows, cols);
    (void)cols;
    mvwprintw(code_win, 0, 2, "[ Assembly Code ]");

    vaddr_t pc = cpu.pc;
    // 计算显示起始位置：让当前 PC 尽量处于窗口上方第 3 行 (索引 2)
    // 这样可以看到前两条指令作为上下文
    vaddr_t start_pc = (pc >= 0x80000008) ? (pc - 8) : 0x80000000;

    int current_row = 1; // 从第 1 行开始绘制（第 0 行是边框）
    
    for (int i = 0; current_row < rows - 1; i++) {
        vaddr_t cur_pc = start_pc + (i * 4);
        
        // 1. 物理内存边界检查 (RISC-V 默认内存范围)
        if (cur_pc < 0x80000000 || cur_pc >= 0x88000000) break;

        // 2. 函数标签检测：如果该地址是一个函数的开头，打印一行函数名
        int func_id = elf_find_func_by_addr(cur_pc);
        if (func_id != -1) {
            Func *f = elf_get_func_by_id(func_id);
            if (cur_pc == f->begin) {
                wattron(code_win, COLOR_PAIR(4) | A_BOLD); // 假设 4 是黄色
                mvwprintw(code_win, current_row++, 2, "<%s>:", f->name);
                wattroff(code_win, COLOR_PAIR(4) | A_BOLD);
                
                // 如果打印完函数名已经没位置了，就跳出
                if (current_row >= rows - 1) break;
            }
        }

        // 3. 读取指令并进行反汇编
        uint32_t inst = vaddr_read(cur_pc, 4);
        char asm_buf[128];

        /* * 逻辑过滤：
         * 1. 如果地址不在任何已知函数范围内 -> 视为数据
         * 2. 如果指令低两位不是 0x3 -> 视为非标准 RV32 指令或对齐
         * 3. 常见的魔数填充
         */
        if (func_id == -1 || (inst & 0x3) != 0x3 || inst == 0 || inst == 0xdeadbe00) {
            snprintf(asm_buf, sizeof(asm_buf), ".word  0x%08x", inst);
        } else {
            void disassemble(char *str, int size, uint64_t pc, uint8_t *code, int nbyte);
            disassemble(asm_buf, sizeof(asm_buf), cur_pc, (uint8_t *)&inst, 4);
        }

        // 4. 绘制指令行
        bool is_current = (cur_pc == pc);
        if (is_current) {
            wattron(code_win, A_REVERSE | COLOR_PAIR(2)); // 高亮当前行
        }

        // 格式说明: [标志] [地址] [十六进制] [汇编助记符]
        mvwprintw(code_win, current_row++, 1, "%s 0x%08x:  %08x  %s", 
                  (is_current ? "-->" : "   "), 
                  cur_pc, inst, asm_buf);

        if (is_current) {
            wattroff(code_win, A_REVERSE | COLOR_PAIR(2));
        }
    }

    wrefresh(code_win);
}