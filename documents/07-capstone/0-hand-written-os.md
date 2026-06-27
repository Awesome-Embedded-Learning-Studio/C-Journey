---
title: "自制操作系统：从按电源到内核 main 的引导链实战"
description: "阶段 7 综合收官。把前面所有阶段攒的编译、内存、位运算、裸机知识熔进一条真实的 x86 引导链——BIOS → MBR → loader → 保护模式 → GDT → 内核入口，全程真实反汇编与 hex 实证。"
chapter: 7
order: 0
tags:
  - host
  - os
  - asm
  - boot
  - memory
  - bit-manipulation
  - linker
  - advanced
difficulty: advanced
reading_time_minutes: 28
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0：编译流程、链接与 GDB"
  - "Chapter 2：指针、内存布局与位运算"
  - "Chapter 6：裸机 C（8051）"
related:
  - "从零写一个操作系统：通电到内核的接力赛（进阶导览）"
  - "projects/os-from-scratch"
---

# 自制操作系统：从按电源到内核 main 的引导链实战

## 引言：为什么一定要亲手搓一个 OS

说实话，学计算机这一路上最让人上头的事，不是刷了多少算法题，而是有一天你按下电源，看着一块硬盘老老实实跑起你自己写的汇编，屏幕上蹦出你亲手写进去的字符——那种"我真正让硬件听我话了"的快感，写多少业务代码都换不来。这一章是 ROADMAP 的**收官阶段**，我们不再单独学某个 C 语言特性，而是把前面攒下的所有家当熔进一条真实的 x86 引导链：编译器怎么把我们写的汇编和 C 变成能跑的机器码（阶段 0）、内存地址到底是怎么一回事（阶段 2）、位运算怎么拼出一段描述符（阶段 2）、裸机下没有 OS 帮你兜底是什么感觉（阶段 6）——全在这一条链里。

这一章我们只盯一个目标：**搞清楚从按下电源到 CPU 第一次执行你的 `main`，中间到底发生了什么、每一棒谁交接给谁**。配套的真实工程就在 [projects/os-from-scratch](../../projects/os-from-scratch/)，里面 `1/` 到 `20/` 是按课次一点点长出来的实验代码，`final/` 是阶段性完整版。进阶导览（[通电到内核的接力赛](../advanced/0-os-from-scratch.md)）已经把整条链的鸟瞰讲了一遍，这一章要做的是把每一棒的**机制**掰开揉碎，并且——老规矩——能本机实测的全部用真实反汇编和 hex 给你钉死，需要 bochs/qemu 才能跑通的部分，我们老实标注，绝不编造启动日志。

> 先交底：MBR 那段我们用 GNU `as`（AT&T 语法）现编一个 512 字节引导扇区，`objdump` 反汇编、`xxd` 看魔数，全是本机 `/tmp` 里实跑出来的；GDT 和保护模式切换那一段同样用 `as` 汇编后反汇编，每一字节都对得上配套工程的 `boot.inc`；内核入口的链接地址也用真实的 `ld -Ttext` 加 `readelf` 验证。凡是真正"开机启动"才看得到的效果（屏幕上真的出现 `MBR`、真的切进保护模式、真的跑起 shell），都明确标注"需 bochs/qemu + 完整工程"，这部分请去 [projects/os-from-scratch](../../projects/os-from-scratch/) 里 `make` + `dd` 写镜像再启动。

## 引导链总览：一场四棒接力

在桌面编程里，"按下电源到我的代码开始跑"中间这段路被操作系统和 BIOS 藏得严严实实；可一旦你要自己写 OS，这条路就得你亲手走完。我们可以把整件事看成一场四棒接力：

```text
通电 → BIOS 自检 → BIOS 把硬盘第一扇区(MBR)读到 0x7c00 并跳过去
                 ↓ 第 1 棒交接
              MBR(你写的)：初始化段、显存直写、从硬盘读 loader 进内存、跳过去
                 ↓ 第 2 棒交接
              loader(你写的)：探内存、进保护模式、建 GDT、读 kernel 进内存、跳过去
                 ↓ 第 3 棒交接
              kernel(你写的)：main 开始跑，OS 正式上线
```

这里最关键的一句是：**BIOS 只帮你跑第一棒**。它干完自检，会去检查硬盘的第一个扇区（512 字节，叫 MBR 主引导记录），如果这个扇区的最后两字节是魔数 `0x55 0xaa`，BIOS 就认定它"可引导"，把它原样读到内存的 `0x7c00` 位置然后跳过去——剩下从第二棒开始的全是你自己的事。所以你的第一段代码，就是塞在这 512 字节里的 MBR，它的唯一使命就是**把 loader 读进来再跳过去**。

### 一个绕不开的问题：为什么偏偏是 0x7c00

第一次写 OS 的人都会问：BIOS 凭什么把 MBR 读到 `0x7c00` 这个怪地址？答案是历史包袱。早期的 8086 机器内存小得可怜，DOS 1.0 时代最小配置是 32KB 内存（`0x8000`）。BIOS 的设计者要给 MBR 找个落脚点，又怕它被过早覆盖（MBR 干完引导就该退场，腾出内存给后续代码），就把它放在了 32KB 的末尾再减去它自己的 512 字节：`0x8000 - 0x0400 = 0x7c00`。这个数字就这么从 1981 年传了下来，成了所有 x86 机器的约定。你今天在任何一台 PC 上写 OS，MBR 还是会被读到 `0x7c00`，谁也改不了——这就是"硬件和软件之间的暗号"在现实里最顽固的样子。

> 复习一下作者笔记里关于地址的小结：8086 有 20 根地址线，能寻址 1MB（`0x00000`～`0xFFFFF`）。低端 `0x00000`～`0x9FFFF` 是 DRAM，顶端 `0xF0000`～`0xFFFFF` 这段映射的是烧在主板 ROM 里的 BIOS 代码。实模式下只能摸到这 1MB，不是 CPU 不想多摸，是地址总线就那么宽——这也是为什么必须切到保护模式才能用上全部内存。

## MBR：512 字节里塞下一个世界

### MBR 的三要素

我们来看一份能本机汇编、本机反汇编的最小 MBR。它干三件事，正好对应 MBR 的三个要素：`vstart=0x7c00` 告诉汇编器"我一定会被加载到 0x7c00"、往 `0xb800` 显存直写字符让"MBR 跑起来了"肉眼可见、结尾两字节必须是 `0x55 0xaa` 魔数。下面这份用 GNU `as` 的 AT&T 语法写（项目原始代码是 NASM 的 Intel 语法，效果完全等价，我们在注释里标了语法）：

```asm
/* Minimal MBR boot sector, 16-bit real mode, GNU as (AT&T syntax). */
    .code16
    .section .text
    .globl _start
_start:
    xorw    %ax, %ax            /* ax = 0 (CS is already 0) */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    %ax, %fs
    movw    $0x7c00, %sp        /* stack grows down from load address */

    /* gs -> 0xb800. In text mode, physical 0xb8000 IS the screen. */
    movw    $0xb800, %ax
    movw    %ax, %gs

    /* Top-left cell: 'M' 'B' 'R', each with attr 0xA4
     * (0xA = red background, 4 = white foreground). */
    movw    $0xa44d, %ax        /* attr<<8 | 'M' */
    movw    %ax, %gs:0
    movw    $0xa442, %ax        /* 'B' */
    movw    %ax, %gs:2
    movw    $0xa452, %ax        /* 'R' */
    movw    %ax, %gs:4

hang:
    hlt
    jmp     hang

    /* Pad the sector out to 510 bytes, then the boot magic. */
    . = _start + 510
    .byte   0x55
    .byte   0xaa
```

看到 `0xb800` 这个魔法数字了吗？在 VGA 文本模式下，**内存的 `0xb800` 段就是屏幕本身**——你往这里写一个字符字节加一个属性字节，屏幕上立刻显示出来，没有任何 API、没有任何驱动，内存即显示。这就是裸机编程的极致：没有抽象，你直接摸硬件。属性字节 `0xA4` 拆开看，高 4 位 `0xA` 是背景色（红），低 4 位 `4` 是前景色（白），这种"一个字节里塞两种信息"的位运算把戏，正是阶段 2 练过的手艺在底层最朴素的应用。

### 真实汇编 + 反汇编 + 魔数实证

我们现在本机汇编这份 MBR，把它做成一个扁平的 512 字节二进制（这就是会被 `dd` 写进镜像、被 BIOS 读走的那个东西），然后用 `objdump` 反汇编、`xxd` 看魔数。下面这些输出全是 `/tmp` 里实跑出来的，没有任何粉饰：

```text
$ as --32 -o mbr.o mbr.s && objcopy -O binary -j .text mbr.o mbr.bin
$ wc -c mbr.bin
512 mbr.bin

$ xxd mbr.bin | tail -2      # 看最后 32 字节，魔数在 0x1fe
000001e0: 0000 0000 0000 0000 0000 0000 0000 0000  ................
000001f0: 0000 0000 0000 0000 0000 0000 0000 55aa  ..............U.

$ xxd mbr.bin | head -3      # 看开头真实代码字节
00000000: 31c0 8ed8 8ec0 8ed0 8ee0 bc00 7cb8 00b8  1...........|...
00000010: 8ee8 b84d a465 a300 00b8 42a4 65a3 0200  ...M.e....B.e...
00000020: b852 a465 a304 00f4 ebfd 0000 0000 0000  .R.e............
```

`55aa` 就安安静静躺在偏移 `0x1fe`（十进制 510）的位置——这正是 BIOS 检查的"可引导扇区"暗号。少了它，BIOS 根本不跳过来执行你的 MBR，你会收获一个 `No bootable device` 然后对着黑屏怀疑人生。再用 `objdump` 把这个扁平二进制按 16 位实模式反汇编，看看汇编器到底吐出了什么机器码：

```text
$ objdump -D -b binary -m i8086 -M addr16,data16 mbr.bin
   0:   31 c0                 xor    %ax,%ax
   2:   8e d8                 mov    %ax,%ds
   4:   8e c0                 mov    %ax,%es
   6:   8e d0                 mov    %ax,%ss
   8:   8e e0                 mov    %ax,%fs
   a:   bc 00 7c              mov    $0x7c00,%sp
   d:   b8 00 b8              mov    $0xb800,%ax
  10:   8e e8                 mov    %ax,%gs
  12:   b8 4d a4              mov    $0xa44d,%ax
  15:   65 a3 00 00           mov    %ax,%gs:0x0
  19:   b8 42 a4              mov    $0xa442,%ax
  1c:   65 a3 02 00           mov    %ax,%gs:0x2
  20:   b8 52 a4              mov    $0xa452,%ax
  23:   65 a3 04 00           mov    %ax,%gs:0x4
  27:   f4                    hlt
  28:   eb fd                 jmp    0x27
 ...
 1fe:   55                    push   %bp
 1ff:   aa                    stos   %al,%es:(%di)
```

你会发现一个有意思的细节：`mov %ax,%gs:0x0` 这条访存指令前面有一个 `65` 字节——那是**段超越前缀**（segment override prefix），告诉 CPU"这次访问用 `gs` 而不是默认的 `ds`"。回看作者笔记里对指令格式的拆解——"前缀 + 操作码 + 寻址方式 + 操作数 + 偏移量"，这里的 `65` 就是那个前缀，活生生的真实例子。而最后 `55 aa` 被反汇编器硬生生解释成了 `push %bp` 和 `stos`——这是对的，因为它俩本来就是合法指令的字节，BIOS 才不管你这里是不是代码，它只认这两个魔数字节。

### MBR 还得读硬盘：loader 在第二扇区

光是显存直写 `MBR` 三个字，OS 还远着呢。真正的 MBR 干完"亮个相"，下一件事是从硬盘把 loader 读进内存再跳过去。配套工程的 [final/boot/mbr.S](../../projects/os-from-scratch/final/boot/mbr.S) 里这段是这么写的（关键片段）：

```asm
   mov eax, LOADER_START_SECTOR    ; 起始 LBA 地址（boot.inc 里 = 0x2，即第 2 扇区）
   mov bx,   LOADER_BASE_ADDR      ; 写入内存的地址（boot.inc 里 = 0x900）
   mov cx,   4                     ; 读入的扇区数
   call rd_disk_m_16               ; 下面那个读硬盘的子程序
   jmp LOADER_BASE_ADDR + 0x300    ; 跳到 loader 的入口
```

`rd_disk_m_16` 这个子程序干的是最朴素的活：往硬盘控制器的端口（`0x1f2`～`0x1f7`）一字节一字节地写命令——要读几个扇区、LBA 地址是多少、发个读命令 `0x20`、轮询状态位看硬盘忙不忙、最后从 `0x1f0` 数据端口一个字一个字地把数据搬进内存。这套"往端口写字节"的 IO 编程，本质上和阶段 6 在 8051 上写寄存器是一回事：CPU 通过 IO 接口跟外设通信，外设按你写进去的命令干活。

> 这里有个真坑（笔记里专门提醒过）：读硬盘那段循环里，缓冲区指针本来用的是 `bx`，可 `bx` 是 16 位的，最大只能指到 `0xffff`。一旦读的扇区数多了、写回的地址超过 `0xffff`，`bx` 就会回卷到 0，把你的栈给覆盖了，`ret` 时返回地址已经被破坏，程序飞到不知道哪里去。所以工程里改成了 `ebx`（32 位），汇编器会自动在指令前面加上 `0x66`/`0x67` 前缀，把操作数大小和寻址方式临时反转成 32 位。这就是笔记里讲的"运行模式反转"在真实工程里的救命应用。

## loader：干最重的活，把 CPU 拽进保护模式

接力棒交到 loader 手里，它的担子比 MBR 重多了。loader 要做四件大事：用 BIOS 中断 `int 0x15` 的 `0xe820` 子功能探测物理内存有多大、打开 A20 地址线、建好 GDT 并 `lgdt` 加载、置 CR0 的 PE 位切进保护模式，最后还要把 kernel.bin 从硬盘读进来、按 ELF 程序头把各个段搬到正确位置、跳进内核入口。我们重点拆"进保护模式"这一棒——它是整个引导链最硬、也最能体现"位运算拼描述符"的环节。

### 实模式 vs 保护模式：为什么要切

实模式是 8086 留下的遗产：段寄存器里存的是段基址，物理地址 = 段基址 ×16 + 段内偏移，最大只能摸到 1MB 内存，而且**没有任何内存保护**——程序写到哪都行，写到别人地盘也没人拦。这对一个现代 OS 来说显然不够用。保护模式（Protected Mode）的"保护"体现在：段寄存器里存的不再是基址，而是一个**选择子**（selector），它指向一张**全局描述符表**（GDT）里的某一项描述符，描述符里写明了这段内存的基址、界限、权限——CPU 在每次访存时都会检查"你有没有越界、有没有权限"，越界就抛异常，这就是保护。

### 实模式寻址：段 ×16 + 偏移的来历

在切保护模式之前，得先吃透实模式是怎么寻址的。8086 的寄存器是 16 位的，最大只能表示 `0xffff`（64KB），可地址线有 20 根、能摸到 1MB。怎么用 16 位寄存器表示 20 位地址？Intel 的工程师想了个折中：用**两个** 16 位寄存器组合——一个段寄存器存"段基址"，一个偏移寄存器存"段内偏移"，物理地址 = 段基址 × 16 + 段内偏移。

这里的"×16"在二进制里就是左移 4 位。比如 `gs = 0xb800`，那 `gs:0` 的物理地址是 `0xb800 × 16 + 0 = 0xb8000`——这正是 VGA 文本模式显存的物理起始地址。所以前文 MBR 里 `movw $0xb800, %ax` 再 `movw %ax, %gs`，本质就是让 `gs` 指向显存，之后 `gs:0`、`gs:2` 这些访问全部落在 `0xb8000` 往后的屏幕单元上。理解了"段 ×16 + 偏移"，那串魔法数字 `0xb800` 就再也不是死记的常数，而是有清晰来历的寻址结果。

> 笔记里有个有趣的细节：实模式下如果算出来的地址超过 20 位（比如段基址和偏移都拉满），8086 会做**回卷**（wrap-around），就像循环队列一样绕回低端。后来的 CPU 为了兼容这些靠回卷技巧跑起来的老程序，专门搞了个 A20 地址线门——这就是 loader 进保护模式前要"打开 A20"的历史原因。

### GDT：用位运算拼出一段内存的"身份证"

GDT 里的每一项叫**段描述符**，64 位（8 字节），里面把基址、界限、一堆属性位揉成一团。它的位布局相当反人类——基址和界限都不是连续的，被拆成了好几段塞在不同位置：

```text
字节布局（一个描述符 8 字节）：
  段界限 [15:0]   ── 字节 0,1
  段基址 [15:0]   ── 字节 2,3,4
  属性位          ── 字节 5,6（高 4 位里还混着段界限[19:16]）
  段基址 [31:24]  ── 字节 7
```

属性位里塞着一堆字段：`G`（粒度，1=4KB，0=1 字节）、`D`（操作数大小，1=32 位）、`P`（段在不在内存）、`DPL`（特权级，2 位）、`S`（系统段还是数据/代码段）、`TYPE`（4 位，代码段是 XCRA=可执行/一致性/可读/已访问，数据段是 XEWA=可执行/扩展方向/可写/已访问）。配套工程的 [final/boot/include/boot.inc](../../projects/os-from-scratch/final/boot/include/boot.inc) 把每个属性位都定义成一个常量，再把它们或（`+`）在一起拼出高 4 字节：

```asm
DESC_TYPE_CODE  equ  1000_00000000b   ; x=1,c=0,r=0,a=0：可执行、非一致、不可读
DESC_TYPE_DATA  equ  0010_00000000b   ; x=0,e=0,w=1,a=0：不可执行、向上扩展、可写
DESC_CODE_HIGH4 equ  (0x00<<24)+DESC_G_4K+DESC_D_32+DESC_LIMIT_CODE2
                   +DESC_P+DESC_DPL_0+DESC_S_CODE+DESC_TYPE_CODE
DESC_DATA_HIGH4 equ  (0x00<<24)+DESC_G_4K+DESC_D_32+DESC_LIMIT_DATA2
                   +DESC_P+DESC_DPL_0+DESC_S_DATA+DESC_TYPE_DATA
```

这就是阶段 2 位运算的"实战大考"：每一个属性都是一个二进制位，多个属性用按位或拼到一个 32 位整数里，最终 `DESC_CODE_HIGH4` 算出来是 `0x00cf9800`。我们可以本机验证这个值到底对不对——用 Python 把 `boot.inc` 的常量原样翻译一遍：

```text
DESC_CODE_HIGH4 = DESC_G_4K + DESC_D_32 + DESC_LIMIT_CODE2 + DESC_P
                + DESC_DPL_0 + DESC_S_CODE + DESC_TYPE_CODE
              = 0x00cf9800
```

### 选择子：段寄存器里装的新东西

描述符建好以后，保护模式下的段寄存器（CS、DS、SS、GS 等）装的就不再是基址了，而是一个 16 位的**选择子**（selector）。它的位布局是：

```text
  bit 15..................3   bit 2    bit 1..0
  ┌─────────────────────────┬────────┬──────────┐
  │   在 GDT 里的索引(13位)  │   TI   │   RPL    │
  └─────────────────────────┴────────┴──────────┘
```

- **高 13 位**是描述符在 GDT 里的索引（13 位正好能编址 8192 项，对应 GDT 最多 8192 个描述符）。
- **TI 位**（Table Indicator）决定去 GDT 还是 LDT 找，0=GDT。
- **低 2 位 RPL**（Requested Privilege Level）是请求特权级。

回头看代码里 `SELECTOR_CODE equ (0x0001<<3) + TI_GDT + RPL0`——`0x0001<<3` 就是把索引 1 左移 3 位（跨过低 3 位），加上 `TI_GDT=0` 和 `RPL0=0`，结果是 `0x08`。这正是前文 `ljmp $0x08, $p_mode_start` 里那个 `0x08` 的来历：索引 1、TI=0、RPL=0，指向 GDT 里第二项（索引 0 是 null 描述符），也就是代码段描述符。`0x10` 是索引 2（数据段）、`0x18` 是索引 3（显存段），全都是 `索引 << 3` 这个规律。理解了选择子的位布局，GDT 和段寄存器之间那根线就彻底通了。

### 真实汇编 + 反汇编：lgdt / cr0 / 远跳

现在我们把"打开 A20 → 加载 GDT → 置 CR0.PE → 远跳刷新流水线"这四步汇编出来（GNU `as`，AT&T 语法），代码段的描述符严格用上面算出来的 `0x00cf98000000ffff`（高 4 字节正是 `DESC_CODE_HIGH4`），然后反汇编给你看每一字节：

```asm
/* GDT + protected-mode entry, GNU as (AT&T syntax), 32-bit. */
    .code32
pmode_entry:
    /* (1) A20 via fast gate (port 0x92, bit 1) */
    inb     $0x92, %al
    orb     $0x02, %al
    outb    %al, $0x92
    /* (2) lgdt: 16-bit limit then 32-bit base */
    lgdt    gdt_ptr
    /* (3) CR0.PE = 1 -> protected mode */
    movl    %cr0, %eax
    orl     $1, %eax
    movl    %eax, %cr0
    /* (4) far jump to selector 0x08 to flush prefetch / reload CS */
    ljmp    $0x08, $p_mode_start

p_mode_start:
    movw    $0x10, %ax          /* SELECTOR_DATA  = 0x10 */
    movw    %ax, %ds
    movw    %ax, %es
    movw    %ax, %ss
    movw    $0x18, %ax          /* SELECTOR_VIDEO = 0x18 */
    movw    %ax, %gs

    .section .data
    .align 8
gdt_base:
    .quad   0x0000000000000000      /* 0x00 null descriptor        */
    .quad   0x00cf98000000ffff      /* 0x08 flat code, boot.inc    */
    .quad   0x00cf92000000ffff      /* 0x10 flat data              */
    .quad   0x00cf9200b8000007      /* 0x18 video @0xb8000         */
gdt_ptr:
    .word   gdt_ptr - gdt_base - 1  /* limit = size - 1            */
    .long   gdt_base
```

本机汇编后反汇编，A20 → `lgdt` → CR0 → 远跳这套动作的机器码清清楚楚：

```text
$ as --32 -o pm.o pm.s
$ objdump -d pm.o
00000000 <pmode_entry>:
   0:   e4 92                 in     $0x92,%al
   2:   0c 02                 or     $0x2,%al
   4:   e6 92                 out    %al,$0x92
   6:   0f 01 15 20 00 00 00  lgdtl  0x20
   d:   0f 20 c0              mov    %cr0,%eax
  10:   83 c8 01              or     $0x1,%eax
  13:   0f 22 c0              mov    %eax,%cr0
  16:   ea 1d 00 00 00 08 00  ljmp   $0x8,$0x1d
0000001d <p_mode_start>:
  1d:   66 b8 10 00           mov    $0x10,%ax
  21:   8e d8                 mov    %eax,%ds
  ...
```

我们再看 GDT 那几个描述符在 `.data` 里到底落成了什么字节，关键是代码段描述符的高位 `0098cf00`——注意 x86 是小端序，所以 `0x00cf9800` 在内存里是 `00 98 cf 00`，跟 `DESC_CODE_HIGH4` 完全对得上：

```text
$ objdump -s -j .data pm.o
 0000 00000000 00000000 ffff0000 0098cf00  ................   <- null, code
 0010 ffff0000 0092cf00 070000b8 0092cf00  ................   <- data, video
 0020 1f000000 0000                        ......           <- gdt_ptr
```

这里每一行都经得起推敲：`gdt_ptr` 的 limit 是 `0x1f`（31），正好是 4 个描述符 × 8 字节 − 1；代码段 `0098cf00` 对应 `DESC_CODE_HIGH4`；显存段低位 `070000b8` 里那个 `b8` 正是显存基址 `0xb8000` 的痕迹。loader 在实模式里做完这四步，CPU 就从 16 位实模式切进了 32 位保护模式，接下来才有资格去加载内核。

### 远跳为什么不能省

笔记里专门讲了，置完 CR0.PE 之后那个 `ljmp $0x08, $p_mode_start` 不是多余的动作。CPU 的流水线里可能还预取着实模式的指令，这时候你直接往下跑，可能撞上还没刷新的旧指令。一个远跳转强制 CPU 重新从新的代码选择子取指，顺便把 CS 寄存器真正装上选择子 `0x08`（保护模式下 CS 装的是选择子不是基址）——这一步是把"模式切换"落到实处。配套工程的 [loader.S](../../projects/os-from-scratch/final/boot/loader.S) 里那句 `jmp dword SELECTOR_CODE:p_mode_start` 干的就是同一件事，注释还顺手吐槽了分支预测。

## 进入内核：文件头 + 文件体的灵活加载

### 为什么不再用"约定死地址"

到这一步我们会发现一个规律：MBR 被 BIOS 加载到写死的 `0x7c00`、loader 被 MBR 读到写死的 `0x900`、kernel 入口又是个写死的 `0xc0001500`——调用方和被调用方都得提前约定好"你在哪个地址"。这种方式很死板：换个程序就得改地址。作者笔记里那段反思说得很到位：**文件头 + 文件体**的加载方式并不是不需要入口地址，而是把入口地址等元信息塞进一个统一的、约定好格式的"文件头"里（像书的目录），任何可执行文件都按这个格式去找入口，比写死常数灵活得多。这个"格式"在 Linux 下就是 ELF。

### 真实链接：把内核入口钉到 0xc0001500

我们本机走一遍内核的编译链接，验证入口地址确实被重定位到了 `boot.inc` 里的 `KERNEL_ENTRY_POINT`。先写一个最小的内核入口（`_start`，因为 `ld` 默认找 `_start`）：

```c
/* kmain.c — 最小内核入口，跟笔记里第一个 kernel main 同构 */
int _start(void) {
    /* kernel entry stub — just a busy loop */
    while (1) { }
    return 0;
}
```

本机编译、链接、查入口，每一步都是真实输出：

```text
$ gcc -c -m32 -ffreestanding -fno-pic -o kmain.o kmain.c
$ file kmain.o
kmain.o: ELF 32-bit LSB relocatable, Intel i386, version 1 (SYSV), not stripped

$ nm kmain.o            # 链接前，_start 还在地址 0（没重定位）
00000000 T _start

$ ld -m elf_i386 -Ttext 0xc0001500 -e _start -o kernel.bin kmain.o
$ readelf -h kernel.bin | grep -i entry
  Entry point address:               0xc0001500
```

看清楚了：链接前 `_start` 趴在地址 0（可重定位目标文件的常态，符号还没排位置），链接时我们用 `-Ttext 0xc0001500` 把整个 `.text` 段搬到了 `0xc0001500`，`readelf` 报出的入口点正是这个值——和 `boot.inc` 的 `KERNEL_ENTRY_POINT equ 0xc0001500` 一字不差。loader 那边 `jmp KERNEL_ENTRY_POINT` 跳过来，正好落在你的 `_start`/`main` 上，OS 就此上线。

> 笔记里有个真实的小插曲值得提：如果你不给 `-e` 指定入口，`ld` 会警告"找不到 `_start`，缺省入口为 0xc0001500"。把函数名从 `main` 改成 `_start`，警告就消失了——这就是为什么内核入口习惯叫 `_start` 而不是 `main`：链接器的默认入口符号就是它。

### loader 怎么把 ELF 搬进内存

loader 把整个 `kernel.bin` 从硬盘一股脑读到 `KERNEL_BIN_BASE_ADDR`（`0x70000`），但 ELF 可执行文件不是"从开头直接跑"——它有程序头表（program header table），每项描述一个段（segment）该被搬到哪个虚拟地址、多大。配套工程的 `kernel_init` 子程序就是按 ELF 程序头逐段 `memcpy` 到指定位置（`p_vaddr`），把可执行文件"摊开"成内存里能跑的样子。

这里值得把"节"和"段"分清楚（笔记里花了大篇幅讲）：**节**（section）是给链接器和程序员看的——`.text` 放机器码、`.rodata` 放只读数据、`.data` 放已初始化全局变量、`.bss` 放未初始化全局变量（不占磁盘空间，只是占位符）。**段**（segment）才是程序真正执行时加载到内存的单位——链接器把多个相关的节合并成段，ELF 的程序头表描述的就是这些段该放哪、多大。所以可重定位目标文件（`gcc -c` 产物）只有节、没有段；经过 `ld` 链接后的可执行文件才有完整的程序头表。本机的 `readelf -S kernel.bin` 看节、`readelf -l kernel.bin` 看段，两个命令一对照，节怎么合并成段就一目了然。

`kernel_init` 干的活其实不复杂：读出 ELF 头里的 `e_phoff`（第一个程序头在哪）、`e_phentsize`（每个程序头多大）、`e_phnum`（有几个程序头），然后遍历每个程序头，只要它的 `p_type` 不是 `PT_NULL`，就把 `p_offset` 处、`p_filesz` 大小的数据搬到 `p_vaddr`。这个"按表逐项搬运"的过程，本质上就是在执行链接器算好的内存布局——和我们本机 `readelf -l` 看到的那张表是一一对应的。

## 裸机调试：Bochs 调试器速查

写 OS 时没有桌面 `gdb` 帮你兜底，代码在 Bochs 里跑，调试全靠 Bochs 自带的调试器。笔记里整理了一份速查表，挑最常用的几类列在这里，省得你第一次进去两眼一抹黑：

| 类别 | 命令 | 作用 |
|---|---|---|
| 执行控制 | `s` / `n` | 单步执行（`s` 进函数，`n` 不进） |
| 执行控制 | `c` | 继续运行到下一个断点 |
| 断点 | `b addr` | 在物理地址下断点 |
| 断点 | `vb seg:off` | 在虚拟地址下断点（保护模式常用） |
| 断点 | `info b` | 列出所有断点 |
| 寄存器/内存 | `info reg` | 查看所有通用寄存器 |
| 寄存器/内存 | `r` | 查看段寄存器 |
| 寄存器/内存 | `xp /Nwx addr` | 查看物理地址处 N 个字 |
| 寄存器/内存 | `x /Nwx addr` | 查看线性地址处 N 个字 |
| 寄存器/内存 | `page addr` | 查某个线性地址的页表映射（开了分页后必备） |

最常用的套路是：`vb 0x7c00` 在 MBR 入口下断点，`c` 跑过去，然后 `s` 单步看 BIOS 把控制权交给你之后到底执行了什么、寄存器是什么状态。开分页之后想确认某个虚拟地址映射到哪，`page 0xc0001500` 一查就清楚。这套调试习惯和桌面 `gdb` 完全是两个世界，提前知道有这些命令，能省下大量对着黑屏发呆的时间。

## 对接 projects/os-from-scratch

这份引导链不是纸上谈兵，它的完整实现就在 [projects/os-from-scratch](../../projects/os-from-scratch/)。读这份工程最舒服的方式是按课次走：`1/` 的 MBR 先在屏幕上蹦出字符，`2/` 的 `pmtest.asm` 第一次切进保护模式，一路推进到 `final/` 里带中断、内存、线程、文件系统、shell 的完整内核。本机能验证的反汇编/链接都验过了，下面这些是**需要 bochs/qemu + 完整工程**才能看到的真实启动效果，老实标注：

- **MBR 真的显示出 "1 MBR"**：要 `nasm`/`as` 汇编后 `dd` 写进镜像，Bochs 里启动。配套工程 `1/`、`final/` 各目录都有 `bochsrc`。
- **loader 真的切进保护模式并打印字符**：`pmtest.asm`（NASM）或本机的 pm.s（AT&T）汇编成镜像后启动。
- **内核真的跑起 shell**：`final/` 的 `make` + `dd` 写镜像 + Bochs 启动，会看到 `[CharlieChen114514@localhost /]$` 提示符。

想深入每一棒的真实代码，按这个顺序读最顺：

| 文件 | 这一棒在干什么 |
|---|---|
| [final/boot/mbr.S](../../projects/os-from-scratch/final/boot/mbr.S) | 16 位实模式：初始化段、显存直写、读 loader |
| [final/boot/include/boot.inc](../../projects/os-from-scratch/final/boot/include/boot.inc) | 所有 GDT 描述符属性常量、地址约定的单一真相源 |
| [final/boot/loader.S](../../projects/os-from-scratch/final/boot/loader.S) | 探内存、进保护模式、建 GDT、读 kernel、跳入口 |
| [final/kernel/main.c](../../projects/os-from-scratch/final/kernel/main.c) | 内核 `main`，`init_all` 拉起所有子系统 |

## 常见踩坑

> **坑 1：`0x55 0xaa` 魔数漏了或写错位置。** 本机 `xxd` 一下，魔数必须在偏移 `0x1fe`（510）。写错位置或忘了，BIOS 直接 `No bootable device`，你盯着黑屏怀疑人生。这是我们本机最容易先验的一项。

> **坑 2：进保护模式的顺序乱了。** 正确顺序是"开 A20 → `lgdt` 加载 GDT → 置 CR0.PE → 远跳刷新"。GDT 没 `lgdt` 就开 PE，或者没远跳就往下跑，CPU 立刻给你一个三重错误重启。本机反汇编能确认顺序，但跑起来要 Bochs。

> **坑 3：代码段描述符的 type 位算错。** `DESC_TYPE_CODE` 是 `1000`（不可读），数据段是 `0010`（可写）。这些 4 位 type 拼错，段加载时 CPU 检查权限就抛异常。本机用 Python 把 `boot.inc` 常量算一遍、和反汇编的描述符字节对一遍，能提前逮住。

> **坑 4：实模式下的 16 位指针回卷。** 读硬盘用 `bx` 当缓冲区指针，地址超过 `0xffff` 就回卷覆盖栈，`ret` 飞掉。工程里改用 `ebx`，靠 `0x66`/`0x67` 前缀临时反转成 32 位寻址——这是笔记里"运行模式反转"的真实救命场景。

> **坑 5：裸机没有桌面调试器。** 桌面上 `gdb` 随便用，写 OS 时代码在 Bochs 里跑，得用 Bochs 自带调试器（`b` 下断点、`info reg` 看寄存器、`page` 看页表），习惯完全不一样。笔记里专门列了 bochs 调试指令速查表。

## 小结

- [ ] 引导链是四棒接力：BIOS → MBR(0x7c00) → loader(0x900) → kernel main，从第二棒起全是你写的
- [ ] MBR 三要素：`vstart=0x7c00`、`0xb800` 显存直写、`0x55 0xaa` 魔数（本机 xxd/objdump 实证）
- [ ] 保护模式切换四步：开 A20 → `lgdt` → CR0.PE=1 → 远跳刷新（本机反汇编实证）
- [ ] GDT 描述符是位运算大考：属性位用按位或拼出 `DESC_CODE_HIGH4 = 0x00cf9800`（Python 复算 + 反汇编字节对齐）
- [ ] 内核入口靠 ELF 文件头灵活定位：`ld -Ttext 0xc0001500` 把入口钉到 `KERNEL_ENTRY_POINT`（readelf 实证）
- [ ] 真正的启动效果（屏幕字符、保护模式打印、shell）需 bochs/qemu + 完整工程，见 projects/os-from-scratch

## 练习

- [ ] 本机汇编本文那份最小 MBR，用 `xxd` 确认魔数在 `0x1fe`、用 `objdump -b binary -m i8086` 看清每条指令的机器码（无需 Bochs，纯本机验证）
- [ ] 用 Python 把 `final/boot/include/boot.inc` 的 `DESC_CODE_HIGH4`/`DESC_DATA_HIGH4` 原样算出来，和本文反汇编的描述符字节逐位对照
- [ ] 本机写一个最小 `_start`，用 `ld -Ttext 0xc0001500 -e _start` 链接，`readelf -h` 确认入口在 `0xc0001500`；试一次不加 `-e`，观察 `ld` 的 `_start` 警告
- [ ] 进阶：照着 `projects/os-from-scratch/2/pmtest.asm`，在 Bochs 里第一次切进保护模式并在屏幕角落打一个字符
- [ ] 终极：顺着 `final/` 把内核跑到 shell，再给它加一个新系统调用，让用户程序能陷进内核

## 参考资源

- 郑钢《操作系统真象还原》——这份工程（`rd_disk_m_16`、显存直写、`boot.inc` 常量风格、课次推进）与之同源，国内写 OS 入门最对路的一本
- 《一个 64 位操作系统的设计与实现》（田宇）——64 位方向的进阶
- OSDev Wiki（wiki.osdev.org）——写 OS 的百科全书，GDT、保护模式、ELF 加载每个细节都能查到
- [projects/os-from-scratch](../../projects/os-from-scratch/) ——本教程配套的真实工程实现
- [从零写一个操作系统：通电到内核的接力赛](../advanced/0-os-from-scratch.md) ——本阶段的进阶导览，鸟瞰整条链

---
*整理自作者学习笔记，按 C-Journey 写作规范重写；反汇编/hex/链接输出本机 `/tmp` 实测捕获（GNU as + objdump + xxd + ld + readelf），需模拟器启动的部分已如实标注。*
