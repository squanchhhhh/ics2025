#ifndef __LIST_H__
#define __LIST_H__

typedef struct list_head {
    struct list_head *next, *prev;
} list_head;

#define LIST_HEAD_INIT(name) { &(name), &(name) }

static inline void list_add_tail(list_head *new, list_head *head) {
    new->next = head;
    new->prev = head->prev;
    head->prev->next = new;
    head->prev = new;
}

static inline void list_del(list_head *entry) {
    entry->next->prev = entry->prev;
    entry->prev->next = entry->next;
}

// 核心宏：通过成员地址反推结构体首地址（对应 Linux 的 container_of）
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (unsigned long)(&((type *)0)->member)))

#endif