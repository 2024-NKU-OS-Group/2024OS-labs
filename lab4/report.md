# 练习1：分配并初始化一个进程控制块（需要编码）

在操作系统中，进程的上下文和中断帧是处理进程切换和系统调用时不可或缺的数据结构。下面是对两个关键结构的详细说明：

## 1. `struct context`

`struct context` 用于保存进程执行时的上下文信息，也就是进程的关键寄存器值。这些寄存器值记录了进程的状态，以便在进程切换时恢复执行状态。

### 主要功能：
- 保存进程的寄存器值。
- 在进程切换时，通过 `switch_to` 函数将当前进程的寄存器值保存。
- 在进程切换回来时，恢复之前的执行状态。

### 作用：
- 在通过 `proc_run` 将进程调度到CPU上运行时，需要保存当前进程的寄存器值以便下次切换回该进程时可以恢复运行状态。

---

## 2. `struct trapframe`

`struct trapframe` 用于保存进程的中断帧，包括 32 个通用寄存器和与异常相关的寄存器。在进程从用户空间跳转到内核空间时，系统调用会改变寄存器的值。

### 主要功能：
- 保存进程的中断帧，包括寄存器状态和异常信息。
- 系统调用时，可以通过调整中断帧来控制返回特定的值。

### 作用：
- 在进程执行系统调用时，操作系统可以利用 `struct trapframe` 来传递执行函数和参数。
- 在创建子线程时，可以通过将中断帧中的寄存器（例如 `a0`）设置为特定值来传递特定信息。

---

## 3. 使用示例

在创建子线程时，可能会通过修改 `struct trapframe` 中的 `a0` 寄存器为 0 来表示子线程的创建请求。也可以通过其他寄存器传递函数指针及其参数来执行特定任务。


# 练习2：为新创建的内核线程分配资源（需要编码）



创建一个内核线程需要分配和设置好很多资源。kernel_thread函数通过调用**do_fork**函数完成具体内核线程的创建工作。do_kernel函数会调用alloc_proc函数来分配并初始化一个进程控制块，但alloc_proc只是找到了一小块内存用以记录进程的必要信息，并没有实际分配这些资源。ucore一般通过do_fork实际创建新的内核线程。do_fork的作用是，创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同。在这个过程中，需要给新内核线程分配资源，并且复制原进程的状态。你需要完成在kern/process/proc.c中的do_fork函数中的处理过程。它的大致执行步骤包括：

- 调用alloc_proc，首先获得一块用户信息块。
- 为进程分配一个内核栈。
- 复制原进程的内存管理信息到新进程（但内核线程不必做此事）
- 复制原进程上下文到新进程
- 将新进程添加到进程列表
- 唤醒新进程
- 返回新进程号

答：

首先，先回看word笔记“进程创建”这一讲。会发现创建新进程需要做下面的工作基本都是复制父进程。只有pid号改变了。

看函数调用图：

[![image-20200617231911066](E:\JW\lab3\report\image1.png)

kern/process/proc.c::do_fork

```
int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf) {
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc;
    if (nr_process >= MAX_PROCESS) {
        goto fork_out;
    }
    ret = -E_NO_MEM;
    //LAB4:EXERCISE2 YOUR CODE
    /*
     * Some Useful MACROs, Functions and DEFINEs, you can use them in below implementation.
     * MACROs or Functions:
     *   alloc_proc:   create a proc struct and init fields (lab4:exercise1)
     *   setup_kstack: alloc pages with size KSTACKPAGE as process kernel stack
     *   copy_mm:      process "proc" duplicate OR share process "current"'s mm according clone_flags
     *                 if clone_flags & CLONE_VM, then "share" ; else "duplicate"
     *   copy_thread:  setup the trapframe on the  process's kernel stack top and
     *                 setup the kernel entry point and stack of process
     *   hash_proc:    add proc into proc hash_list
     *   get_pid:      alloc a unique pid for process
     *   wakup_proc:   set proc->state = PROC_RUNNABLE
     * VARIABLES:
     *   proc_list:    the process set's list
     *   nr_process:   the number of process set
     */

    //    1. call alloc_proc to allocate a proc_struct
    //    2. call setup_kstack to allocate a kernel stack for child process
    //    3. call copy_mm to dup OR share mm according clone_flag
    //    4. call copy_thread to setup tf & context in proc_struct
    //    5. insert proc_struct into hash_list && proc_list
    //    6. call wakup_proc to make the new child process RUNNABLE
    //    7. set ret vaule using child proc's pid

    /*
     * 想干的事：创建当前内核线程的一个副本，它们的执行上下文、代码、数据都一样，但是存储位置不同，PID不同。
     */
    // 调用alloc_proc() 为要创建的线程分配空间
    // 如果第一步 alloc 都失败的话，应该来说是比较严重的错误。直接退出。
    if ((proc = alloc_proc()) == NULL) {
        goto fork_out;
    }
    // 获取被拷贝的进程的pid号 即父进程的pid
    //proc->parent = current;
    // 分配大小为 KSTACKPAGE 的页面作为进程内核堆栈
    setup_kstack(proc);
    // 拷贝原进程的内存管理信息到新进程
    copy_mm(clone_flags, proc);
    // 拷贝原进程上下文到新进程
    copy_thread(proc, stack, tf);


    bool intr_flag;
    // 停止中断
    local_intr_save(intr_flag);
    // {} 用来限定花括号中变量的作用域，使其不影响外面。
    {
        proc->pid = get_pid();
        // 新进程添加到 hash方式组织的的进程链表，以便于以后对某个指定的线程的查找（速度更快）
        hash_proc(proc);
        // 将线程加入到所有线程的链表中，以便于调度
        list_add(&proc_list, &(proc->list_link));
        // 将全局线程的数目加1
        nr_process ++;
    }
    // 允许中断
    local_intr_restore(intr_flag);


    // 唤醒新进程
    wakeup_proc(proc);
    // 新进程号
    ret = proc->pid;

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);
bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}
```



请在实验报告中简要说明你的设计实现过程。请回答如下问题：

- 请说明ucore是否做到给每个新fork的线程一个唯一的id？请说明你的分析和理由。

答：

一方面，从上面的代码可以看出。在分配PID之前禁止了中断，然后再分配OK后允许中断。相当于事务的隔离性。

查看pid分配函数 proc.c::get_pid：

```
static int
get_pid(void) {
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;

    // 两个静态变量 next_safe = MAX_PID, last_pid = MAX_PID; 指向最大可以分配的pid号码
    static int next_safe = MAX_PID, last_pid = MAX_PID;

    if (++ last_pid >= MAX_PID) {
        last_pid = 1;
        goto inside;
    }

    if (last_pid >= next_safe) {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list) {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid) {
                // 如果last_pid+1 后等于MAX_PID，意味着pid已经分配完了
                if (++ last_pid >= next_safe) {
                    // 如果last_pid超出最大pid范围，则last_pid重新从1开始编号
                    if (last_pid >= MAX_PID) {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    // 重新编号去 现在 last_pid == 1; next_safe == MAX_PID;
                    goto repeat;
                }
            }
            // 上面的是需要重新编号的情况，下面是不需要的情况
            // 满足 last_pid < proc->pid < next_safe
            else if (proc->pid > last_pid && next_safe > proc->pid) {
                // last_pid < proc->pid < next_safe
                // last_pid < proc->pid
                //			< next_safe
                next_safe = proc->pid;
            }
        }
    }
    // last_pid作为新颁发的编号
    return last_pid;
}
```



注释很明确的说明了原因：分为需要从1开始编号的情况和无需重新编号共计两种情况。

# 练习3：编写proc_run 函数（需要编码）

## proc_run 函数编写
基于proc_run 函数要实现的功能，我实现了如下代码：
```c
void
proc_run(struct proc_struct *proc) {
    if (proc != current) {//检查要切换的进程是否与当前正在运行的进程相同，如果相同则不需要切换。
        bool intr_flag;//用于切换中断状态的标志位。

        struct proc_struct *prev = current;//暂时保存当前进程
        struct proc_struct *next = proc;// 保存需要切换的进程

        local_intr_save(intr_flag);        // 禁用中断
        {
            current = proc;// 切换当前进程为要运行的进程。
            lcr3(next->cr3);//切换页表，以便使用新进程的地址空间。这里通过加载新进程的页目录表(next)到CR3寄存器实现
            switch_to(&(prev->context), &(next->context));//实现上下文切换。这里通过调用switch_to函数实现，实现当前进程和新进程的context切换。
        }
        local_intr_restore(intr_flag);// 恢复之前的中断状态
    }
}
```
函数的功能是切换当前进程为指定的进程。它首先检查要切换的进程是否与当前正在运行的进程相同。如果是，则不需要切换。否则，它禁用中断，保存当前进程和要切换的进程，切换页表，实现上下文切换，恢复中断状态。
## 在本实验的执行过程中，创建且运行了几个内核线程？
两个内核线程，第一个是idleproc线程，第二个是initproc线程。
第一个线程idleproc是一个空闲进程。用于在系统没有其他任务需要执行时，占用 CPU 时间，同时便于进程调度的统一化。除此之外不做任何事情。
第二个线程initproc是一个内核线程，它由函数kernel_thread创建，并在其中调用函数init_main。这个函数会打印信息。schedule函数会调度该进程使其切换后运行。

# 扩展练习challenge
## 说明语句local_intr_save(intr_flag);....local_intr_restore(intr_flag);是如何实现开关中断的？
local_intr_save(intr_flag) 和 local_intr_restore(intr_flag) 通过保存和恢复中断状态来实现中断的开关。具体流程如下：
1. local_intr_save(intr_flag) 的执行过程
调用 __intr_save() 函数：在执行 local_intr_save(intr_flag) 时，实际上是调用了 __intr_save() 函数。该函数的作用是检查当前的中断状态，并根据中断状态决定是否禁用中断。 read_csr(sstatus) & SSTATUS_SIE：read_csr(sstatus) 会读取当前的 sstatus 寄存器，这个寄存器包含了当前中断的状态。SSTATUS_SIE 是其中的一个标志位，表示是否启用了中断。 
如果 SSTATUS_SIE 为 1，表示中断已启用，__intr_save() 会执行 intr_disable() 来禁用中断。 
如果 SSTATUS_SIE 为 0，表示中断未启用，__intr_save() 直接返回 0，不做任何操作。 
保存当前中断状态：无论中断是否被禁用，__intr_save() 函数都会将当前中断状态（1 或 0）返回并存储到传入的变量 intr_flag 中。这是为了在后续恢复中断时参考这个状态。 
2. 执行临界区代码
中断被禁用：当 local_intr_save(intr_flag) 执行完毕并保存了中断状态后，程序进入临界区代码。此时如果中断被禁用，系统不会响应外部中断，从而避免了在关键代码执行过程中被打断，确保了代码的原子性。 
中断未启用：如果中断在 local_intr_save 时就已禁用（即 intr_flag 为 0），那么程序继续执行临界区代码，期间不涉及中断操作。 
3. local_intr_restore(intr_flag) 的执行过程
调用 __intr_restore() 函数：在执行完临界区代码后，调用 local_intr_restore(intr_flag) 来恢复中断状态。此时，intr_flag 中存储了 local_intr_save 时保存的中断状态（1 或 0）。 如果 intr_flag 为 1，表示在进入临界区时中断被禁用，__intr_restore() 会调用 intr_enable() 来重新启用中断。 
如果 intr_flag 为 0，表示在进入临界区时中断已经是禁用状态，因此不做任何操作，程序继续执行。 
4. 恢复中断
恢复中断状态：根据保存的 intr_flag 状态，local_intr_restore(intr_flag) 确保在临界区外的代码中，中断的状态得以恢复。如果之前中断被禁用，那么它会被恢复；如果中断已关闭，则恢复过程不会做任何更改。 
总结
通过 local_intr_save(intr_flag) 和 local_intr_restore(intr_flag)，系统能够在临界区代码执行时确保中断不会被打断。具体来说，local_intr_save 会保存当前中断状态并禁用中断，执行临界区代码时中断不会干扰。local_intr_restore 会根据保存的状态恢复中断，确保程序在执行完关键操作后恢复正常的中断处理。这个机制通常用于需要保证原子性操作的场景，如在多线程编程或中断驱动的系统中，避免由于中断导致的数据不一致问题。
