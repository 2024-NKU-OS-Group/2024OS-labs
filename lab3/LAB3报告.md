

练习1：理解基于FIFO的⻚⾯替换算法（思考题）

 描述FIFO⻚⾯置换算法下，⼀个⻚⾯从被换⼊到被换出的过程中，会经过代码⾥哪些函数/宏的处理 （或者说，需要调⽤哪些函数/宏），并⽤简单的⼀两句话描述每个函数在过程中做了什么？（为了⽅ 便同学们完成练习，所以实际上我们的项⽬代码和实验指导的还是略有不同，例如我们将FIFO⻚⾯置 换算法头⽂件的⼤部分代码放在了 kern/mm/swap_fifo.c  ⽂件中，这点请同学们注意）

​		 • ⾄少正确指出10个不同的函数分别做了什么？如果少于10个将酌情给分。我们认为只要函数原型不 同，就算两个不同的函数。要求指出对执⾏过程有实际影响,删去后会导致输出结果不同的函数（例 如assert）⽽不是cprintf这样的函数。如果你选择的函数不能完整地体现”从换⼊到换出“的过 程，⽐如10个函数都是⻚⾯换⼊的时候调⽤的，或者解释功能的时候只解释了这10个函数在⻚⾯换 ⼊时的功能，那么也会扣除⼀定的分数

⻚⾯换⼊算法

我们⾸先对⻚⾯换⼊算法进⾏总体概述：

流程概述：

1. 缺⻚异常触发与处理当程序访问不存在的虚拟地址（即缺⻚异常）， 数会被触发，它调⽤ exception_handler  函 pgfault_handler  来处理这个缺⻚异常。 

2.   调⽤ pgfault_handler pgfault_handler  ⾸先会打印缺⻚信息，然后将内存管理结构 mm_struct  、错误码、缺⻚地址等信息传递给 do_pgfault  ，它负责具体的缺⻚处理，包括⻚ ⾯置换。 

3.  查找虚拟内存区域在 do_pgfault  中，调⽤ （VMA）。 find_vma  会⾸先检查 >mmap_list  来查找。 

4.  获取⻚表项与处理找到相应的VMA后， find_vma  来查找包含异常地址的虚拟内存区域 mm->mmap_cache  ，如果缓存中找不到，则会遍历 do_pgfault  会调⽤ mm get_pte  来获取该地址对应的⻚表 项（PTE）。如果该⻚表项不存在，意味着需要分配物理⻚并创建⻚表项；如果存在，则可能是⼀ 个交换条⽬，需通过 swap_in  函数从磁盘加载⻚⾯。 

5. 获取与插⼊物理⻚⾯如果需要创建新的⻚表项或将磁盘数据换⼊内存， 分配物理⻚⾯，并通过 pgdir_alloc_page  会 page_insert  将物理⻚⾯与虚拟地址建⽴映射关系。 过修改⻚表项将新的物理⻚⾯映射到虚拟地址，并调⽤ page_insert  通 tlb_invalidate  来刷新TLB （Translation Lookaside Buffer），确保TLB中的旧映射被清除。 

6.  磁盘交换与⻚⾯换⼊如果缺⻚需要从磁盘加载数据， swap_in  将调⽤ swapfs_read  从交换空 间读取数据到内存。这是⼀个从虚拟磁盘（在内核空间的静态存储区模拟的磁盘）到物理内存的过 程。 

7.  FIFO算法与⻚⾯替换在处理完⻚⾯置换后，⻚⾯的管理会通过FIFO算法进⾏优化。这是通过 _fifo_map_swappable  来管理访问过的⻚⾯，插⼊⻚⾯访问队列中，确保换出的⻚⾯是最久 未访问的⻚⾯。 

8. 磁盘读取 swapfs_read  通过调⽤ ide_read_secs  从磁盘读取数据。该函数通过 memcpy  将 磁盘中的数据复制到内存⻚⾯，这种磁盘I/O操作实际上是模拟的内存间复制，因为磁盘在该操作 中仅作为内存的⼀部分使⽤。 

   各个函数的功能： 

   • 这⾥对上述各个函数的作⽤进⾏逐点总结： 

   1. exception_handler-该函数处理异常，特别是缺⻚异常，并调⽤`pgfault_handler`进⾏缺⻚ 处理。
   2.   pgfault_handler-处理缺⻚异常，打印相关信息，并将内存管理结构、错误码和地址等传给 `do_pgfault` 进⾏后续处理。 
   3. do_pgfault-处理缺⻚错误：通过`find_vma`查找地址对应的虚拟内存区域，并进⾏⻚表的更 新。如果⻚表项不存在，则分配新的物理⻚并映射；如果⻚表项存在，则进⾏⻚⾯交换操作。
   4.  find_vma-查找内存管理结构`mm`中包含指定地址`addr`的虚拟内存区域（`vma`），⽀ 持缓存优化和链表遍历。
   5. PTE_U-⻚表项标志，表⽰该⻚可由⽤⼾程序访问。 
   6. VM_WRITE-`vma`的标志，表⽰该虚拟内存区域是可写的。 
   7. PTE_R-⻚表项标志，表⽰该⻚是可读的。 
   8.  PTE_W-⻚表项标志，表⽰该⻚是可写的。 
   9.  get_pte-获取给定⻚⽬录`pgdir`中与线性地址`la`对应的⻚表项，如果⻚表项不存在，可以 创建新的⻚表。 
   10.  pgdir_alloc_page-为⻚⽬录`pgdir`中的线性地址`la`分配物理⻚⾯并设置权限，返回物理 ⻚。 11
   11. swap_in-处理⻚⾯交换⼊操作，分配物理⻚并从磁盘读取数据到该⻚中。 
   12. page_insert-将物理⻚⾯`page`与线性地址`la`进⾏映射，并更新⻚表项，最后刷新TLB 
   13. tlb_invalidate-刷新TLB，确保⻚表更改后，TLB中的条⽬被清除。
   14.  flush_tlb-执⾏TLB刷新操作，使⽤`sfence.vma`指令⽆效化TLB。
   15.  swap_map_swappable-设置⻚⾯`page`为可交换状态，通过交换管理器`sm`的 `map_swappable`接⼝进⾏管理。 
   16.  _fifo_map_swappable-使⽤FIFO算法管理⻚⾯交换，插⼊⻚⾯到访问队列的末尾。 
   17.  swapfs_read-从交换空间读取数据，将磁盘数据读取到内存中的⻚⾯。 
   18.  ide_read_secs-从磁盘读取指定扇区的数据，并通过`memcpy`将其复制到⽬标内存中。实 际实现磁盘I/O操作。

详细流程 

⾸先追踪⼀下发⽣缺⻚异常ucore是怎么进⾏处理的： 

1.在kern/init/init.c  中可以观察到，相对于前⾯的实验，lab3多了 swap_init()  函数的调⽤，该函数⽤于初始化⻚⾯替换算法。

2. 在完成lab1时我们知道当发⽣中断或者异常时，会根据其类型分发给不同的handler进⾏处理， 在 kern/trap/trap.c  中可以找到发⽣异常时会调⽤ 发⽣缺⻚异常时会继续调⽤ 中会再调⽤ exception_handler()  函数，其中 pgfault_handler()  函数进⾏处理。 pgfault_handler  函数 do_pgfault  函数，由此才真正开始进⾏缺⻚异常的处理，也就是⻚⾯的换⼊换出。 

3. 下⾯对 do_pgfault  函数所做的⼯作以及其中调⽤的相关函数进⾏⼤致说明。 

   ​	3.1 在函数开始时调⽤了 find_vma(mm, addr)  ，其中mm是内存管理的结构体，addr保存的 是发⽣缺⻚异常的虚拟地址。 find_vma()  函数通过遍历链表，在虚拟内存管理结构体中找到包 含addr的虚拟内存块，从⽽⽤于建⽴从虚拟内存到物理内存的映射。

   ​	 3.2 设置标志位后调⽤了 get_pte()  函数到mm->pgdir（指向⻚⽬录的指针）获取对应的⻚表 项，不存在则会创建⼀个新的⻚表项并返回。 

   ​	3.3 预备⼯作做好后，接下来的else-if(swap_init-ok)中便实现了⻚⾯替换的⼯作。⾸先调⽤ swap_in()  函数进⾏⻚的换⼊。在 swap_in()  函数中⾸先调⽤了 访问的虚拟地址分配⼀个物理⻚，然后调⽤ alloc_page()  函数来为 get_pte()  函数获取对应的⻚表项，最后调⽤ swapfs_read()  函数将物理地址的数据写⼊⻚表项。 的 i swapfs_read()  函数调⽤了 ide.c  中 de_read_secs()  函数，传⼊的参数分别是挂载磁盘数、读写⼤⼩、虚拟地址、所需扇区 数，由于ucore是⽤⼀块内存模拟磁盘，所以函数实现仅仅是使⽤了 换出时会调⽤ swapfs_write()  函数，原理与 memcpy()  将数据拷⻉。在⻚ swapfs_read()  基本⼀致，后⽂不再赘述。⾸ 先追踪⼀下发⽣缺⻚异常ucore是怎么进⾏处理的： 

   ​		1、在`kern/init/init.c`中可以观察到，相对于前⾯的实验，lab3多了`swap_init()`函数的调⽤， 该函数⽤于初始化⻚⾯替换算法。

   ​		 2、在完成lab1时我们知道当发⽣中断或者异常时，会根据其类型分发给不同的handler进⾏处 理，在`kern/trap/trap.c`中可以找到发⽣异常时会调⽤`exception_handler()`函数，其中发⽣缺 ⻚异常时会继续调⽤`pgfault_handler()`函数进⾏处理。`pgfault_handler`函数中会再调⽤ `do_pgfault`函数，由此才真正开始进⾏缺⻚异常的处理，也就是⻚⾯的换⼊换出。

   ​		 3、下⾯对`do_pgfault`函数所做的⼯作以及其中调⽤的相关函数进⾏⼤致说明。

   ​		 > 3.1 在函数开始时调⽤了`find_vma(mm,addr)`，其中mm是内存管理的结构体，addr保存的是 发⽣缺⻚异常的虚拟地址。`find_vma()`函数通过遍历链表，在虚拟内存管理结构体中找到包含 addr 的虚拟内存块，从⽽⽤于建⽴从虚拟内存到物理内存的映射。 

   ​		> 3.2 设置标志位后调⽤了`get_pte()`函数到mm->pgdir（指向⻚⽬录的指针）获取对应的⻚表 项，不存在则会创建⼀个新的⻚表项并返回。 

   ​		> 3.3 预备⼯作做好后，接下来的else-if(swap_init-ok)中便实现了⻚⾯替换的⼯作。⾸先调⽤ `swap_in()`函数进⾏⻚的换⼊。在`swap_in()`函数中⾸先调⽤了`alloc_page()`函数来为访问的 虚拟地址分配⼀个物理⻚，然后调⽤`get_pte()`函数获取对应的⻚表项，最后调⽤ `swapfs_read()`函数将物理地址的数据写⼊⻚表项。`swapfs_read()`函数调⽤了`ide.c`中的 `ide_read_secs()`函数，传⼊的参数分别是挂载磁盘数、读写⼤⼩、虚拟地址、所需扇区数，由于 ucore 是⽤⼀块内存模拟磁盘，所以函数实现仅仅是使⽤了`memcpy()`将数据拷⻉。在⻚换出时会 调⽤`swapfs_write()`函数，原理与`swapfs_read()`基本⼀致，后⽂不再赘述。

⾸先追踪⼀下发⽣缺⻚异常ucore是怎么进⾏处理的：

 1、在 kern/init/init.c  中可以观察到，相对于前⾯的实验，lab3多了 swap_init()  函数的 调⽤，该函数⽤于初始化⻚⾯替换算法。 

2、在完成lab1时我们知道当发⽣中断或者异常时，会根据其类型分发给不同的handler进⾏处理， 在 k ern/trap/trap.c  中可以找到发⽣异常时会调⽤ 缺⻚异常时会继续调⽤ ⽤ d exception_handler()  函数，其中发⽣ pgfault_handler()  函数进⾏处理。 pgfault_handler  函数中会再调 o_pgfault  函数，由此才真正开始进⾏缺⻚异常的处理，也就是⻚⾯的换⼊换出。 

3、下⾯对 do_pgfault  函数所做的⼯作以及其中调⽤的相关函数进⾏⼤致说明。

​		3.1 在函数开始时调⽤了 find_vma(mm, addr)  ，其中mm是内存管理的结构体，addr保存的 是发⽣缺⻚异常的虚拟地址。 find_vma()  函数通过遍历链表，在虚拟内存管理结构体中找到包 含addr的虚拟内存块，从⽽⽤于建⽴从虚拟内存到物理内存的映射。

​		 3.2 设置标志位后调⽤了 get_pte()  函数到mm->pgdir（指向⻚⽬录的指针）获取对应的⻚表 项，不存在则会创建⼀个新的⻚表项并返回。

​		 3.3 预备⼯作做好后，接下来的else-if(swap_init-ok)中便实现了⻚⾯替换的⼯作。⾸先调⽤ swap_in()  函数进⾏⻚的换⼊。在 swap_in()  函数中⾸先调⽤了 访问的虚拟地址分配⼀个物理⻚，然后调⽤ alloc_page()  函数来为 get_pte()  函数获取对应的⻚表项，最后调⽤ swapfs_read()  函数将物理地址的数据写⼊⻚表项。 的 i swapfs_read()  函数调⽤了 ide.c  中 de_read_secs()  函数，传⼊的参数分别是挂载磁盘数、读写⼤⼩、虚拟地址、所需扇区 数，由于ucore是⽤⼀块内存模拟磁盘，所以函数实现仅仅是使⽤了 换出时会调⽤ swapfs_write()  函数，原理与 memcpy()  将数据拷⻉。在⻚ swapfs_read()  基本⼀致，后⽂不再赘述。

```c
 int swap_in(struct mm_struct *mm, uintptr_t addr, struct Page **ptr_result) { 
     struct Page *result = alloc_page();// 
     assert(result!=NULL);
     pte_t *ptep = get_pte(mm->pgdir, addr, 0); 
     cprintf("SWAP: load ptep %x swap entry %d to vaddr 0x%08x, page %x, No  %d\n", ptep, (*ptep)>>8, addr, result, (result-pages)); 
     int r;
     if ((r = swapfs_read((*ptep), result)) != 0) 
     {
         assert(r!=0); 
     } 
     cprintf("swap_in: load disk swap entry %d with swap_page in vadr 0x%x\n",  (*ptep)>>8, addr);
     *ptr_result=result; 
     return 0; 
 }
```

​		3.4 ⻚换⼊之后调⽤ page_insert()  函数来建⽴从物理⻚到⻚表项的映射。实现中⾸先调⽤ get_pte()  来获取⻚表项，随后调⽤ ⽤ p page_ref_inc()  增加映射数，若映射已经存在则会调 age_ref_dec()  来修正增加的ref，若映射原本对应的⻚不⽤，则会调⽤ page_remove_pte  删除原本的映射。最后调⽤ pte_create()  函数来创建⻚表项。

```c
 int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm) {
 pte_t *ptep = get_pte(pgdir, la, 1);
 if (ptep == NULL) {
 return -E_NO_MEM;
 }
 page_ref_inc(page);
 if (*ptep & PTE_V) {
 struct Page *p = pte2page(*ptep);
 if (p == page) {
 page_ref_dec(page);
 } else {
 page_remove_pte(pgdir, la, ptep);
 }
 }
 *ptep = pte_create(page2ppn(page), PTE_V | perm);
 tlb_invalidate(pgdir, la);
 return 0;
 }
```

​		 3.5 随后调⽤ swap_map_swappable()  函数，⽬的是设置⼀个⻚可交换。具体实现就是将换⼊的 ⻚放到队列（基于链表实现）的最前。

```c
static int _fifo_map_swappable(struct mm_struct *mm, uintptr_t addr, struct 
Page *page, int swap_in)
 {
 list_entry_t *head=(list_entry_t*) mm->sm_priv;
 list_entry_t *entry=&(page->pra_page_link);
 assert(entry != NULL && head != NULL);
 list_add(head, entry);
 return 0;
}
```

⻚⾯换出算法

 ⻚⾯换出的过程是操作系统内存管理中的⼀种常⻅机制，通常发⽣在内存不⾜时，⽤来将不常⽤的⻚ ⾯换出到硬盘上的交换区，从⽽释放内存空间。

 流程概述 

1. 尝试分配内存⻚⾯：通过 alloc_pages  尝试分配连续的物理⻚⾯。如果成功分配则直接返回； 如果分配失败，检查是否可以通过换出其他⻚⾯来释放内存。
2.   选择待换出⻚⾯：如果内存不⾜且不能分配所需⻚⾯，调⽤ swap_out  函数来选择⼀个可以被换 出的⻚⾯。通常会通过某种⻚⾯换出算法来选择，例如FIFO算法。 
3.  换出⻚⾯到硬盘：选择⼀个⻚⾯后，调⽤ swapfs_write  将⻚⾯内容写⼊硬盘的交换区，同时 释放该⻚⾯并更新相关数据结构。

主要函数与宏说明 

​	alloc_pages：⽤于分配内存⻚⾯，如果⽆法分配连续的⻚⾯或没有⾜够的物理⻚⾯时，会尝试换 出其他⻚⾯。 

​	swap_out：⽤于将选中的内存⻚⾯换出到磁盘上。通过调⽤ 待换出的⻚⾯，并确保该⻚⾯的⻚表项有效（即 态。sm->swap_out_victim  来选择 PTE_V  标志位为1）。换出后，更新内存管理状

 _fifo_swap_out_victim：FIFO ⻚⾯换出算法实现，选择最早访问的⻚⾯进⾏换出。通过链表管 理⻚⾯访问顺序，头部的⻚⾯最早被访问，因此最优先被换出。 

PTE_V：⻚表项的标志位之⼀，表⽰该⻚表项有效。当⼀个⻚⾯被换出时，需要确保⻚表项的有效 位是1，表⽰该⻚⾯当前在内存中。

 swapfs_write：负责将⻚⾯内容写⼊硬盘的交换区。它调⽤ ide_write_secs  将⻚⾯数据写 ⼊指定的磁盘扇区。 

ide_write_secs：实际执⾏磁盘写⼊操作的函数，通过 memcpy  将内存中的⻚⾯数据写⼊指定的 磁盘扇区。 过程概述

1. alloc_pages  会尝试分配所需的内存。如果成功分配，直接返回。 
2. 如果 alloc_pages  失败，它会调⽤ swap_out  以换出⻚⾯。 法（如FIFO）通过 swap_out  会使⽤⻚⾯换出算 sm->swap_out_victim  选择待换出的⻚⾯。 
3. 被换出的⻚⾯会被写⼊硬盘的交换区，写⼊操作通过 swapfs_write  完成。实际的磁盘写⼊由 ide_write_secs  实现。写⼊后，释放该⻚⾯并更新相关的内存管理状态。 详细流程 swap_out()  函数⽤于⻚的换出。⾸先通过 swap_out_victim()  来获取可以换出的⻚，即找 到FIFO的队⾸的前⼀个节点（也就是队尾节点），将其对应的⻚存⼊ptr_page随后将其删除。接 着调⽤ swapfs_write()  函数进⾏数据的转移。

详细流程 

swap_out()  函数⽤于⻚的换出。⾸先通过 swap_out_victim()  来获取可以换出的⻚，即找 到FIFO的队⾸的前⼀个节点（也就是队尾节点），将其对应的⻚存⼊ptr_page随后将其删除。接 着调⽤ swapfs_write()  函数进⾏数据的转移。

```c
 static int
 _fifo_swap_out_victim(struct mm_struct *mm, struct Page ** ptr_page, int 
in_tick)
 {

 list_entry_t *head=(list_entry_t*) mm->sm_priv;
 assert(head != NULL);
 assert(in_tick==0);
 /* Select the victim */
 //(1)  unlink the  earliest arrival page in front of pra_list_head qeueue
 //(2)  set the addr of addr of this page to ptr_page
 list_entry_t* entry = list_prev(head);
 if (entry != head) {
 list_del(entry);
 *ptr_page = le2page(entry, pra_page_link);
 } else {
 *ptr_page = NULL;
 }
 return 0;
 }
```

#### 练习2：深入理解不同分页模式的工作原理

`get_pte` 函数**获取pte并返回该pte的内核虚拟地址**作为需要映射的线性地址，并且如果页表中不包含这个pte，需要给页表分配一个新页。在分析函数功能之前，先了解一下RISC-V中的不同分页模式：

##### 2.1 RISC-V 的分页模式及工作原理

[RISC-V特权文档](https://rcore-os.cn/rCore-Tutorial-Book-v3/chapter4/3sv39-implementation-1.html)中定义了几种分页机制：sv32，sv39和sv48(还有sv57)。其中，sv32用于32位系统，sv39和sv48用于64位系统，后面跟着的数字表示**虚拟地址长度**的位数：

- sv32的地址长度为32位：由12位的页内偏移和两个**10位**的页号组成，分别对应一级和二级页表
- sv39的地址长度为39位，sv48的地址长度为39位，由12位页偏移和三(四)个**9位**页号组成，分别对应一级、二级、三级(四级页表)

对于我们所使用的**sv39**模式，其虚拟内存地址格式如下：![32](C:\Users\86186\Downloads\32.jpg)



三种分页模式可寻址的**物理内存**也不同，sv32的物理地址空间为34位，可以寻址16GB的内存；Sv39的物理地址空间为56位，可以寻址256TB的内存；Sv48的物理地址空间为64位，可以寻址16EB的内存。

对于我们所使用的**sv39**模式，其物理内存地址格式如下：

![33](C:\Users\86186\Downloads\33.jpg)

这三种分页模式的寻址过程都利用 `satp` 寄存器，其位数和系统的位数有关：

- sv32中 `satp` 为32位，其中低20位存放根页表地址，高12位存放分页模式（0000表示关闭分页，0001表示开启Sv32）
- sv39和sv64中 `satp` 为64位，其中低44位存放根页表地址，高4位存放分页模式（0000表示关闭分页，1000表示开启Sv39，1001表示开启Sv48），具体格式如下图所示：

![31](C:\Users\86186\Downloads\31.jpg)

那么如何利用 `satp` 寻址呢？以三级页表sv39为例：我们可以通过 `satp` 获取到二级页表（根页表）的地址，再使用VPN[2]获取到对应的页表项。根据页表项可以取到一级页表的地址，再使用VPN[1]获取到一级页表项。同，然后继续用VPN[0]取到零级页表项。零级页表项就可以取出虚拟地址对应的物理页地址， 加上页内偏移地址，就完成了整个寻址过程。

类似地，sv32需要少寻找一次，因为只有两级页表(但需要注意页号是10位的)；sv48需要多寻找一次

整个寻址过程的示意图如下所示：

![34](C:\Users\86186\Downloads\34.png)

##### 2.2 get_pte 函数解析

参数及返回值解释：

- pgdir:  PDT(Page Directory Table，疑似x86遗留叫法)的内核虚拟基地址
- la:   需要映射的线性地址
- create: 决定是否为 PT 分配页面的逻辑值
- 返回值：该 pte 的内核虚拟地址

由于我们使用的是sv39分页模式，这个函数的逻辑就是上面描述的那样，首先第一段代码：

```c
pde_t *pdep1 = &pgdir[PDX1(la)];
if (!(*pdep1 & PTE_V)) { //没有找到，检查create
   struct Page *page;
    if (!create || (page = alloc_page()) == NULL) {
        return NULL;
    }
    set_page_ref(page, 1);
    uintptr_t pa = page2pa(page);
    memset(KADDR(pa), 0, PGSIZE);
    *pdep1 = pte_create(page2ppn(page), PTE_U | PTE_V);
}
```

首先，利用 `PDX` 宏计算虚拟地址 `la` 的页目录项索引，并将其指向 `pgdir` 的**VPN[1]**。然后进入判断，检查这个目录项是否有效，检查是否为pte分配页面，如果是的话则分配一个新页面并设置映射数量为1，然后利用`KADDR`获取新页面的物理地址并返回对应的内核虚拟地址，最后分配一个新的 PTE。

接着是极为相似的第二段代码：

```c
pde_t *pdep0 = &((pde_t *)KADDR(PDE_ADDR(*pdep1)))[PDX0(la)];
if (!(*pdep0 & PTE_V)) {
	struct Page *page;
	if (!create || (page = alloc_page()) == NULL) {
		return NULL;
	}
	set_page_ref(page, 1);
	uintptr_t pa = page2pa(page);
	memset(KADDR(pa), 0, PGSIZE);
	*pdep0 = pte_create(page2ppn(page), PTE_U | PTE_V);
}
```

首先，通过 `pdep1` 和虚拟地址 `la` 得到 **VPN[0]** ，即`pdep0`，具体做法是：将pdep1所指向的页目录项中存放的物理地址转换成虚拟地址，并强制转换成页目录项类型的指针，即 `(pde_t *)KADDR(PDE_ADDR(*pdep1))`，然后利用 `PDX` 宏提取 `la` 中的第二级页目录索引，接下来的操作和上面一模一样，检查有效性以及分配页面和pte。

```c
return &((pte_t *)KADDR(PDE_ADDR(*pdep0)))[PTX(la)];
```

最后，返回对应的页表项的指针。

> 在设计中，页表项从高到低的三级页表的页码分别称作PDX1,PDX0和PTX

这两段代码极为相似的原因是**它们执行了相同的操作，只不过是在不同级别的页表上**，因为这个函数是为了支持Sv39分页模式而设计的，所以它需要处理三级页表，同时由于每一级页表的管理方式和原理都相似，这个函数应该容易变化为 `sv32` 和 `sv48` 分页模式下的。比如对于 `sv32`，我们可以把 `PDX1(la)` 修改为 `PDX(la)` 因为sv32模式只有一个页目录索引；同时把 `pdep0` 改为 `pdep`，因为sv32只有一个页表项指针；还有把 `KADDR(PDE_ADDR(pdep1))` 改为 `KADDR(pdep1)`，因为Sv32模式的这一级中存放的就是页表的物理地址，不需要再取低44位。

##### 2.3 合并 or 拆分

我觉得这种写法好，因为调用者只需要调用一次 `get_pte` 就可以获得对应的页表项的指针，无论它是否存在，如果拆开成两个函数，一个用来查找页表项，一个用来分配页表项，那么调用者就需要先调用查找函数，然后判断返回值是否为空，如果为空再调用分配函数，然后再次调用查找函数，这样就更加要求调用者的细心程度，并且很容易出错，虽然这种写法可能导致一些不必要的开销，比如在不需要创建新页表的情况下，也要传递一个create参数，但对于操作系统这么大的工程，简化开发的优先级应该比解耦更高。

# 练习3：给未被映射的地址映射上物理页

## 设计实现过程说明

`do_pgfault` 函数是缺页处理程序，负责在页面访问异常时处理页缺失，分配新的物理页并建立虚拟地址和物理地址之间的映射。这是具体的实现步骤：

1. **查找相关的 VMA**：
   - `find_vma(mm, addr)` 用于查找包含给定地址的虚拟内存区域（VMA）。如果没有找到相关的 VMA，说明这个地址不合法，不应该被访问，因此直接返回错误。

2. **设置页面权限**：
   - 根据找到的 VMA，确定页面权限。权限由 `vma->vm_flags` 来决定，如果该 VMA 是可写的，则设置页面权限为用户可读写（`PTE_U | PTE_R | PTE_W`）。

3. **获取页表项地址**：
   - 使用 `get_pte(mm->pgdir, addr, 1)` 获取对应的页表项。如果页表不存在，则分配一个新的页表页。

4. **判断页表项的状态**：
   - 如果页表项内容为 0，说明该页没有映射到物理内存，需要调用 `pgdir_alloc_page` 分配一个新的物理页并建立映射。
   - 如果页表项内容不是 0，说明该页可能是一个交换条目，需要从磁盘加载该页面。通过 `swap_in()` 函数将页面从磁盘中加载到内存中，接着通过 `page_insert()` 建立物理地址和线性地址的映射，再通过 `swap_map_swappable()` 将该页面设置为可交换。

## 页目录项（PDE）和页表项（PTE）在页替换中的潜在用途

页目录项（PDE）和页表项（PTE）在 ucore 实现页替换算法中具有以下潜在用途：

1. **有效位（Present Bit）**：
   - 用于判断该页是否存在于内存中。当有效位为 0 时，表示该页不在内存中，缺页异常触发后需要调用页替换算法将页面换入内存。

2. **访问位（Accessed Bit）**：
   - 记录页面是否被访问过。ucore 可以利用此位来选择需要替换的页面，例如在实现 LRU 或类似算法时，通过访问位来判断页面是否活跃。

3. **脏页位（Dirty Bit）**：
   - 记录页面是否被修改过。若页面被修改且需要换出时，必须将其内容写回磁盘。页替换算法可以利用此位决定是否需要将页面写回磁盘。

4. **权限位（Write, User）**：
   - 用于保护页面不被非法访问，页替换过程需要根据 VMA 来设置新页面的访问权限，保证替换后的页面权限与原页面保持一致。

## 缺页服务例程中页访问异常时的硬件操作

当缺页服务例程在执行过程中访问内存，出现页访问异常时，硬件执行的操作如下：

1. **触发缺页异常**：
   - 当处理器访问一个不存在于内存中的页时，产生缺页异常（Page Fault），并进入异常处理模式。

2. **保存处理器状态**：
   - 处理器保存当前执行状态，以便在异常处理结束后可以恢复程序的正常执行。

3. **将缺页地址存入 CR2 寄存器**：
   - 处理器会将发生缺页异常的线性地址存入特定寄存器（例如 x86 架构中的 CR2 寄存器），以便操作系统的缺页处理程序可以查找对应的页表项。

4. **跳转到缺页异常处理程序**：
   - 处理器根据中断向量表跳转到操作系统的缺页异常处理程序，操作系统利用 CR2 中的信息定位发生缺页的地址并处理缺页。

## `Page` 数据结构与页表项的对应关系

`Page` 数据结构是一个全局数组，每个元素对应一个物理页面，与页表项存在以下关系：

1. **物理页面的管理**：
   - `Page` 数据结构用于记录物理页面的状态，例如是否空闲、被哪个进程使用、是否可换出等。每个 `Page` 实例代表一个物理页面。

2. **页表项和 `Page` 的映射**：
   - 当一个虚拟地址被映射到物理页面时，页表项中记录了对应的物理地址。而 `Page` 数据结构中的信息记录了物理页面的管理信息。
   - 页表项中的物理地址字段指向物理内存中的某一页，而该物理页的元数据由全局的 `Page` 数组来管理。

3. **页替换过程中的使用**：
   - 在页面换入或换出过程中，需要通过页表项找到对应的 `Page` 结构体。例如，在缺页中断中通过 `get_pte` 获取页表项地址，再通过页表项中的物理地址找到对应的 `Page` 结构体以操作页面数据。

通过以上设计，`do_pgfault` 函数可以正确处理缺页异常，实现页面从磁盘加载到内存、建立物理地址与虚拟地址的映射，并且能够记录和管理页面的状态以支持后续的页面置换。


# 练习4：补充完成Clock页替换算法

## 设计实现过程

Clock 页替换算法是一种对 FIFO 算法的改进，它通过维护一个循环链表和访问位来更智能地选择被替换的页面。以下是 Clock 算法的实现步骤：

1. **初始化页面链表和指针**：
   - 在 `_clock_init_mm` 函数中，首先初始化 `pra_list_head` 为空链表，并设置 `curr_ptr` 指向 `pra_list_head`，表示当前页面替换的位置。
   - 同时，将 `mm->sm_priv` 指向 `pra_list_head`，便于后续页面替换算法操作。

2. **页面映射操作**：
   - 在 `_clock_map_swappable` 函数中，将新到达的页面插入到页面链表 `pra_list_head` 的末尾，并将页面的 `visited` 标志设置为 1，表示该页面已被访问。

3. **选择要换出的页面**：
   - 在 `_clock_swap_out_victim` 函数中，使用 Clock 算法选择要换出的页面。
   - Clock 算法的核心思想是遍历页面链表，查找 `visited` 标志为 0 的页面。如果找到，则将其从链表中删除并返回。
   - 如果当前页面的 `visited` 标志为 1，则将其设置为 0，表示该页面已被重新访问，然后继续查找下一个页面。
   - 为了实现 Clock 算法的循环特性，我们在每次查找时移动 `curr_ptr`，确保遍历整个链表。

## Clock 页替换算法和 FIFO 算法的比较

### 1. **替换策略**
   - **FIFO（First-In-First-Out）**：
     - FIFO 算法根据页面进入内存的顺序来选择被替换的页面，最先进入内存的页面会被最先换出。
     - 这种策略简单且低开销，但会导致 "Belady's Anomaly"，即增加内存页面数反而会增加缺页率。
   - **Clock 算法**：
     - Clock 算法是对 FIFO 的改进，使用一个循环指针和访问位来判断页面是否应该被替换。
     - 当要替换页面时，如果页面的访问位为 1，表示该页面最近被访问过，则将访问位重置为 0 并跳过该页面，继续查找其他页面。
     - 如果访问位为 0，则选择该页面进行替换。
     - 这种改进使得 Clock 算法能够更好地保留活跃的页面在内存中，减少不必要的页面替换。

### 2. **性能和复杂度**
   - **FIFO 算法**：
     - FIFO 算法的实现较为简单，只需维护一个队列即可，但它在很多实际应用中性能较差，因为它没有考虑页面的访问频率。
     - 例如，一个频繁使用的页面可能会因为先进入内存而被优先换出，导致性能下降。
   - **Clock 算法**：
     - Clock 算法通过访问位的判断来避免替换掉频繁访问的页面，使得它在性能上优于 FIFO。
     - 它的实现稍微复杂一些，需要维护一个循环指针和访问位，但能够显著减少缺页率。

### 3. **实现差异**
   - **FIFO 实现**：
     - FIFO 算法只需要维护一个简单的链表，始终替换链表头的页面。
   - **Clock 实现**：
     - Clock 算法需要一个循环链表，并使用 `curr_ptr` 作为循环指针，每次查找未被访问的页面进行替换。
     - Clock 算法还需要维护页面的 `visited` 标志位，以记录页面是否被访问过。

## 总结

Clock 页替换算法通过增加一个访问位和循环指针，在保留 FIFO 算法简单性的同时，大大提高了性能。它有效地避免了频繁访问的页面被替换的情况，从而降低了缺页率。而 FIFO 算法由于不考虑页面的使用情况，可能导致性能问题，尤其是在页面访问模式变化较大时。因此，Clock 算法在实际应用中更为常用，尤其是在需要提高页面命中率的场景下。


# 练习5：页表映射方式分析

## 思考题

如果我们采用“一个大页”的页表映射方式，相比分级页表，有什么好处、优势，有什么坏处、风险？

### 好处：

1. **减少页表项数量，节省内存开销**  
   采用大页可以减少需要存储的页表项数量，节省了内存空间。
   
2. **提高 TLB（Translation Lookaside Buffer）命中率**  
   每个 TLB 条目能覆盖更大的虚拟地址范围。因此，在程序访问大量连续地址时，TLB 的命中率会显著提高，进而减少了地址转换的开销。
   
3. **减少页表访问次数**  
   分级页表需要多次内存访问来完成地址转换（每一级页表都可能引发一次内存访问）。而大页页表通常只需一次访问，大幅减少内存访问延迟。
   
4. **适合大块连续内存分配**  
   对于需要映射大块连续内存的应用（如大数据处理、虚拟机、GPU 内存等），大页可以减少复杂性，提高映射效率。

### 坏处：

1. **内存浪费（碎片化）**  
   大页粒度意味着分配的内存块可能包含大量未使用的地址空间，这可能导致内存碎片化加剧。对于小数据块或不连续内存访问的应用，内存利用率可能显著降低。
   
2. **不适合小数据场景**  
   如果程序频繁访问的小数据块分散在地址空间中，大页可能会浪费大量不必要的内存，同时不会显著提高性能。
   
3. **灵活性下降**  
   分级页表允许更细粒度地控制内存映射和保护属性，而大页缺乏这种灵活性。不适合实现复杂的内存管理策略，如按需分页、内存共享等。
   
4. **管理复杂度增加**  
   在某些硬件架构中，支持大页可能需要更复杂的页表管理代码和配置。如果需要动态调整页大小，会增加软件实现的复杂性。

---

# Challenge：LRU 页替换算法实现

设计实现过程：  
大部分代码补充部分均与 fifo 算法一致，但我们需要修改其页面换出的方法。我实现的具体逻辑是触发时钟事件，从而执行 `lru_tick_event` 函数，这个函数将会扫描链表中所有的页面，通过页表项中的 A 位获知是否被访问过。如果被访问过，就将该页面放到链表首部，这样每次从尾部换出页面时就能保证是最久之前被使用的页面了。

具体的 `lru_tick_event` 函数代码及解释如下：

```c
static int
_lru_tick_event(struct mm_struct *mm)
{ 
    // 获取 LRU 链表的头节点
    list_entry_t *head = (list_entry_t *)mm->sm_priv;
    assert(head != NULL); // 确保链表头节点不为空

    // 从链表的第一个页面开始扫描
    list_entry_t *entry = list_next(head);

    // 遍历整个 LRU 链表
    while (entry != head)
    {
        // 获取与该链表节点相关联的 Page 结构体
        struct Page *page = le2page(entry, pra_page_link);

        // 获取该页面虚拟地址的页表项指针
        pte_t *ptep = get_pte(mm->pgdir, page->pra_vaddr, 0);

        /**
         * 检查页表项中的 A 位（访问位）。
         * 如果 A 位为 1，表示该页面自上次扫描以来有被访问过。
         */
        if ( PTE_A & *ptep )
        {
            // 将页面从链表中删除
            list_del(entry);

            // 将该页面插入到链表的首部（表示它最近被访问过）
            list_add(head, entry);

            // 清除 A 位，重置页面的访问状态
            *ptep &= ~PTE_A;

            // 使该页面的 TLB 条目失效，以确保 A 位的修改生效
            tlb_invalidate(mm->pgdir, page->pra_vaddr);
        }

        // 移动到下一个页面节点
        entry = list_prev(head);
    }

    // 输出日志，表示链表已更新
    cprintf("LRU链表已更新\n");
    return 0; 
}




