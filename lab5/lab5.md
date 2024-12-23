## 练习0：

#### alloc_proc函数

```
static struct proc_struct *
alloc_proc(void) {
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc != NULL) {
        proc->state = PROC_UNINIT;
        proc->pid = -1;
        proc->runs = 0;
        proc->kstack = 0;
        proc->need_resched = 0;
        proc->parent = NULL;
        proc->mm = NULL;
        memset(&(proc->context), 0, sizeof(struct context));
        proc->tf = NULL;
        proc->cr3 = boot_cr3;
        proc->flags = 0;
        memset(proc->name, 0, PROC_NAME_LEN + 1);

     
        proc->wait_state = 0;                        
        proc->cptr = proc->yptr = proc->optr = NULL; 
    }
    return proc;
}
```

改进的内容为：

```
proc->wait_state = 0; //初始化进程等待状态
proc->cptr = proc->optr = proc->yptr = NULL; //指针初始化
```

代码用于初始化进程等待状态、和进程的相关指针

#### do_fork函数

    int
    do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
        int ret = -E_NO_FREE_PROC;
        struct proc_struct *proc;
        if (nr_process >= MAX_PROCESS) {
            goto fork_out;
        }
        ret = -E_NO_MEM;
    
       if((proc = alloc_proc()) == NULL)
        {
            goto fork_out;
        }
        proc->parent = current; 
        assert(current->wait_state == 0);
        if(setup_kstack(proc) != 0)
        {
            goto bad_fork_cleanup_proc;
        }
        ;
        if(copy_mm(clone_flags, proc) != 0)
        {
            goto bad_fork_cleanup_kstack;
        }
        if(cow_copy_mm(proc) != 0) {
            goto bad_fork_cleanup_kstack;
        }
        copy_thread(proc, stack, tf);
        bool intr_flag;
        local_intr_save(intr_flag);
        {
            proc->pid = get_pid();
            hash_proc(proc);
            set_links(proc);
        }
        local_intr_restore(intr_flag);
        wakeup_proc(proc);
        ret = proc->pid;
     
    fork_out:
        return ret;
    
    bad_fork_cleanup_kstack:
        put_kstack(proc);
    bad_fork_cleanup_proc:
        kfree(proc);
        goto fork_out;
    }

改进内容为：

```
assert(current->wait_state == 0); //确保进程在等待
set_links(proc); //设置进程链接
```



## 练习1：加载应用程序并执行（需要编码）

```
tf-> gpr.sp = USTACKTOP;
tf-> epc = elf-> e_entry;
tf-> status = (sstatus & ~SSTATUS_SPP) | SSTATUS_SPIE;
```

- `tf->gpr.sp` ：设置用户栈指针为用户栈的顶部。
- `tf->epc` ：设置用户程序的入口地址。
- `tf->status` ：设置状态寄存器，确保用户程序在用户态运行，并开启中断

#### 简述过程：

1. **进程创建和等待：**
   - `initproc` 创建用户进程 `userproc`。
   - `initproc` 调用 `do_wait`，等待 `RUNNABLE` 状态的子进程。
2. **进程调度和运行：**
   - 当存在 `RUNNABLE` 子进程，`schedule` 激活并调用 `proc_run`。
   - `proc_run` 设置栈指针、加载页目录表，切换上下文并跳至 `forkret`。
3. **进程切换和系统调用处理：**
   - `forkret` 调用 `forkrets`，跳转到 `__trapret`，恢复寄存器并通过 `iret` 跳转到 `kernel_thread_entry`。
   - `kernel_thread_entry` 压栈后跳转至 `user_main`。
   - `user_main` 打印信息后调用 `kernel_execve` 执行 `exec`。
4. **应用程序加载和执行：**
   - `do_execve` 处理系统调用，释放内存，调用 `load_icode` 加载程序。
   - 设置页目录表，初始化 BSS，更新进程状态。
5. **从内核态到用户态的转换：**
   - `__trapret` 执行返回，跳转到应用程序入口。
   - `alltraps` 和 `trapret` 维护寄存器状态，实现内核态到用户态的过渡。

## 练习2: 父进程复制自己的内存空间给子进程（需要编码）

#### 创建子进程的函数do_fork在执行中将拷贝当前进程（即父进程）的用户内存地址空间中的合法内容到新进程中（子进程），完成内存资源的复制。具体是通过copy_range函数（位于kern/mm/pmm.c中）实现的，请补充copy_range的实现，确保能够正确执行。

#### copy_range的实现

基于拷贝父进程的用户地址空间的目的，那么我们首先要先记录父进程的页表项，然后为子进程创建新的页表项，然后将刚刚父进程页表项对应的所有内存页拷贝到子进程页表项对应的内存页中。最后把这个子进程页表项添加进子进程的页表。那么具体代码如下：

```
uintptr_t *src_kvaddr = page2kva(page); //父进程内存页的地址
uintptr_t *dst_kvaddr = page2kva(npage); //子进程的内存页的地址
memcpy(dst_kvaddr, src_kvaddr, PGSIZE); //复制内存页
ret = page_insert(to, npage, start, perm); //将子进程的页表项加入到子进程的页表中
```

#### 如何设计实现Copy on Write机制？给出概要设计，鼓励给出详细设计。

#### Copy on Write机制

查看Copy on Write机制的定义，我们考虑到实现这个机制的关键是只获得复制的页表项，即指向同一个资源的指针，但是使用同一个内存页，并且将两个进程的页表项的权限都设置为只读，那么读操作时就可以同时读同一个内存页。如果有进程进行写操作就会触发缺页异常，然后在缺页异常处理函数中我们可以再把内存页复制一遍，这就拥有了私有拷贝，这样进程可以单独对此进行写操作。



## 练习3：阅读分析源代码，理解进程执行 fork/exec/wait/exit 的实现，以及系统调用的实现（不需要编码）

#### 1、fork函数

```
fork：通过系统调用执行 do_fork，用于创建并唤醒线程：
初始化新线程。
分配内核栈和虚拟内存（或共享）。
获取并设置原线程的上下文与中断帧。
将新线程插入哈希表和链表。
唤醒新线程，返回线程ID。
```

在用户态：父进程init调用fork函数，随后触发系统调用SYS_fork进入内核态；

内核态：do_fork函数复制init进程的所有资源如mm、文件信息等，还会新建`proc_struct`和子进程的内核栈，这样就创建一个子进程，随后fork调用返回，程序将回到用户态

用户态：通过fork函数得到了新进程PID

#### 2、exec函数

```
exec：通过系统调用执行 do_execve，用于加载并执行用户程序：
回收当前线程的虚拟内存。
分配新虚拟内存并加载应用程序。
```

在用户态：父进程将PC放在`user_main`函数处，通过宏定义进行系统调用，进入`kernel_execve()`。

内核态：`kernel_execve()`通过`ebreak`进行系统调用，进而调用 `load_icode` 函数加载二进制文件，随后执行用户进程

#### 3、wait函数

```
wait：通过系统调用执行 do_wait，用于等待线程完成：
查找状态为 PROC_ZOMBIE 的子线程。
设置线程状态，切换线程，或调用 do_exit 处理已退出线程。
删除线程并释放资源。
```

用户态：执行wait函数，随后使用系统调用进入`sys_wait`函数，此时进入内核态

内核态：调用`do_wait`函数，让父进程休眠并调度，等待这子进程的退出

用户态：`do_wait`函数退出后将释放子进程的内核堆栈、子进程的进程控制块，此时回到用户态的`wait`函数返回的地方，继续执行用户态函数中`wait`函数返回后面的代码

#### 4、exit函数

```
exit：通过系统调用执行 do_exit，用于退出线程：
销毁当前线程的虚拟内存（如果未被其他线程使用）。
将线程状态设为 PROC_ZOMBIE，唤醒父线程。
调用 schedule 切换到其他线程。
```

用户态：执行exit函数，随后使用系统调用进入`sys_exit`函数，此时进入内核态

内核态：调用`do_exit`函数，释放进程的虚拟内存空间、调用调度函数进行调度，选择新的进程去执行

用户态：`do_exit`函数退出后将回到用户态的`wait`函数返回的地方，继续执行用户态函数中`exit`函数返回后面的代码

### 执行流程

在操作系统中，系统调用在内核态执行，用户程序则在用户态运行。当用户程序发起系统调用时，产生 `ebreak` 异常，切换到内核态。在内核态，内核利用进程控制块和中断帧等数据结构记录用户程序的当前状态，完成系统调用后，通过 `kernel_execve_ret` 将中断帧添加到线程的内核栈中，并使用 `sret` 指令返回到用户态。内核在返回之前通过中断返回指令（IRET）恢复用户程序的执行环境，从而使得用户程序能够继续从中断点恢复执行。

### 生命周期图

```shell
                    +-------------+
               +--> |	 none 	  |
               |    +-------------+       ---+
               |          | alloc_proc	     |
               |          V				     |
               |    +-------------+			 |
               |    | PROC_UNINIT |			 |---> do_fork
               |    +-------------+			 |
      do_wait  |         | wakeup_proc		 |
               |         V			   	  ---+
               |    +-------------+    do_wait 	  	  +-------------+
               |    |PROC_RUNNABLE| <------------>    |PROC_SLEEPING|
               |    +-------------+    wake_up        +-------------+
               |         | do_exit
               |         V
               |    +-------------+
               +--- | PROC_ZOMBIE |
                    +-------------+
```



## Challenge1

#### 1. `cow_copy_mm`

```
int cow_copy_mm(struct proc_struct *proc) {
    struct mm_struct *mm, *oldmm = current->mm;

    if (oldmm == NULL) {
        return 0;
    }
    int ret = 0;
    if ((mm = mm_create()) == NULL) {
        goto bad_mm;
    }
    if (setup_pgdir(mm) != 0) {
        goto bad_pgdir_cleanup_mm;
    }
    lock_mm(oldmm);
    {
        ret = cow_copy_mmap(mm, oldmm);
    }
    unlock_mm(oldmm);

    if (ret != 0) {
        goto bad_dup_cleanup_mmap;
    }

good_mm:
    mm_count_inc(mm);
    proc->mm = mm;
    proc->cr3 = PADDR(mm->pgdir);
    return 0;

bad_dup_cleanup_mmap:
    exit_mmap(mm);
    put_pgdir(mm);
bad_pgdir_cleanup_mm:
    mm_destroy(mm);
bad_mm:
    return ret;
}
```

- **作用**：进行进程内存空间的复制。

- **分析**：

  - 通过调用 `mm_create()` 创建一个新的 `mm_struct` 结构。
  - 使用 `setup_pgdir()` 为新进程分配和初始化页目录。
  - 调用 `lock_mm` 和 `unlock_mm` 锁定旧进程的内存管理结构，确保内存映射在复制过程中不会发生变化。
  - 使用 `cow_copy_mmap()` 复制内存映射（即虚拟内存区域）。
  - 在成功复制后，增加新进程的内存引用计数，并将新进程的 `mm` 和 `cr3`（页目录基址）进行关联。

  **关键点**：`cow_copy_mmap()` 是关键函数，它会复制旧进程的虚拟内存区域，确保 COW 机制的实现。

#### 2. `cow_copy_mmap`

```
int cow_copy_mmap(struct mm_struct *to, struct mm_struct *from) {
    assert(to != NULL && from != NULL);
    list_entry_t *list = &(from->mmap_list), *le = list;
    while ((le = list_prev(le)) != list) {
        struct vma_struct *vma, *nvma;
        vma = le2vma(le, list_link);
        nvma = vma_create(vma->vm_start, vma->vm_end, vma->vm_flags);
        if (nvma == NULL) {
            return -E_NO_MEM;
        }
        insert_vma_struct(to, nvma);
        if (cow_copy_range(to->pgdir, from->pgdir, vma->vm_start, vma->vm_end) != 0) {
            return -E_NO_MEM;
        }
    }
    return 0;
}
```

- **作用**：复制虚拟内存区域（VMA）。
- 分析
  - 遍历 `from` 进程的 `mmap_list`，每一个 `vma_struct` 代表一个虚拟内存区域。
  - 为每个 VMA 创建一个新的 VMA，并通过 `insert_vma_struct()` 将其插入到新进程的 `to` 进程的内存管理结构中。
  - 通过 `cow_copy_range()` 复制 VMA 对应的内存页。
  - **关键点**：`cow_copy_range()` 是实现 COW 的具体逻辑，它将物理内存页从父进程映射到子进程，并设置为只读。

#### 3. `cow_copy_range`

```
int cow_copy_range(pde_t *to, pde_t *from, uintptr_t start, uintptr_t end) {
    assert(start % PGSIZE == 0 && end % PGSIZE == 0);
    assert(USER_ACCESS(start, end));
    do {
        pte_t *ptep = get_pte(from, start, 0);
        if (ptep == NULL) {
            start = ROUNDDOWN(start + PTSIZE, PTSIZE);
            continue;
        }
        if (*ptep & PTE_V) {
            *ptep &= ~PTE_W;
            uint32_t perm = (*ptep & PTE_USER & ~PTE_W);
            struct Page *page = pte2page(*ptep);
            assert(page != NULL);
            int ret = 0;
            ret = page_insert(to, page, start, perm);
            assert(ret == 0);
        }
        start += PGSIZE;
    } while (start != 0 && start < end);
    return 0;
}
```

- **作用**：复制物理页面，并设置为只读。
- 分析
  - 遍历 `start` 到 `end` 之间的内存页。
  - 获取每一页的页表项，检查其是否有效（通过 `PTE_V` 标志位）。
  - 如果有效，则将该页表项的写权限去掉（即设置为只读），并将该页插入到子进程的页表中。
  - **关键点**：通过将页面设置为只读，并延迟写操作（直到页面发生写时复制），实现 COW 的核心思想。

#### 4. `cow_pgfault`

```
int cow_pgfault(struct mm_struct *mm, uint_t error_code, uintptr_t addr) {
    cprintf("COW page fault at 0x%x\n", addr);
    int ret = 0;
    pte_t *ptep = NULL;
    ptep = get_pte(mm->pgdir, addr, 0);
    uint32_t perm = (*ptep & PTE_USER) | PTE_W;
    struct Page *page = pte2page(*ptep);
    struct Page *npage = alloc_page();
    assert(page != NULL);
    assert(npage != NULL);
    uintptr_t* src = page2kva(page);
    uintptr_t* dst = page2kva(npage);
    memcpy(dst, src, PGSIZE);
    uintptr_t start = ROUNDDOWN(addr, PGSIZE);
    *ptep = 0;
    ret = page_insert(mm->pgdir, npage, start, perm);
    ptep = get_pte(mm->pgdir, addr, 0);
    return ret;
}
```

- **作用**：处理 COW 页面缺页异常。
- 分析
  - 当进程对一个只读页面进行写操作时，触发页面错误，进入 `cow_pgfault`。
  - 通过 `get_pte()` 获取对应的页表项，并将该页表项的权限改为可写。
  - 复制原页面到一个新的物理页面（通过 `alloc_page()` 分配新页）。
  - 将新的物理页面插入到进程的页表中，并更新页表项。
  - **关键点**：通过将原页面复制到新页面，并修改页表项，避免对共享页面的写入冲突，从而实现 COW。

#### 5.`do_pgfault`中添加判断页表项：

```
if ((ptep = get_pte(mm->pgdir, addr, 0)) != NULL) {
    if((*ptep & PTE_V) & ~(*ptep & PTE_W)) {
        return cow_pgfault(mm, error_code, addr);
    }
}
```

#### 6.`do_fork`函数中添加`cow_copy_mm`：

```
if(cow_copy_mm(proc) != 0) {
    goto bad_fork_cleanup_kstack;
}
```

### 总结

COW 的核心思想是在多个进程共享相同页面时，直到某个进程写入时才会创建页面的副本。通过修改页表项为只读并延迟复制操作，`cow_pgfault()` 捕获写操作时的缺页异常并完成页面复制，从而避免不必要的内存复制，优化内存使用。在 `cow_copy_mm()` 和 `cow_copy_mmap()` 中，通过逐一复制虚拟内存区域和页表来建立进程间的共享内存结构。





## Challenge2

- **何时载入内存**
  用户程序是在整个项目编译时载入内存的。在宏定义 `KERNEL_EXECVE` 中可以看到，用户态程序的加载实际上是通过特定的编译输出文件完成的。在本次实验中，修改了 `Makefile`，并通过 `ld` 命令将 `user` 文件夹中的用户程序代码链接到项目中。因此，用户程序在 `ucore` 启动时就已经被加载到内存中。
- **与常用操作系统加载的区别与原因**
  与常规操作系统不同，常规操作系统中的应用程序通常会在需要时才加载到内存中，即只有用户需要运行某个程序时，操作系统才会将其加载到内存。而在本次实验中，用户程序是通过修改 `Makefile` 执行 `ld` 命令，将执行程序的代码直接连接到内核代码的末尾，导致用户程序在操作系统启动时就已经被加载到内存中。原因是我们没有实现用户进程间的调度，用户程序是直接由 `init` 进程 `fork` 出来的。为了简化代码实现，用户程序在启动时就加载到内存，而无需等待调度器的加载。