#ifndef __ERROR_H__
#define __ERROR_H__
#include <common.h>
#include "itrace.h"
#include "ftrace.h"
#include "mtrace.h"

typedef struct{
    char fe[128];
    ITraceEntry ie;
    MTraceEntry me;
}ErrorEntry;
void get_error_mtrace(ErrorEntry *e);
void get_error_ftrace(ErrorEntry *e);
void get_error_itrace(ErrorEntry *e, int n);
void fill_unified_log();
void dump_unified_error();
#endif // __ERROR_H__