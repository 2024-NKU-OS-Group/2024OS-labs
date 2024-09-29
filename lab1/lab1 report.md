# lab0.5

## Assembly Code Explanation

### Instructions from 0x1000 to 0x1010

0x1000: auipc t0,0x0  将当前 PC 加载到寄存器 t0
0x1004: addi a1,t0,32   将 t0 的值加上 32 存储到 a1 中
0x1008: csrr a0,mhartid  读取当前硬件线程的 ID 存储到 a0 中
0x100c: ld t0,24(t0)  从内存地址 t0+24 处加载 64 位数据到 t0 中
0x1010: jr t0  跳转到 t0 指定的地址执行

# lab1

##  练习1：理解内核启动中的程序入口操作

```

#include <mmu.h>
#include <memlayout.h>

    .section .text,"ax",%progbits
    .globl kern_entry
kern_entry:
    la sp, bootstacktop

    tail kern_init

.section .data
    # .align 2^12
    .align PGSHIFT
    .global bootstack
bootstack:
    .space KSTACKSIZE
    .global bootstacktop
bootstacktop:
```

上述是entry.S的代码，入口点kern_entry处的地址为0x80200000，对应的汇编指令如下：

```
0x80200000 <kern_entry> auipc sp,0x4 
0x80200004 <kern_entry+4> mv sp,sp 
0x80200008 <kern_entry+8> j 0x8020000c <kern_init>
```

la sp, bootstacktop对应auipc sp, 0x4，将sp变为0x80204000， tail kern_init对应j 0x802000c，跳转到kern_init部分代码

riscv说明书中有对tail的说明： tail symbol pc = &symbol; clobber x[6] 尾调用(Tail call). 伪指令(Pseudoinstuction), RV32I and RV64I. 设置 pc 为 symbol，同时覆写 x[6]。实际扩展为 auipc x6, offsetHi 和 jalr x0, offsetLo(x6)。

为什么bootstacktop所在位置是0x80204000？首先，我们得看包含的两个头文件mmu.h以及memlayout.h，它们的内容如下：

mmu.h

```
#ifndef __KERN_MM_MMU_H__
#define __KERN_MM_MMU_H__

#define PGSIZE          4096                    // bytes mapped by a page
#define PGSHIFT         12                      // log2(PGSIZE)

#endif /* !__KERN_MM_MMU_H__ */
```

memlayout.h

```
#ifndef __KERN_MM_MEMLAYOUT_H__
#define __KERN_MM_MEMLAYOUT_H__

#define KSTACKPAGE          2                           // # of pages in kernel stack
#define KSTACKSIZE          (KSTACKPAGE * PGSIZE)       // sizeof kernel stack

#endif /* !__KERN_MM_MEMLAYOUT_H__ */
```

从这两个头文件可知，PGSHIFT为12，KSTACKSIZE为8KB(对应十六进制2000) 回到entry.S，`.align PGSHIFT`意味着对齐设置为12，即对齐4KB(212212)的边缘，`.space KSTACKSIZE`则意味着开辟8KB的空间。但往最大可能说，就算对齐需要填充整整4KB空间，加上8KB的空间，也到达0x3000而不是0x4000，所以这个问题还没有解决。

接下来需要注意的是`.section`助记符，entry.S中有两个`.section`，一个是代码段(.text)，一个是数据段(.data)。通过`.section .text`提示汇编器和链接器，先在这一部分的位置填装满指令。因此，从0x80200000开始往后的大段内容都是指令，我们也可以看到，kern_init的位置正是0x8020000c，也就正好在跳转指令之后而已。再次观察程序，看到代码终结的位置大致在0x802010f8处，此时对齐4KB正好到0x80202000，再开辟8KB空间恰好是0x80204000。



## 练习2：完善中断处理 （需要编程）

其实lab1的文档已经描述得很详细了，具体代码也已经上传到gitee，这里总结一下思路： 总体分为三个阶段：

1. 每10ms触发一次时钟中断；
2. 每100次时钟中断，调用一次print_ticks()
3. 调用10次print_ticks后，调用sbi_shutdown()关机 后面两项十分简单，在处理程序中调用两个变量计数就行(ticks和num)，关键在于第一项，如何每10ms触发一次时钟中断？ `sbi_set_timer()`接口，可以传入一个时刻，让它在那个时刻触发一次时钟中断，但是我们需要的是“每10ms”——即时间间隔，而该接口只接受“时刻”。 解决这个问题需要`rdtime`伪指令，根据32位和64位的区别，将其封装在函数`get_cycles`中，通过其获得从运行开始到目前执行的时间。 因此，只需要sbi_set_timer(get_cycles+10ms)就能为下一个10ms设置中断了。将该段代码放在处理函数中，就能实现每次中断都为下一个10ms设置中断。 现在，只需要让“石头滚下”——设置第一个中断。在kern_init()中调用clock_init()，清空ticks并设置第一个时钟中断，这样就完成了任务。

根据这个分析具体编程如下：

```
case IRQ_S_TIMER:
    clock_set_next_event(); 
    ticks++;
    if (ticks % TICK_NUM == 0) {
        print_ticks();
        num++;
    }
    if(num == 10) {
        sbi_shutdown();
    }
     break;
```

在clock.c中封装着一个get_cycles函数，用来获取时间戳。然后在clock_init函数中需要首先将sie寄存器中的时钟使能信号打开，然后设置一个时钟中断信息，并设定timebase = 100000，对于QEMU，模拟出来CPU的主频是10MHz，每个时钟周期也就是100ns，达到timebase共需要10ms，即10ms触发一次时钟中断。每100次时钟中断打印一次信息，也就是每1s打印一次`100 ticks`。num用来标记打印的次数，当num达到10时，我们调用sbi_shutdown函数，实现关机。需要注意的是，我们需要“每隔若干时间就发生一次时钟中断”，但是OpenSBI提供的接口一次只能设置一个时钟中断事件。因此这里我们采用的方式是：一开始只设置一个时钟中断，之后每次发生时钟中断的时候，设置下一次的时钟中断，而设置下一次时钟中断的clock_set_next_event函数的主体内容是当timer的数值变为当前时间 + timebase 后，就会触发一次时钟中断。

## Challenge1: 描述与理解中断流程

### 一、处理中断异常的流程：

答：
1. 首先操作系统发现异常并发送给CPU，默认情况下，发生所有异常的时候，控制权都会被移交到 M 模式的异常处理程序然后进行处理，事实上M 模式的异常处理程序也可以将异常重新导向 S 模式，也支持通过异常委托机制选择性地将中断和同步异常直接交给 S 模式处理，而完全绕过 M 模式。
2. 接着CPU开始访问 `stvec`，也就是所谓的“中断向量表基址”，事实上是一个CSR（控制状态寄存器）。`stvec` 最低位的两位用来编码“模式”，如果是“00”就说明更高的 `SXLEN-2` 个二进制位存储的是唯一的中断处理程序的地址（`SXLEN` 是 `stval` 寄存器的位数），如果是“01”说明更高的 `SXLEN-2` 个二进制位存储的是中断向量表基址，通过不同的异常原因来索引中断向量表。在本实验采用 Direct 模式，也就是只有一个中断处理程序，PC 直接跳转到 `stvec` 中的地址，本实验中是 `__alltraps` 的入口地址。
3. 进入 `__alltraps` 后，会实现上下文的保存，以便后续的恢复。此时会定义一个 `trap_frame` 结构体，里面依存放着通用寄存器 `x0` 到 `x31`，然后是 4 个和中断相关的 CSR，然后通过汇编宏 `SAVE-ALL` 将这个结构体放入栈顶，即存储其中的内容，之后 `__alltraps` 使用指令 `move  a0, sp` 将刚刚保存上下文的中断栈帧的栈顶地址值赋给了函数参数，之后开始调用 `trap` 函数进行中断处理。
4. 在 `trap` 函数中把中断处理、异常处理的工作分发给了 `interrupt_handler()`，`exception_handler()`，这些函数再根据中断或异常的不同类型来处理。实际过程是在 `trap_dispatch()` 函数中，根据 `cause` 寄存器判断捕捉到的是中断还是异常，中断就调用 `interrupt_handler()`，异常就调用 `exception_handler()`。
5. 最后一切都完成后 `trap` 函数返回，再执行 `__trapret` 部分，内容就是调用 `RESTORE_ALL` 汇编宏恢复上下文，然后使用 `sret` 指令把 `sepc` 的值赋值给 `pc`，即回到原来的程序继续执行。

### 二、mov a0，sp的目的是什么？

答：
将刚刚保存上下文的中断栈帧的栈顶地址值赋给了 `a0` 寄存器，即参数寄存器，这样可以把中断帧的地址作为参数传给 `trap` 函数，使其能获得上下文。

### 三、SAVE_ALL中寄存器保存在栈中的位置是什么确定的？

答：
中断帧及其内部存放的各个寄存器通过栈顶寄存器 `sp` 来索引的。指令 `addi sp, sp, -36 * REGBYTES`，在内存中空出了给寄存器准备的栈空间，这样只要使用 `sp` 便能访问需要的上下文。

### 四、对于任何中断，__alltraps 中都需要保存所有寄存器吗？请说明理由。

答：
是的，我们的中断处理流程是固定的程序，即使有的中断可能并不一定刚需保存所有寄存器，但是为了中断处理的规范性和统一性，尽可能保存足够多的信息才是保证有效安全处理的最佳策略。

## Challenge2: 理解上下文切换机制

### 一、在trapentry.S中汇编代码 `csrw sscratch, sp`；`csrrw s0, sscratch, x0` 实现了什么操作，目的是什么？

答：
- `csrw sscratch, sp` 指令：把中断帧的栈顶指针 `sp` 赋值给 `sscratch` 特权寄存器。
- `csrrw s0, sscratch, x0` 指令：把 `sscratch` 寄存器里存储的栈顶指针的值写入 `s0` 寄存器，把零寄存器 `x0` 赋值给 `sscratch`，实现 `sscratch` 的清零。

目的：将 `sp` 的值临时保存在 `sscratch`，维护关键数据，并将其传给 `s0` 作为参数传给 `trap` 函数。

### 二、SAVE_ALL 里面保存了 stval scause 这些 csr，而在 RESTORE_ALL 里面却不还原它们？那这样 store 的意义何在呢？

答：
`stval` 和 `scause` 这两个 CSR 保存的是异常中断的来源信息，比如指令地址和原因之类。我们只在中断程序处理的过程中因为需要了解本次中断信息才访问这些 CSR。中断处理程序结束后，这些信息便不再有用，恢复上下文与其无关，之后可以直接覆盖这两个寄存器，不进行恢复更加方便。主要意义是临时保存中断处理需要的相关来源信息以便查询。


# Challenge3：完善异常中断

这段代码是一个异常处理器的一部分，用于处理两种不同类型的异常：**非法指令异常**和**断点异常**。这些异常发生在RISC-V架构中，代码对它们进行相应处理。

## 1. 处理非法指令异常 (`CAUSE_ILLEGAL_INSTRUCTION`):
- **异常触发的原因**: 
    - 当CPU遇到无法识别或不合法的指令时，会触发这个异常。

- **代码功能**:
    - (1) 第一步输出一条消息，表明发生了“非法指令（Illegal instruction）”异常。
    - (2) 第二步输出引发异常的指令地址。这里 `tf->epc` 表示异常发生时的程序计数器（EPC），即发生异常的指令的地址。
    - (3) 最后一步更新 `tf->epc` 寄存器的值，加上4来跳过当前的非法指令，继续执行下一条指令（假设当前指令是32位指令）。

    ```c
    case CAUSE_ILLEGAL_INSTRUCTION:
        // 非法指令异常处理
        /* LAB1 CHALLENGE3   YOUR CODE :  */
        /*(1) 输出指令异常类型（ Illegal instruction）
        *(2) 输出异常指令地址
        *(3) 更新 tf->epc 寄存器
        */
        cprintf("Exception type: Illegal instruction\n");
        cprintf("Illegal instruction caught at 0x%08x\n", tf->epc);
        tf->epc += 4;
        break;
    ```

## 2. 处理断点异常 (`CAUSE_BREAKPOINT`):
- **异常触发的原因**: 
    - 断点异常通常用于调试，当程序执行到 `ebreak` 指令时，会触发断点异常。

- **代码功能**:
    - (1) 第一步输出一条消息，表明发生了“断点（breakpoint）”异常。
    - (2) 第二步输出触发断点的指令地址，使用 `tf->epc` 显示发生异常的地址。
    - (3) 与非法指令处理类似，`tf->epc` 寄存器更新为加上4，以跳过 `ebreak` 指令并继续执行下一条指令。

    ```c
    case CAUSE_BREAKPOINT:
        // 断点异常处理
        /* LAB1 CHALLLENGE3   YOUR CODE :  */
        /*(1) 输出指令异常类型（breakpoint）
        *(2) 输出异常指令地址
        *(3) 更新 tf->epc 寄存器
        */
        cprintf("Exception type: breakpoint\n");
        cprintf("ebreak caught at 0x%08x\n", tf->epc);
        tf->epc += 4; 
        break;
    ```

### 具体说明:
- **`cprintf()`**: 一个格式化输出函数，可能用于向控制台输出调试信息。
- **`tf->epc`**: 程序计数器寄存器（EPC，Exception Program Counter），保存了发生异常时的指令地址。
- **`tf->epc += 4;`**: 该语句将EPC的值增加4字节，以跳过当前的指令并继续执行后面的指令（因为RISC-V中的标准指令长度为32位或4个字节）。

