#ifndef __FTRACE_H__
#define __FTRACE_H__

#include <common.h>

//最多解析128个函数
#define FUNC_NUM 128

typedef struct {
  bool valid;    //有效位，用于排除某些函数
  char name[32]; //函数名
  vaddr_t begin; //函数开始地址，由  Elf32_Addr	st_value;		/* Symbol value */ 给出
  vaddr_t end;  //函数结束地址，由st_value+ Elf32_Wordst_size;		/* Symbol size */给出
} Func;   

extern Func funcs[FUNC_NUM]; //函数数组
extern int nr_func; //解析后的函数个数

int find_func_by_addr(vaddr_t addr);  //根据函数地址获取函数名 [begin,end)
void init_funcs();

#define MAX_FUNC_TRACE 4096

typedef enum { FUNC_CALL, FUNC_RET } FTraceType;  //事件类型

typedef struct {
  FTraceType type;
  vaddr_t pc; 
  int func_id;
} FTraceEntry;
extern FTraceEntry te[MAX_FUNC_TRACE];  //事件数组
extern int nr_func_trace_event;              //事件个数
void ftrace_record(vaddr_t caller_pc,int fid,FTraceType type); //记录调用者的pc地址，函数数组下标，调用类型
void ftrace_print();  //输出

#endif