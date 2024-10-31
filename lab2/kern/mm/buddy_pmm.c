#include <pmm.h>
#include <list.h>
#include <string.h>
#include <buddy_pmm.h>
#include <stdio.h>

#define BUDDY_MAX_DEPTH 30  // 最大二叉树深度
static unsigned int* buddy_page;  // 伙伴系统位图
static unsigned int buddy_page_num; // 伙伴系统使用的页数
static unsigned int max_pages; // 伙伴系统管理的最大页数
static struct Page* buddy_allocatable_base; // 可分配的基础页

#define max(a, b) ((a) > (b) ? (a) : (b))  // 返回最大值的宏定义

// 初始化函数（未实现）
static void buddy_init(void) {}

// 初始化内存映射
static void buddy_init_memmap(struct Page *base, size_t n) {
    int i = 0;
    assert(n > 0);
    
    // 计算伙伴系统最大管理页数
    max_pages = 1;
    for (i = 1; i < BUDDY_MAX_DEPTH; ++i, max_pages <<= 1)
        if (max_pages + (max_pages >> 9) >= n)
            break;
    max_pages >>= 1;
    buddy_page_num = (max_pages >> 9) + 1;
    cprintf("buddy init: total %d, use %d, free %d\n", n, buddy_page_num, max_pages);
    
    // 将使用的页标记为已保留
    for (i = 0; i < buddy_page_num; ++i)
        SetPageReserved(base + i);
    
    // 设置剩余的页为可分配
    buddy_allocatable_base = base + buddy_page_num;
    struct Page* p;
    for (p = buddy_allocatable_base; p != base + n; ++p) {
        ClearPageReserved(p);
        SetPageProperty(p);
        set_page_ref(p, 0);
    }
    
    // 初始化伙伴系统页
    buddy_page = (unsigned int*)KADDR(page2pa(base));
    for (i = max_pages; i < max_pages << 1; ++i)
        buddy_page[i] = 1;
    for (i = max_pages - 1; i > 0; --i)
        buddy_page[i] = buddy_page[i << 1] << 1;
}

// 分配页函数
static struct Page* buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > buddy_page[1]) return NULL;  // 若需求页数超过空闲页数则返回NULL
    
    unsigned int index = 1, size = max_pages;
    
    // 从根节点开始查找满足需求大小的块
    for (; size >= n; size >>= 1) {
        if (buddy_page[index << 1] >= n) index <<= 1;  // 左节点满足需求
        else if (buddy_page[index << 1 | 1] >= n) index = index << 1 | 1;  // 右节点满足需求
        else break;
    }
    buddy_page[index] = 0;  // 分配后将节点标记为空
    
    // 根据分配节点计算起始页
    struct Page* new_page = buddy_allocatable_base + index * size - max_pages;
    struct Page* p;
    for (p = new_page; p != new_page + size; ++p)
        set_page_ref(p, 0), ClearPageProperty(p);  // 清除页属性并重置引用计数
    
    // 更新伙伴系统的父节点，保持最大值
    for (; (index >>= 1) > 0; )
        buddy_page[index] = max(buddy_page[index << 1], buddy_page[index << 1 | 1]);
    return new_page;
}

// 释放页函数
static void buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    unsigned int index = (unsigned int)(base - buddy_allocatable_base) + max_pages, size = 1;
    
    // 找到第一个对应的空节点
    for (; buddy_page[index] > 0; index >>= 1, size <<= 1) {
        // 循环体内容为空
    }
    
    struct Page* p;
    // 将所有页标记为空闲
    for (p = base; p != base + n; ++p) {
        assert(!PageReserved(p) && !PageProperty(p));
        SetPageProperty(p), set_page_ref(p, 0);
    }
    
    // 更新伙伴系统的父节点
    for (buddy_page[index] = size; size <<= 1, (index >>= 1) > 0;)
        buddy_page[index] = (buddy_page[index << 1] + buddy_page[index << 1 | 1] == size) ? size : max(buddy_page[index << 1], buddy_page[index << 1 | 1]);
}

// 返回空闲页数
static size_t buddy_nr_free_pages(void) { return buddy_page[1]; }

// 检查函数
static void buddy_check(void) {
    int all_pages = nr_free_pages();
    struct Page* p0, *p1, *p2, *p3;
    
    // 测试超量分配是否返回NULL
    assert(alloc_pages(all_pages + 1) == NULL);

    // 依次分配测试
    p0 = alloc_pages(1);
    assert(p0 != NULL);
    p1 = alloc_pages(2);
    assert(p1 == p0 + 2);
    assert(!PageReserved(p0) && !PageProperty(p0));
    assert(!PageReserved(p1) && !PageProperty(p1));

    p2 = alloc_pages(1);
    assert(p2 == p0 + 1);
    p3 = alloc_pages(2);
    assert(p3 == p0 + 4);
    assert(!PageProperty(p3) && !PageProperty(p3 + 1) && PageProperty(p3 + 2));

    // 测试释放并检查
    free_pages(p1, 2);
    assert(PageProperty(p1) && PageProperty(p1 + 1));
    assert(p1->ref == 0);

    // 继续释放和分配测试
    free_pages(p0, 1);
    free_pages(p2, 1);

    p2 = alloc_pages(2);
    assert(p2 == p0);
    free_pages(p2, 2);
    assert((*(p2 + 1)).ref == 0);
    assert(nr_free_pages() == all_pages >> 1);

    // 释放剩余页并测试大页分配
    free_pages(p3, 2);
    p1 = alloc_pages(129);
    free_pages(p1, 256);
}

// 定义伙伴系统内存管理器接口
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
