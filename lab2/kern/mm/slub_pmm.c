#include <defs.h>
#include <list.h>
#include <memlayout.h>
#include <assert.h>
#include <slub_pmm.h>
#include <pmm.h>
#include <stdio.h>

// 定义小块内存块结构体
struct mem_block {
    int size_units;  // 块的大小（单位为BLOCK_UNIT）
    struct mem_block *next;  // 指向下一个块的指针
};
typedef struct mem_block mem_block_t;

#define BLOCK_UNIT sizeof(mem_block_t)  // 块的基本单位大小
#define BLOCK_UNITS(size) (((size) + BLOCK_UNIT - 1) / BLOCK_UNIT)  // 计算需要的块数量

// 定义大块内存块结构体
struct large_block {
    int order;  // 页数的次方（比如，order = 2表示4页）
    void *pages;  // 指向大块内存的指针
    struct large_block *next;  // 指向下一个大块的指针
};
typedef struct large_block large_block_t;

// 初始化基本的内存块和空闲列表
static mem_block_t base_arena = { .next = &base_arena, .size_units = 1 };
static mem_block_t *free_list = &base_arena;
static large_block_t *large_blocks = NULL;

// 释放小块内存的静态函数声明
static void free_small_block(void *block, int size);

// 分配小块内存
static void *allocate_small_block(size_t size) {
    assert(size < PGSIZE);  // 确保小块内存小于页面大小

    mem_block_t *prev = free_list;
    mem_block_t *current;
    int required_units = BLOCK_UNITS(size);  // 计算所需块的数量

    // 遍历空闲列表查找足够大的块
    for (current = prev->next; ; prev = current, current = current->next) {
        if (current->size_units >= required_units) {  // 找到合适大小的块
            if (current->size_units == required_units) {  // 如果正好符合大小
                prev->next = current->next;
            } else {  // 如果当前块比需求大，分割块
                mem_block_t *new_block = (mem_block_t *)((char *)current + required_units * BLOCK_UNIT);
                new_block->size_units = current->size_units - required_units;
                new_block->next = current->next;
                prev->next = new_block;
                current->size_units = required_units;
            }
            free_list = prev;  // 更新空闲列表头部
            return current;
        }
        if (current == free_list) {  // 如果遍历完成仍未找到
            current = (mem_block_t *)alloc_pages(1);  // 分配新页面
            if (!current) {
                return NULL;
            }
            free_small_block(current, PGSIZE);  // 将新页面添加到空闲列表
            current = free_list;
        }
    }
}

// 释放小块内存
static void free_small_block(void *block, int size) {
    if (!block) return;

    mem_block_t *to_free = (mem_block_t *)block;
    if (size) {
        to_free->size_units = BLOCK_UNITS(size);  // 设置释放块的大小
    }

    mem_block_t *current;
    // 查找插入位置，并确保按地址顺序排列
    for (current = free_list; !(to_free > current && to_free < current->next); current = current->next) {
        if (current >= current->next && (to_free > current || to_free < current->next)) {
            break;
        }
    }

    // 检查是否可以合并相邻的块
    if ((mem_block_t *)((char *)to_free + to_free->size_units * BLOCK_UNIT) == current->next) {
        to_free->size_units += current->next->size_units;
        to_free->next = current->next->next;
    } else {
        to_free->next = current->next;
    }

    if ((mem_block_t *)((char *)current + current->size_units * BLOCK_UNIT) == to_free) {
        current->size_units += to_free->size_units;
        current->next = to_free->next;
    } else {
        current->next = to_free;
    }

    free_list = current;  // 更新空闲列表头部
}

// 初始化SLUB分配器
void slub_initialize(void) {
    cprintf("SLUB allocator initialized successfully.\n");
}

// 分配内存
void *slub_allocate(size_t size) {
    mem_block_t *small_block;
    large_block_t *large_block;

    if (size < PGSIZE - BLOCK_UNIT) {  // 如果请求小于一页，则分配小块
        small_block = allocate_small_block(size + BLOCK_UNIT);
        return small_block ? (void *)(small_block + 1) : NULL;
    }

    large_block = allocate_small_block(sizeof(large_block_t));  // 分配大块描述符
    if (!large_block) return NULL;

    large_block->order = ((size - 1) >> PGSHIFT) + 1;  // 计算页数
    large_block->pages = (void *)alloc_pages(large_block->order);  // 分配多页内存

    if (large_block->pages) {
        large_block->next = large_blocks;
        large_blocks = large_block;  // 将大块链接到大块列表
        return large_block->pages;
    }

    free_small_block(large_block, sizeof(large_block_t));  // 失败时释放大块描述符
    return NULL;
}

// 释放内存
void slub_release(void *block) {
    if (!block) return;

    if (!((uintptr_t)block & (PGSIZE - 1))) {  // 检查是否为大块
        large_block_t *current_block = large_blocks;
        large_block_t **previous = &large_blocks;

        while (current_block) {  // 遍历大块列表
            if (current_block->pages == block) {
                *previous = current_block->next;
                free_pages((struct Page *)block, current_block->order);  // 释放页
                free_small_block(current_block, sizeof(large_block_t));  // 释放描述符
                return;
            }
            previous = &current_block->next;
            current_block = current_block->next;
        }
    }

    free_small_block((mem_block_t *)block - 1, 0);  // 释放小块
}

// 获取块大小
unsigned int slub_block_size(const void *block) {
    if (!block) return 0;

    if (!((uintptr_t)block & (PGSIZE - 1))) {  // 判断大块
        large_block_t *current_block;
        for (current_block = large_blocks; current_block; current_block = current_block->next) {
            if (current_block->pages == block) {
                return current_block->order << PGSHIFT;
            }
        }
    }

    return ((mem_block_t *)block - 1)->size_units * BLOCK_UNIT;  // 返回小块大小
}

// 计算空闲块数
int count_free_blocks() {
    int count = 0;
    mem_block_t *current = free_list->next;
    while (current != free_list) {
        count++;
        current = current->next;
    }
    return count;
}

// SLUB调试输出
void slub_debug() {
    cprintf("SLUB allocator debug start\n");
    cprintf("Number of free blocks: %d\n", count_free_blocks());

    void *p1 = slub_allocate(4096);
    cprintf("Number of free blocks after p1 allocation: %d\n", count_free_blocks());

    void *p2 = slub_allocate(2);
    void *p3 = slub_allocate(2);
    cprintf("Number of free blocks after p2 and p3 allocation: %d\n", count_free_blocks());

    slub_release(p2);
    cprintf("Number of free blocks after p2 deallocation: %d\n", count_free_blocks());

    slub_release(p3);
    cprintf("Number of free blocks after p3 deallocation: %d\n", count_free_blocks());

    cprintf("SLUB allocator debug end\n");
}
