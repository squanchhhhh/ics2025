#include "trace/itrace.h"
#include "trace/tui.h"
IRing iringbuf;

void push_inst(vaddr_t pc, const char *s) {
  iringbuf.pc[iringbuf.tail] = pc; 
  strncpy(iringbuf.buf[iringbuf.tail], s, LOG_BUF_LEN - 1);
  iringbuf.buf[iringbuf.tail][LOG_BUF_LEN - 1] = '\0';

  iringbuf.tail = (iringbuf.tail + 1) % RING_SIZE;
  if (iringbuf.num < RING_SIZE) iringbuf.num++;
}

// 传入文件名和行号，返回这一行的代码内容
char* get_source_line_content(const char* filename, int line) {
    static char line_buf[256];
    FILE *fp = fopen(filename, "r");
    if (!fp) return "Source not found";

    int cur = 0;
    while (fgets(line_buf, sizeof(line_buf), fp)) {
        cur++;
        if (cur == line) {
            fclose(fp);
            line_buf[strcspn(line_buf, "\r\n")] = 0;
            return line_buf;
        }
    }
    fclose(fp);
    return "Line not found";
}
typedef struct {
    vaddr_t pc;
    char filename[256];
    int line;
} SourceInfo;

void print_recent_insts() {
    int cnt = iringbuf.num;
    SourceInfo src_list[RING_SIZE];

    printf("------- [ Recent Instructions (itrace) ] -------\n");

    printf("asm:\n");
    for (int i = 0; i < cnt; i++) {
        src_list[i].pc = iringbuf.pc[i];
        get_pc_source(src_list[i].pc, src_list[i].filename, &src_list[i].line);

        const char *prefix = (i == iringbuf.error_idx) ? "-->" : "   ";
        printf("%s %-50s (PC: 0x%08x)\n", prefix, iringbuf.buf[i], iringbuf.pc[i]);
    }

    printf("\nsrc:\n");

    for (int i = 0; i < cnt - 1; i++) {
        for (int j = 0; j < cnt - i - 1; j++) {
            if (src_list[j].pc > src_list[j + 1].pc) {
                SourceInfo temp = src_list[j];
                src_list[j] = src_list[j + 1];
                src_list[j + 1] = temp;
            }
        }
    }
    for (int i = 0; i < cnt; i++) {
        if (src_list[i].line != -1) {
            char *content = get_source_line_content(src_list[i].filename, src_list[i].line);
            
            char *short_name = strrchr(src_list[i].filename, '/');
            short_name = (short_name) ? short_name + 1 : src_list[i].filename;

            printf("    [0x%08x] %s:%d | %s\n", 
                   src_list[i].pc, short_name, src_list[i].line, content);
        } else {
            printf("    [0x%08x] unknown source\n", src_list[i].pc);
        }
    }
    printf("------------------------------------------------\n");
}
void init_iring(){
  iringbuf.tail = 0;
  iringbuf.num = 0;
  iringbuf.error_idx = -1;
}
