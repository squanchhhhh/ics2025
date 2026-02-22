#ifndef __LIST_H__
#define __LIST_H__

#include <stddef.h>
#include <stdint.h>

/* 链表节点结构 */
typedef struct list_head {
    struct list_head *next, *prev;
} list_head;

/* ------------------ 模块一：初始化 (Initialization) ------------------ */

/**
 * LIST_HEAD_INIT - 静态初始化宏
 * @name: 链表头的变量名
 */
#define LIST_HEAD_INIT(name) { &(name), &(name) }

/**
 * LIST_HEAD - 声明并定义一个已初始化的链表头
 * @name: 链表头的变量名
 */
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

/**
 * INIT_LIST_HEAD - 动态初始化函数
 * @ptr: 指向要初始化的 list_head 结构体
 */
static inline void INIT_LIST_HEAD(list_head *ptr) {
    ptr->next = ptr;
    ptr->prev = ptr;
}

/* ------------------ 模块二：增删操作 (Add & Delete) ------------------ */

/**
 * __list_add - 底层添加接口
 * 仅供内部使用：在两个已知节点之间插入一个新节点
 */
static inline void __list_add(list_head *new, list_head *prev, list_head *next) {
    next->prev = new;
    new->next = next;
    new->prev = prev;
    prev->next = new;
}

/**
 * list_add - 在指定位置之后插入节点（头插法）
 * @new: 新节点
 * @head: 指定位置的节点（通常是链表头）
 */
static inline void list_add(list_head *new, list_head *head) {
    __list_add(new, head, head->next);
}

/**
 * list_add_tail - 在链表末尾插入节点（尾插法）
 * @new: 新节点
 * @head: 链表头
 */
static inline void list_add_tail(list_head *new, list_head *head) {
    __list_add(new, head->prev, head);
}

/**
 * __list_del - 底层删除接口
 * 仅供内部使用：通过修改相邻节点的指向来使中间节点脱离
 */
static inline void __list_del(list_head *prev, list_head *next) {
    next->prev = prev;
    prev->next = next;
}

/**
 * list_del - 从链表中删除一个已知节点
 * @entry: 要删除的节点
 */
static inline void list_del(list_head *entry) {
    __list_del(entry->prev, entry->next);
    // 建议设置为 NULL 避免悬挂指针野访问
    entry->next = NULL;
    entry->prev = NULL;
}

/**
 * list_del_init - 删除节点并将其重新初始化
 * @entry: 要删除并重置的节点
 */
static inline void list_del_init(list_head *entry) {
    __list_del(entry->prev, entry->next);
    INIT_LIST_HEAD(entry);
}

/* ------------------ 模块三：状态检查与移动 (Status & Move) ------------------ */

/**
 * list_empty - 检查链表是否为空
 * @head: 链表头
 */
static inline int list_empty(const list_head *head) {
    return head->next == head;
}

/**
 * list_is_last - 检查节点是否为链表末尾
 * @list: 要检查的节点
 * @head: 链表头
 */
static inline int list_is_last(const list_head *list, const list_head *head) {
    return list->next == head;
}

/**
 * list_move - 将节点从一个链表移动到另一个链表的头部
 * @list: 要移动的节点
 * @head: 目标链表头
 */
static inline void list_move(list_head *list, list_head *head) {
    __list_del(list->prev, list->next);
    list_add(list, head);
}

/**
 * list_move_tail - 将节点从一个链表移动到另一个链表的末尾
 * @list: 要移动的节点
 * @head: 目标链表头
 */
static inline void list_move_tail(list_head *list, list_head *head) {
    __list_del(list->prev, list->next);
    list_add_tail(list, head);
}

/* ------------------ 模块四：定位与遍历 (Entry & Traverse) ------------------ */

/**
 * list_entry - 通过链表节点地址计算宿主结构体的起始地址
 * @ptr: 结构体中 list_head 成员的地址
 */
#define list_entry(ptr, type, member) \
    ((type *)((char *)(ptr) - (uintptr_t)(&((type *)0)->member)))

/**
 * list_first_entry - 获取链表中的第一个宿主结构体
 * @ptr: 链表头
 */
#define list_first_entry(ptr, type, member) \
    list_entry((ptr)->next, type, member)

/**
 * list_for_each - 顺序遍历所有 list_head 节点
 * @pos: 迭代变量
 * @head: 链表头
 */
#define list_for_each(pos, head) \
    for (pos = (head)->next; pos != (head); pos = pos->next)

/**
 * list_for_each_prev - 逆序遍历所有 list_head 节点
 */
#define list_for_each_prev(pos, head) \
    for (pos = (head)->prev; pos != (head); pos = pos->prev)

/**
 * list_for_each_entry - 顺序遍历所有宿主结构体 (最常用)
 * @pos: 宿主结构体指针迭代变量
 * @head: 链表头
 * @member: 结构体中 list_head 的变量名
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_entry((head)->next, typeof(*pos), member);	\
	     &pos->member != (head); 	\
	     pos = list_entry(pos->member.next, typeof(*pos), member))

/**
 * list_for_each_entry_safe - 安全遍历所有宿主结构体 (允许在遍历时删除)
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_entry((head)->next, typeof(*pos), member),	\
		n = list_entry(pos->member.next, typeof(*pos), member);	\
	     &pos->member != (head); 					\
	     pos = n, n = list_entry(n->member.next, typeof(*n), member))

#endif