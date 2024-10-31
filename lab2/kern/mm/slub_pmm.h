#ifndef __KERN_MM_SLUB_PMM_H__
#define __KERN_MM_SLUB_PMM_H__

#include <defs.h> // 通用类型定义和宏

/**
 * @brief 初始化 SLUB 内存分配器。
 * 
 * 在使用分配器进行内存操作前需要调用该函数进行初始化。
 */
void slub_initialize(void);

/**
 * @brief 分配指定大小的内存块。
 * 
 * 如果请求小于页大小的内存，则分配小块；如果请求超过页大小，则分配大块。
 * 
 * @param size 要分配的内存大小，以字节为单位。
 * @return void* 成功时返回指向分配内存的指针；若分配失败则返回 NULL。
 */
void *slub_allocate(size_t size);

/**
 * @brief 释放先前通过 slub_allocate 分配的内存块。
 * 
 * @param block 指向待释放内存块的指针。必须为先前通过 slub_allocate 分配的指针。
 */
void slub_release(void *block);

/**
 * @brief 获取指定内存块的大小。
 * 
 * 用于查询已分配的内存块的实际大小。
 * 
 * @param block 指向要查询的内存块。
 * @return unsigned int 内存块的大小，以字节为单位；若内存块无效则返回 0。
 */
unsigned int slub_block_size(const void *block);

/**
 * @brief 统计当前空闲链表中的节点数量。
 * 
 * @return int 返回空闲链表中的节点数。
 */
int count_free_blocks(void);

/**
 * @brief 检查 SLUB 分配器的状态并输出调试信息。
 * 
 * 用于调试和查看当前内存块的分配和空闲情况。
 */
void slub_debug(void);

#endif /* !__KERN_MM_SLUB_PMM_H__ */