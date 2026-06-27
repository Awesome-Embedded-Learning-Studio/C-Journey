---
title: "Linux 字符设备驱动入门：从 hello-world 模块到用户态交互"
description: "在内核态写 C：内核模块骨架、字符设备 file_operations、Makefile 构建、insmod/rmmod，以及用户态怎么撬开一个 /dev 设备。"
chapter: 6
order: 1
tags:
  - host
  - embedded
  - driver
  - os
  - system-programming
  - asm
  - build
  - make
  - debug
difficulty: advanced
reading_time_minutes: 22
platform: host
c_standard: [99, 11]
prerequisites:
  - "Chapter 0：编译流程与 Makefile"
  - "Chapter 2：指针、内存布局与位运算"
  - "Chapter 5：文件 I/O 与系统调用"
  - "Chapter 6：8051 裸机（理解直接操作硬件）"
related:
  - "裸机 C 入门：8051 时钟"
  - "Chapter 5：POSIX 文件 I/O（read/write/open 的用户态那一侧）"
---

# Linux 字符设备驱动入门：从 hello-world 模块到用户态交互

## 引言

前面我们在 8051 上玩的是**裸机**——你的 C 程序就是操作系统，直接怼寄存器,`GPIO1_DR = 0` 这种代码能立刻点灯。可一旦我们站到有 Linux 的板子上（比如 IMX6ULL），世界就变了：你想动一个 GPIO？对不起，那块地址被内核映射走了，用户态碰都不能碰。你想让你的代码跑在内核里、直接摸硬件？那就得按内核的规矩来——写一个**内核模块**。

这一章我们折腾的就是这条路：怎么在内核态写 C，把一个模块塞进运行中的内核，再让它以**字符设备**的形式露出来，让用户态那边的 `open/read/write` 能打到内核里。坦白说，这一章是整本书里"最难在本机端到端复现"的一章——内核模块离不开**对应版本的内核源码树**，而且加载它需要 root + 一个真的内核（或目标板）。所以这一章的真实输出我们会分两块讲清楚：**能在本机抓的**（源码、Makefile、编译产物、`modinfo` 的 vermagic）我们老老实实抓给你看；**抓不到的**（`insmod` 之后的 `dmesg` 日志、用户态 `read` 设备的真实回显）我会明确标出来,绝不编。

> 我们现在要做的是：先把"内核模块到底是什么"这件事想透，然后给出一份真实编译过的 hello-world，再往上长出字符设备的骨架，最后看用户态怎么跟它握上手。

## 核心概念一：内核模块到底是什么

你可以把内核模块理解成一个**可以热插拔的 .ko 文件**。内核本身是一坨巨大的 C 程序，编译成 `vmlinux` 烧在板子里；而模块是一段**运行时塞进去的额外代码**，塞进去之后就**和内核共享同一个地址空间、同一个内核栈**——这一点至关重要，它不是用户态进程那种隔离开的沙盒。

模块和普通 C 程序有两个最直观的区别：

第一，**它没有 `main`**。一个普通程序的入口是 `main`，但模块的入口是两个你**主动注册**给内核的函数：一个在加载时被调用（初始化），一个在卸载时被调用（清理）。你用 `module_init()` 和 `module_exit()` 这两个宏把函数登记上去，剩下的调度内核自己来。

第二，**它和内核版本是绑死的**。这点无数新手栽跟头，我们必须一开始就讲透。

我们回头看它的生命周期。当你在用户态敲下 `insmod hello.ko`，`insmod` 这个工具先打开 `.ko` 文件、把整个二进制读进用户态内存，然后通过 `init_module(2)` 这条系统调用把镜像连同参数一起塞进内核空间。内核拿到后做几件事：分配一块连续的内核虚拟地址放各个 ELF 节，然后比对**版本魔术**（vermagic）——当前运行的内核版本、SMP、preempt 这些标志，必须和模块编译时用的内核树对得上，否则直接拒绝加载，防止 ABI 不兼容把内核搞崩。版本过了，内核才去解析重定位表、把模块里对 `printk` 这类内核导出符号（`EXPORT_SYMBOL`）的引用接到真实地址，最后调用你注册的 init 函数。init 返回 0，模块正式上线；返回非 0，内核自动回滚，把已经分配的全都吐回去。

卸载走镜像流程，`rmmod hello` 触发 `delete_module(2)`，内核先看引用计数——只要还有别的模块或进程在用它，立刻拒绝，这点是为了防止野指针和崩溃。计数归零才调用你的 exit 函数，撤销一切注册，回收内存，从全局模块链表里摘掉自己。

事情到这里，内核模块的"骨"就清楚了：一个 init、一个 exit、两个宏、一组元信息。下面我们把它写成真代码。

## 内核模块骨架：第一个 hello-world

> Platform: host(用 ARM64 内核树交叉编译,产物 `hello.ko` 为 `AArch64` ELF)；C 标准: GNU11(内核 C)。加载/卸载需目标内核。

先上一份最经典的开局代码。几乎所有驱动教程都拿它 0 帧起手,我也不能免俗——因为对于驱动开发,这确实就是最简单的一段能跑的代码：

```c
#include <linux/init.h>
#include <linux/module.h>

MODULE_AUTHOR("Charliechen <725610365@qq.com>");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("A simple hello world module (C-Journey demo)");
MODULE_VERSION("1.0");

static int __init hello_init(void)
{
    pr_info("Hello, world!\n");
    return 0;
}

static void __exit hello_exit(void)
{
    pr_info("Goodbye, world!\n");
}

module_init(hello_init);
module_exit(hello_exit);
```

我们一段一段拆。头文件这边没有 `<stdio.h>`——内核里根本不存在这个东西。`<linux/init.h>` 给我们 `__init`/`__exit`/`module_init`/`module_exit`，`<linux/module.h>` 给那堆 `MODULE_*` 宏。`pr_info` 来自 `<linux/printk.h>`（被 module.h 间接带进来），它是 `printk(KERN_INFO ...)` 的语法糖，等价于 printf 但输出到内核环形缓冲区，不是 stdout。

接下来那四个宏是模块的**元信息**，编译时被塞进 `.ko` 的 `.modinfo` 节，`modinfo` 命令能把它们读出来。其中 `MODULE_LICENSE` 不是写着玩的——内核加载时会校验它,如果你填的 license 和内核（GPL）不兼容，内核会把自己标记成 "tainted"（被污染），并在 `dmesg` 里打印一条警告，同时部分 GPL-only 的内核符号你也用不了。常见的合法取值有 `GPL`、`GPL v2`、`Dual MIT/GPL`、`Dual BSD/GPL`、`Proprietary` 等；省略的话内核默认按 `Proprietary` 处理并警告。其余三个 `MODULE_AUTHOR/DESCRIPTION/VERSION` 纯粹是文档性质，方便你 `modinfo hello.ko` 时一眼看清这模块是谁写的、干嘛的、第几版。

> 这一点真的坑过不少人：`MODULE_LICENSE("GPL")` 不是版权声明，它是**内核的准入票**。乱填一个不匹配的字符串，你后续依赖的 GPL 符号会全部 unresolved，模块根本 insmod 不进去。

真正干活的是两个函数。`hello_init` 加载时被调一次，打印 "Hello, world!" 然后 `return 0` 表示成功。`hello_exit` 卸载时被调一次，打印 "Goodbye, world!"。注意它们前面有两个标记：`static` 让符号不导出（内核里全局命名空间很挤,能 static 就 static）；`__init` 告诉内核"这个函数只在加载时用一次，加载完可以把它所在的 `.init.text` 段回收掉省内存"，`__exit` 类似,标记"这个清理函数只有在模块可卸载时才需要，编译进内核（builtin）时直接丢弃"。

最后两个宏把这两个函数**注册**成入口和出口。`module_init(hello_init)` 展开后会把这个函数指针放进一个特殊的 init 段，内核加载模块时去那里找；`module_exit` 同理。这就是为什么模块"没有 main"——它的入口是被这两行显式交出去的。

很好,骨架有了。但光有 init/exit 的模块只能往内核日志里吐字,跟用户态还隔着一层。要让用户态能"用"上它,我们得把它包装成**字符设备**。

## 核心概念二：字符设备与 file_operations

在 Linux 的世界观里，**一切皆文件**。一个 LED、一个按键、一个串口，在用户态看来都是一个 `/dev/xxx` 文件，你 `open` 它、`read`/`write` 它、`close` 它。内核怎么把"对一个文件的 `read`"翻译成"驱动里的某个动作"？靠的就是一张函数指针表——`struct file_operations`。

你可以把它理解为驱动交给 VFS（虚拟文件系统）的一张**回调表**。当用户态对 `/dev/mydev` 调 `read()`，系统调用进到内核的 VFS 层，VFS 一查这个文件对应的 `file_operations`，找到里面的 `.read` 指针，调它。所以写一个字符设备驱动，本质上就是：实现一组 open/read/write/release 回调，把它们填进 `file_operations`，然后把这个结构注册给内核。

下面是一个最小可用的字符设备模块，比 hello-world 多了"被用户态读写"的能力。我们先看全貌,再拆细节：

```c
#include <linux/init.h>
#include <linux/module.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/uaccess.h>
#include <linux/device.h>

#define CHRDEV_NAME "cj_chrdev"
#define CHRDEV_CNT  1

static dev_t g_devid;
static struct cdev g_cdev;
static struct class* g_class;
static struct device* g_device;

/* 内核里自己留的一小块"设备数据",演示用 */
static char g_kbuf[128] = "hello from kernel cj_chrdev\n";
static size_t g_klen = 29;

static int chrdev_open(struct inode* inode, struct file* filp)
{
    pr_info("cj_chrdev: open() called\n");
    return 0;
}

static ssize_t chrdev_read(struct file* filp, char __user* buf, size_t count, loff_t* ppos)
{
    size_t to_copy;

    /* 已经读到末尾就返回 0,用户态 read 据此判断 EOF */
    if (*ppos >= g_klen) {
        return 0;
    }

    to_copy = g_klen - *ppos;
    if (to_copy > count) {
        to_copy = count;
    }

    /*
     * 关键点:用户态指针不能直接解引用。
     * 必须用 copy_to_user 把内核数据安全地搬到用户空间。
     */
    if (copy_to_user(buf, g_kbuf + *ppos, to_copy)) {
        return -EFAULT;
    }

    *ppos += to_copy;
    pr_info("cj_chrdev: read() -> %zu bytes\n", to_copy);
    return to_copy;
}

static ssize_t chrdev_write(struct file* filp, const char __user* buf, size_t count, loff_t* ppos)
{
    size_t to_copy = count;

    if (to_copy > sizeof(g_kbuf) - 1) {
        to_copy = sizeof(g_kbuf) - 1;
    }

    if (copy_from_user(g_kbuf, buf, to_copy)) {
        return -EFAULT;
    }

    g_kbuf[to_copy] = '\0';
    g_klen = to_copy;
    *ppos = 0; /* 下次 read 从新写入内容开头读 */
    pr_info("cj_chrdev: write() <- %zu bytes\n", to_copy);
    return count;
}

static int chrdev_release(struct inode* inode, struct file* filp)
{
    pr_info("cj_chrdev: release() called\n");
    return 0;
}

/* 把上面的回调填进这张函数指针表 */
static const struct file_operations chrdev_fops = {
    .owner = THIS_MODULE,
    .open = chrdev_open,
    .read = chrdev_read,
    .write = chrdev_write,
    .release = chrdev_release,
};

static int __init chrdev_init(void)
{
    int ret;

    /* 1. 申请设备号:主+次 */
    ret = alloc_chrdev_region(&g_devid, 0, CHRDEV_CNT, CHRDEV_NAME);
    if (ret < 0) {
        pr_err("cj_chrdev: alloc_chrdev_region failed: %d\n", ret);
        return ret;
    }

    /* 2. 初始化 cdev 并把它和 file_operations 绑定 */
    cdev_init(&g_cdev, &chrdev_fops);
    g_cdev.owner = THIS_MODULE;

    /* 3. 把 cdev 加进内核字符设备子系统,关联到设备号 */
    ret = cdev_add(&g_cdev, g_devid, CHRDEV_CNT);
    if (ret < 0) {
        pr_err("cj_chrdev: cdev_add failed: %d\n", ret);
        goto err_unregister;
    }

    /* 4. 在 /sys/class 下建一个类,自动创建 /dev 节点(配合 udev/mdev) */
    g_class = class_create(CHRDEV_NAME);
    if (IS_ERR(g_class)) {
        ret = PTR_ERR(g_class);
        goto err_cdev_del;
    }

    g_device = device_create(g_class, NULL, g_devid, NULL, CHRDEV_NAME);
    if (IS_ERR(g_device)) {
        ret = PTR_ERR(g_device);
        goto err_class_destroy;
    }

    pr_info("cj_chrdev: registered, major=%d minor=%d\n",
            MAJOR(g_devid), MINOR(g_devid));
    return 0;

err_class_destroy:
    class_destroy(g_class);
err_cdev_del:
    cdev_del(&g_cdev);
err_unregister:
    unregister_chrdev_region(g_devid, CHRDEV_CNT);
    return ret;
}

static void __exit chrdev_exit(void)
{
    /* 卸载顺序严格逆着 init 来,谁后建谁先拆 */
    device_destroy(g_class, g_devid);
    class_destroy(g_class);
    cdev_del(&g_cdev);
    unregister_chrdev_region(g_devid, CHRDEV_CNT);
    pr_info("cj_chrdev: unregistered\n");
}

module_init(chrdev_init);
module_exit(chrdev_exit);

MODULE_AUTHOR("Charliechen <725610365@qq.com>");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_DESCRIPTION("Minimal char device for C-Journey");
```

这段代码信息量不小,我们挑几处最该讲透的地方。

**第一处是 `file_operations` 那张表。** 用 C99 的 designated initializer(指定成员初始化)填,没指定的字段自动是 NULL——内核 VFS 看到某个回调是 NULL 就走默认行为。`.owner = THIS_MODULE` 这行特别重要,它建立了"这个 file_operations 属于当前模块"的引用关系,内核据此防止模块在被使用时被卸载。`.open/.read/.write/.release` 四个就是我们实现的回调,分别对应用户态 `open/read/write/close` 进内核后的落点。

**第二处是 `__user` 和 `copy_to_user`/`copy_from_user`。** 这是用户态和内核态交互里**最容易出事**的地方。看 `chrdev_read` 的签名:`char __user* buf`——这个 `buf` 是用户态传进来的指针。新手最想干的事就是直接 `memcpy(buf, ...)` 或者 `buf[0] = 'x'`。千万别。内核里直接解引用用户态指针是一个巨大的安全/稳定性黑洞:那个地址可能根本没映射、可能触发缺页、可能来自恶意进程。内核专门提供 `copy_to_user`(内核→用户)和 `copy_from_user`(用户→内核)这两个函数,它们会做合法性检查、处理缺页,失败时返回未拷贝的字节数。所以我们的 `read` 里 `copy_to_user` 失败就老老实实返回 `-EFAULT`(errno.h 里的错误码),用户态那边 `read` 会拿到 -1 并置 `errno=EFAULT`。

> 真正的坑在这里:内核态直接 `memcpy` 一个 `__user` 指针,代码能编过(Smatch/sparse 会警告,但 gcc 不拦),运行起来时好时坏——运气好就是段错误,运气不好就是内核 oops 把整个系统带崩。看到 `__user` 标注的指针,条件反射想 `copy_to_user`/`copy_from_user`,这条规矩没有例外。

**第三处是 init 里的四步注册和那一串 `goto err_xxx`。** 注册顺序是:申请设备号 → 初始化 cdev → cdev_add → 建 class → 建 device。每一步都可能失败(资源耗尽、名字冲突),失败时必须把**之前已经成功的步骤**全撤销,否则就资源泄漏。这就是那些 `goto` 的用途——它们不是乱写的跳转,而是一条精心编排的回滚链:`err_class_destroy` 回滚到 `class_destroy`,然后 `err_cdev_del` 回滚到 `cdev_del`,层层向前。这种 "goto unwind" 是内核错误处理的标准范式,务必习惯它。卸载函数 `chrdev_exit` 严格逆着 init 的顺序拆:device → class → cdev → 设备号。

> 这里千万别手滑:init 里申请的顺序和 exit 里释放的顺序**必须严格镜像**,谁后建谁先拆。乱了的话,轻则 `device_destroy` 报警告,重则 use-after-free 把内核搞挂。

代码骨架到这里就齐了。接下来问题来了——它怎么变成 `.ko`?

## 构建:Makefile 与内核版本绑定

内核模块不能拿普通 `gcc -c` 编出来就完事,它必须借助**内核自身的构建系统(Kbuild)**。原因前面讲过:模块要和具体内核 ABI 绑死,所以编译时必须指着一棵**和目标内核版本完全一致的内核源码树**,让 Kbuild 帮你插入正确的编译选项、正确的 `vermagic`。

我们写的 Makefile 非常短,但它做的事情一点都不简单:

```makefile
# cj_chrdev 的 Makefile(把 hello 换成 cj_chrdev 即可)

obj-m += cj_chrdev.o      # 注意:obj-m 的对象是"去掉 .c 后缀的文件名",必须和源文件名一致

KDIR ?= /lib/modules/$(shell uname -r)/build
ARCH ?=
CROSS_COMPILE ?=

all:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) modules

clean:
	$(MAKE) -C $(KDIR) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) M=$(PWD) clean
```

逐行看。`obj-m += cj_chrdev.o` 是 Kbuild 的约定:`obj-m` 表示"要编成模块(.ko)的对象",后面跟的是源文件去掉后缀的名字——所以你的源文件必须叫 `cj_chrdev.c`,对得上。如果你想一个 Makefile 编多个模块,就多写几行 `obj-m += xxx.o`。

`-C $(KDIR)` 让 make 切到内核源码树目录去执行(那里有 Kbuild 的主 Makefile);`M=$(PWD)` 告诉 Kbuild"我的模块源码在外面的这个目录";最后的 `modules` 是 Kbuild 的目标,专门编外部模块。`ARCH` 和 `CROSS_COMPILE` 这两个变量是给交叉编译用的:本机编本机跑就留空;给 ARM 板编,就填 `ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu-`。

> 新手注意:**你必须用你准备让驱动跑在的那个内核版本的源码树来编**。很多人本机用 `uname -r` 的内核源码编出来,以为编译通过就万事大吉,结果拿到板子上一 `insmod` 抱怨 "disagrees about version of symbol" 或者 vermagic 对不上——直接拒载。这是因为模块加载时内核会拿 `.ko` 里的 vermagic 比对当前内核的版本、SMP、preempt 标志,差一点都不行。所以目标板是什么内核,你就拿那棵树编,别偷懒用别的。

### 本机实测:我真的编出来了

说到做到,我没有只在纸上谈。本机没有 x86_64 的内核头(WSL2 的 `/lib/modules/$(uname -r)/build` 是空的),但我手头有一棵已经构建过 `vmlinux` 的 ARM64 内核树(源码版本 `7.0.0+`),配合 `aarch64-linux-gnu-gcc`,我把上面的 hello-world 真的编出了 `hello.ko`。下面是**真实抓到的构建日志**,没动过一行:

```text
$ make
make -C /path/to/linux ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- M=/tmp/imp_driver modules
make[1]: Entering directory '/path/to/linux'
make[2]: Entering directory '/tmp/imp_driver'
  CC [M]  hello.o
  MODPOST Module.symvers
  CC [M]  hello.mod.o
  CC [M]  .module-common.o
  LD [M]  hello.ko
make[2]: Leaving directory '/tmp/imp_driver'
make[1]: Leaving directory '/path/to/linux'
```

编出来的产物是真实的 ELF 文件,`file` 命令如实报告:

```text
$ file hello.ko
hello.ko: ELF 64-bit LSB relocatable, ARM aarch64, version 1 (SYSV), not stripped
```

最有说服力的是 `modinfo`——它把编译时戳进 `.modinfo` 节的 vermagic 读了出来,这条 vermagic 就是后续 `insmod` 时内核要校验的那个版本指纹。下面这段是**真实输出**,包括内核版本、SMP、preempt、`mod_unload` 标志:

```text
$ modinfo hello.ko
filename:       /tmp/imp_driver/hello.ko
version:        1.0
description:    A simple hello world module (C-Journey demo)
license:        Dual MIT/GPL
author:         Charliechen <725610365@qq.com>
srcversion:     D3111D6BE338FFA75E1FC6E
depends:
name:           hello
vermagic:       7.0.0+ SMP preempt mod_unload aarch64
```

看清楚了吗?我们源码里写的 `MODULE_AUTHOR/DESCRIPTION/VERSION/LICENSE` 全部如实进了 `.modinfo`;`vermagic` 那行则证明这个 `.ko` 是绑死在 `7.0.0+ SMP preempt` 这棵 ARM64 内核树上的。**这一侧的证据是真实抓到的。**

> 本机抓不到的部分(老实交代):`insmod hello.ko` 之后 `dmesg` 里应该出现 `Hello, world!`,`rmmod hello` 后出现 `Goodbye, world!`——但这一步需要一个**正在运行的、版本匹配的 ARM64 Linux**(目标板或 QEMU 跑起来的 arm64 系统)。本机是 WSL2 x86_64,既没有目标内核也加载不了 AArch64 的 `.ko`,所以 `dmesg` 那段日志我没有编造,需要你拿到目标板/仿真器上自己 `insmod` 验证。`pr_info` 走 `KERN_INFO`,日志默认级别下未必上控制台,记得 `dmesg | tail` 或调高 `cat /proc/sys/kernel/printk`。

另外要补充一句:那棵内核树原本的 `.config` 是关掉模块支持的(没有 `CONFIG_MODULES=y`),我第一次直接 `make modules` 被内核直接拒绝("The present kernel disabled CONFIG_MODULES")。开了 `CONFIG_MODULES`+`CONFIG_MODULE_UNLOAD`、跑了 `make modules_prepare` 之后外部模块才编得动。这段小折腾也如实记在这里,免得你照着别的内核树一编就撞墙还摸不着头脑。

## 加载、卸载与用户态交互

模块编出来之后,接下来的流程在**目标板**上跑(本机这一侧做不到,我们只讲流程 + 标注)。把它放到板子上,然后:

```bash
# 加载(两种方式)
insmod hello.ko          # 直接塞,不做依赖解析
modprobe hello           # 先查 modules.dep,按依赖顺序加载(更适合有依赖的复杂模块)

# 看内核日志
dmesg | tail             # 应能看到 hello_init 打印的 "Hello, world!"
lsmod | grep hello       # 看模块是否在线,以及被谁引用(Used by 列)

# 卸载
rmmod hello              # 直接卸,不管引用计数
modprobe -r hello        # 按依赖逆序卸
```

`insmod` 和 `modprobe` 的区别值得记一笔。`insmod` 是"傻"工具,你给它哪个 `.ko` 它就硬塞哪个,不解析依赖、不读 `/etc/modprobe.d`。`modprobe` 是"聪明"工具:它先读 `depmod` 预先生成的 `modules.dep`,按依赖顺序把底层模块先加载好,再加载你要的;卸载时也按引用计数逆序拆。简单单文件模块用 `insmod` 就够了,一旦你用了 `EXPORT_SYMBOL` 把一个模块当另一个模块的库,就得靠 `modprobe`。

> 内核为每个加载的模块维护**引用计数**。当别的模块 `try_module_get()` 引用了你,你的计数 +1;只要计数 > 0,`rmmod` 会被拒绝。这是防止"还在用就被拔"导致野指针。`lsmod` 能直接看到这个计数和"被谁引用"。

### 用户态怎么撬开字符设备

回到我们的字符设备。模块加载后,因为 init 里调了 `class_create` + `device_create`,配合板子上的 udev/mdev,`/dev/cj_chrdev` 会自动冒出来(没 udev 就得手动 `mknod /dev/cj_chrdev c <major> <minor>`,major/minor 在 `dmesg` 里能看到)。用户态这边,设备就是一个普通文件,我们用标准的 POSIX 文件 I/O(`open/read/write/close`)去读它:

```c
/* 用户态测试程序:user_test.c —— 这段是真正的用户态 C,可以本机编本机跑(对着一个真设备节点) */
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>

int main(void)
{
    char buf[64] = {0};
    ssize_t n;
    int fd = open("/dev/cj_chrdev", O_RDWR);
    if (fd < 0) {
        perror("open");
        return EXIT_FAILURE;
    }

    /* 读:会触发内核 chrdev_read -> copy_to_user */
    n = read(fd, buf, sizeof(buf) - 1);
    if (n < 0) {
        perror("read");
        close(fd);
        return EXIT_FAILURE;
    }
    printf("read back %zd bytes: %s", n, buf);

    /* 写:会触发内核 chrdev_write -> copy_from_user */
    const char* msg = "ping from userspace\n";
    n = write(fd, msg, strlen(msg));
    printf("wrote %zd bytes\n", n);

    /* 再读一次,这次拿到的是刚写进去的内容 */
    lseek(fd, 0, SEEK_SET);
    n = read(fd, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    printf("read back %zd bytes: %s", n, buf);

    /* close 对应内核 chrdev_release */
    close(fd);
    return EXIT_SUCCESS;
}
```

这段用户态代码本身就是 Chapter 5 里讲过的标准文件 I/O,没有任何新 API。**新东西全在内核那一侧**:用户态的每一次 `read(fd,...)`,经系统调用进 VFS,VFS 找到 `/dev/cj_chrdev` 对应的 `file_operations`,调到我们的 `chrdev_read`,我们的 `copy_to_user` 把内核里的 `g_kbuf` 搬到用户态 `buf`——这就是"用户态与内核态交互"最朴素也最本质的一幅图。

> 本机这一侧:`user_test.c` 可以用普通 `gcc` 在本机编出来跑,但本机没有 `/dev/cj_chrdev` 这个节点(我们的内核模块是 ARM64 的,加载不到 WSL2),所以会停在 `open` 报 `No such file or directory`。它真正的回显需要在加载了 `cj_chrdev.ko` 的目标板上才有。源码是真的、行为逻辑是真的、回显需要目标板——这点我标清楚。

### 另一条路:sysfs,不写驱动也能玩硬件

在结束能量之前,值得提一条**更省事**的路径,因为它解释了为什么很多时候你根本不用写驱动。内核把很多设备的控制接口直接暴露成了 sysfs 里的虚拟文件——你在 `/sys/class/leds/sys-led/brightness` 写个 `0` 或 `1` 就能灭/亮点灯,在 `/sys/class/gpio/export` 写个编号就能导出 GPIO 引脚,完全不用碰 `.ko`。

它的本质是:已有的内核驱动(LED 子系统、GPIO 子系统)帮你注册好了字符属性,用户态通过普通的文件读写去触发驱动的回调。换句话说,`echo 1 > brightness` 这条命令在内核里走的也是"文件操作→驱动回调"的同一条路子,只不过回调是别人写好的。当你只是要点个灯、读个引脚电平,先去 sysfs 里找找,十有八九已经有了;真要操作内核还没抽象的自定义硬件、或者要自定义协议,才轮到我们这一章写的字符设备驱动登场。

## 常见坑(踩坑预警)

这一章的坑比前面几章都致命——内核态崩了可不像用户态段错误那么温柔,动辄整个系统挂掉。把我踩过的几个最痛的列在这里:

- **内核树版本对不上**。用错内核树编出来的 `.ko`,`insmod` 直接报 vermagic 不匹配。目标板什么内核,就拿那棵树的源码 + 同样的 `CONFIG_*` 编,连 `LOCALVERSION` 这种小版本号都要一致。
- **直接解引用 `__user` 指针**。看到 `char __user* buf` 一律 `copy_to_user`/`copy_from_user`,不要 `memcpy`、不要直接下标。这是安全红线。
- **init 里注册顺序和 exit 里释放顺序不镜像**。谁后建谁先拆,乱了就 use-after-free。失败回滚用 `goto unwind` 链,别图省事 `return`。
- **忘填 `MODULE_LICENSE`**。默认按 `Proprietary` 处理,内核 tainted + GPL 符号用不了,模块起不来。
- **模块返回了未初始化的非零值当成功**。init 函数记得 `return 0` 表示成功,任何失败路径要返回负的 errno(`-ENOMEM`、`-EFAULT` 这种),内核靠负值判断成败。
- **加载了就改代码、改完直接 `insmod` 报忙**。模块还在被占用时改不了,先 `rmmod` 干净,或者 `lsof /dev/cj_chrdev` 看谁还开着它。
- **本机没有内核头硬编**。`/lib/modules/$(uname -r)/build` 指向不存在的目录,`make` 直接报 "No rule to make target"。要么装 `linux-headers-$(uname -r)`,要么拿一棵已构建的内核树 + 交叉编译。

## 小结

到这里,我们走完了 Linux 字符设备驱动的入门闭环。关键要点回顾一下:

- 内核模块是**可热插拔的 `.ko`**,和内核共享地址空间,入口是 `module_init`/`module_exit` 注册的 init/exit 函数,没有 `main`。
- 模块和内核版本**绑死**,`vermagic` 不匹配直接拒载——必须用目标内核对应的源码树编。
- 字符设备的核心是一张 `struct file_operations` 回调表;用户态的 `open/read/write/close` 经 VFS 落到驱动里对应的回调。
- **用户态↔内核态传数据必须用 `copy_to_user`/`copy_from_user`**,绝不直接碰 `__user` 指针。
- init 里的注册和 exit 里的释放**严格镜像**,失败用 `goto unwind` 回滚。
- 加载用 `insmod`/`modprobe`,看日志 `dmesg`,看在线状态 `lsmod`,卸载 `rmmod`/`modprobe -r`。
- 优先看 sysfs(`/sys/class/...`)能不能解决需求,再决定要不要自己写驱动。

这一章的真实输出分了两块:能本机抓的(源码、Makefile、真实编译日志、`file`、`modinfo` 的 vermagic)我都抓给你看了;抓不到的(`insmod` 后的 `dmesg`、用户态读设备的真实回显)需要目标板/仿真器,我没有编造。这是 Linux 驱动开发绕不过去的一道坎——它天然绑定具体内核和具体硬件,本机桌面环境没法端到端跑通。但你现在已经有了能编译、能上板的全部素材,拿到板子一 `insmod` 就能见到 `Hello, world!`。

## 练习

1. 把 hello-world 模块改成带参数的:`module_param(name, type, perm)` 加一个 `int repeat` 参数,加载时 `insmod hello.ko repeat=3`,让 init 里循环打印 `repeat` 次。`dmesg` 验证参数确实传进去了。
2. 在 `cj_chrdev` 里加一个 `.llseek` 和一个 `.unlocked_ioctl` 回调,实现用户态用 `ioctl(fd, CMD, &val)` 设置内核 `g_kbuf` 的内容。
3. 给 `cj_chrdev` 的 `read` 加并发保护:用 `DEFINE_MUTEX` 包住对 `g_kbuf`/`g_klen` 的访问,思考为什么多核 + SMP 下不加锁会出问题(提示:`read` 和 `write` 可能同时在不同 CPU 上跑)。
4. 在一个真的 ARM64 Linux(QEMU 跑 `qemu-system-aarch64` 也行)上加载你的 `hello.ko`,抓一份真实的 `dmesg` 输出贴到笔记里——这是本章唯一需要你补上的"本机抓不到"那一块。
5. 用 sysfs 之路:在你的板子上 `echo 1 > /sys/class/leds/sys-led/brightness` 点灯,然后写一段用户态 C 用 `open/write` 实现同样的事,对比"用现成子系统"和"自己写字符设备"两种方案的取舍。

## 参考资源

- [The Linux Kernel documentation — Linux Kernel Module Programming](https://www.kernel.org/doc/html/latest/admin-guide/modules.html)
- [Linux Device Drivers, 3rd Edition (LDD3, O'Reilly, GPL 在线版)](https://lwn.net/Kernel/LDD3/)
- [kernel.org printk-basics(`pr_*` 宏体系)](https://www.kernel.org/doc/html/latest/core-api/printk-basics.html)
- 本仓库 Chapter 5:POSIX 文件 I/O(用户态那一侧的 `open/read/write/close`)
- 本仓库 Chapter 6 第 0 篇:8051 裸机 C(对比"有 OS"和"没 OS"两种世界)

---

*整理自作者笔记(《内核学习——驱动编程部分整理1:基础模块编程》《Linux 驱动开发之应用层 1/2》《深入理解 Linux 内核 1:printk 系列》),按 C-Journey 写作规范重写;可在本机构建的部分(模块源码、Makefile、`hello.ko` 编译产物与 `modinfo` vermagic)已实测,需目标板的部分(`insmod`/`dmesg`/用户态读设备回显)已标注。*
