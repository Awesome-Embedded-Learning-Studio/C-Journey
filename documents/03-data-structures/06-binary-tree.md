---
title: "二叉树基础:节点、前/中/后序遍历、递归与释放"
description: "阶段3·第6章。从单链表的「一个 next」升级到二叉树的「left/right 两个分叉」:节点用自引用 struct Node(同样必须写 struct Node*),手动连接节点造一棵小树,然后核心讲透递归三态遍历——前序(根→左→右,1 2 4 3 5)、中序(左→根→右,4 2 1 3 5)、后序(左→右→根,4 2 5 3 1)用同一棵树真跑三种顺序,差别只在「printf 根的那一句摆在递归左子、右子的什么位置」。再讲释放树必须后序(先 free 左右子、最后 free 根)——坑是:手滑写成先 free 根再递归,就读不到 root->left/right 了,直接 use-after-free(呼应阶段2 Ch7),gcc 编译期 -Wuse-after-free + ASan 运行期 heap-use-after-free 双重抓;正确后序写法 ASan 复核退出码 0 无泄漏。全 gcc16+clang22 真跑 -std=c11 -Wall,free 加 -fsanitize=address。"
chapter: 3
order: 6
tags:
  - host
  - data-structures
  - pointers
difficulty: intermediate
reading_time_minutes: 13
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段3·第1章:单链表(节点、自引用 struct Node*、指针串联)"
  - "阶段2·第6/7章:动态内存(malloc/free、ASan 抓 UAF)"
  - "第 8 章:函数(递归)"
related:
  - "阶段3·第7章:二叉搜索树 BST(插入/查找/删除)、第12章:大 O(树高 O(log n) vs 退化 O(n))"
---

# 二叉树基础:节点、前/中/后序遍历、递归与释放

## 引言:从「一条链」到「分叉」

阶段3·Ch1 我们用单链表迈出了数据结构的第一步:每个节点揣一个 `next` 指针,把散在堆上的节点串成一条线。这条线只能往前走、不能分叉,适合「排队」「栈」这类**线性**结构。但现实里的数据经常是**分叉**的——一个公司的组织架构(CEO 下面几个 VP、VP 下面几个总监)、一个文件系统(一个目录下若干子目录)、一个表达式 `a + b * c`(加号下面挂着 a 和「乘法子树」)——这些用一根 `next` 串不成,得允许每个节点有**两个**分支,这就是**二叉树(binary tree)**。

二叉树是后续好几章的地基:第 7 章的**二叉搜索树 BST**(左子都比根小、右子都比根大,查找插入 O(log n))、堆、甚至哈希表里冲突处理用的链表都能换成树——这些全都建立在「节点有 left/right 两个子指针」这个结构上。所以这一章我们只做两件事、把它们做扎实:**怎么定义和连接二叉树的节点**、**怎么用递归把整棵树走一遍(三种走法)**。`malloc` 申请节点(阶段2·Ch6)、递归思想(第 8 章)、ASan 抓内存错(阶段2·Ch7)这些前面攒下的功底,这一章全用上。

## 节点:自引用,但这次揣两个指针

二叉树的节点和单链表的节点几乎一样,只是把「一个 next」换成「两个子指针」——一个 `left`(左子)、一个 `right`(右子):

```c
typedef struct Node {
    int data;
    struct Node* left;
    struct Node* right;
} Node;
```

那个在阶段3·Ch1 重点讲过的自引用坑,这里一模一样:`left` 和 `right` 必须写全名 `struct Node*`,不能图省事写 `Node*`。原因还是那条——`typedef struct Node {...} Node;` 这一行得从左读到右才生效,而 `left`/`right` 这两个成员声明出现在结构体**内部**、出现在右边的 `} Node` **之前**,此刻编译器还只认得 `struct Node` 这个带标签的全名、不知道 `Node` 这个别名(§6.7.2.1 讲结构成员声明)。好在指针大小固定、和指向类型多大无关(阶段2·Ch1 真跑过 64 位机上任何指针都是 8 字节),所以「指向自己这种还没完全定义的类型的指针」是完全合法的——没有循环定义的悖论。

建节点的工厂函数也和单链表如出一辙,`malloc` 之后立刻查 NULL、再把两个子指针都置 NULL:

```c
Node* new_node(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) { /* malloc 可能失败,必查(阶段2·Ch6) */
        return NULL;
    }
    n->data = data;
    n->left = NULL;  /* 别忘了置空,否则是垃圾地址的野指针 */
    n->right = NULL;
    return n;
}
```

`malloc` 给的内存是**没初始化的**(阶段2·Ch6 真跑过 calloc 才清零、malloc 不清零),所以 `n->left` 和 `n->right` 里是上一任主人留下的垃圾值——如果漏了这两行置 NULL,新节点就会揣着两个随机地址的野指针,后续遍历时顺着它们乱跳,直接重现阶段2·Ch7 那批内存错误。两个 `= NULL` 能省掉无穷无尽的玄学 bug,值。

## 构建一棵小树:手动连接

二叉树的「插入逻辑」(怎么自动决定一个新节点放左还是放右)要等到第 7 章 BST 才讲——那会用到「比根小放左、比根大放右」的规则。本章我们用最朴素的方式:**手动连接**,直接给每个节点的 `left`/`right` 赋值,造一棵想要的形状出来。这样能让我们把注意力集中在「遍历」和「释放」这两件事上,不被插入规则分心。

我们造这么一棵小树:

```text
        1
       / \
      2   3
     /     \
    4       5
```

根是 1,1 的左子是 2、右子是 3;2 的左子是 4(2 没有右子);3 的右子是 5(3 没有左子)。每个节点要么有两个子、要么有一个、要么一个都没有(叶子节点,如 4 和 5)。建它的代码就是先把 5 个节点各自 `new_node` 出来,再手动把父子关系连上:

```c
Node* root = new_node(1);
root->left = new_node(2);
root->right = new_node(3);
root->left->left = new_node(4);
root->right->right = new_node(5);
```

留意最后一行 `root->right->right = ...`:这是阶段2·Ch1 讲过的「指针顺着地址走」的连用——`root->right` 拿到节点 3、再 `->right` 拿到节点 3 的右子指针字段、给它赋值。这种 `a->b->c` 的链式访问在树代码里到处都是,本质就是「顺着指针一层层解下去」。

## 遍历三态:前序、中序、后序

树建好了,怎么「走一遍」它?线性结构走法只有一种——从头顺着 next 一路往后。但二叉树每个节点分叉,你得决定**先访问根、还是先访问子树**——这个决定就分出了三种经典走法,合称**深度优先遍历**。三种走法的代码结构几乎一样,都是递归:对一个节点,递归处理它的左子树、递归处理它的右子树,中间穿插「访问根」(这里「访问」就是 `printf` 打印它的 data)。**唯一的差别,就是 `printf` 这一句摆在什么位置**。

前序(preorder)是把「访问根」摆**最前**:先打印自己,再递归左、递归右。

```c
/* 前序:根 -> 左 -> 右 */
void preorder(Node* root) {
    if (root == NULL) {
        return;
    }
    printf("%d ", root->data); /* 先访问根 */
    preorder(root->left);
    preorder(root->right);
}
```

中序(inorder)是把「访问根」夹在**中间**:先递归左、再打印自己、再递归右。

```c
/* 中序:左 -> 根 -> 右 */
void inorder(Node* root) {
    if (root == NULL) {
        return;
    }
    inorder(root->left);
    printf("%d ", root->data); /* 根夹在中间 */
    inorder(root->right);
}
```

后序(postorder)是把「访问根」摆**最后**:先递归左、递归右,两棵子树都处理完了,才轮到打印自己。

```c
/* 后序:左 -> 右 -> 根 */
void postorder(Node* root) {
    if (root == NULL) {
        return;
    }
    postorder(root->left);
    postorder(root->right);
    printf("%d ", root->data); /* 最后访问根 */
}
```

这三个函数长得很像,你只要盯住 `printf` 那一行的位置:前序在最前、中序在中间、后序在最后——名字就是这么来的。`if (root == NULL) return;` 是递归的**基线**(第 8 章讲过,递归必须有基线否则无限递归把栈压爆):走到 NULL(空子树)就返回,递归在这里收口。把三种遍历拼到刚才那棵树上真跑一遍:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* left;
    struct Node* right;
} Node;

Node* new_node(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->data = data;
    n->left = NULL;
    n->right = NULL;
    return n;
}

void preorder(Node* root) {
    if (root == NULL) {
        return;
    }
    printf("%d ", root->data);
    preorder(root->left);
    preorder(root->right);
}

void inorder(Node* root) {
    if (root == NULL) {
        return;
    }
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
}

void postorder(Node* root) {
    if (root == NULL) {
        return;
    }
    postorder(root->left);
    postorder(root->right);
    printf("%d ", root->data);
}

void free_tree(Node* root) {
    if (root == NULL) {
        return;
    }
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main(void) {
    /* 建树:
     *         1
     *        / \
     *       2   3
     *      /     \
     *     4       5
     */
    Node* root = new_node(1);
    root->left = new_node(2);
    root->right = new_node(3);
    root->left->left = new_node(4);
    root->right->right = new_node(5);

    printf("preorder:  ");
    preorder(root);
    printf("\n");

    printf("inorder:   ");
    inorder(root);
    printf("\n");

    printf("postorder: ");
    postorder(root);
    printf("\n");

    free_tree(root);
    root = NULL;
    return 0;
}
```

```text
$ gcc -std=c11 -Wall traverse.c -o t && ./t
preorder:  1 2 4 3 5
inorder:   4 2 1 3 5
postorder: 4 2 5 3 1
```

同一棵树,三种遍历给出三串完全不同的输出——这正是「访问根的时机」决定的。我们手动模拟一遍前序,看清楚 `1 2 4 3 5` 是怎么来的:从根 1 开始,前序先打印自己,所以**第一个打印的是 1**;然后递归进左子树(根是 2),前序先打印 2,**第二个是 2**;再递归进 2 的左子树(根是 4),打印 **4**;4 是叶子,左右子树都是 NULL 直接返回,2 的右子树也是 NULL,于是回到 1、递归进 1 的右子树(根是 3),打印 **3**;3 没有左子,递归进右子树(根是 5),打印 **5**。串起来就是 `1 2 4 3 5`,和真跑输出逐字对上。

中序的关键是「根夹中间」:你得先把左子树整棵走完,才轮到打印自己。所以最左下角的叶子 4 第一个被打印,然后是它的父亲 2,然后回到根 1——`4 2 1` 这一段就是这么来的,根 1 居然排在第三位,因为它要等整棵左子树走完。中序有个很重要的性质(第 7 章 BST 会用到):**对一棵二叉搜索树做中序遍历,输出是排好序的**。后序则是「根最后」:4 和 5 这种叶子最先被打印,根 1 要等两棵子树全部走完才轮到、所以排最后 `... 1`。

三种遍历用的递归调用完全一样,差别只在「打印根」这一行摆在递归左、递归右的前、中、后——记住这一点,前/中/后序就不必死记。

## 释放整棵树:必须后序

树和单链表一样,节点都是 `malloc` 从堆上要来的,用完得逐个 `free` 干净,否则就是内存泄漏(阶段2·Ch7 的 ASan 在程序退出时会报 `detected memory leaks`)。释放一棵树的正确做法,正是我们刚学的**后序遍历**——这绝不是巧合:后序「先处理子、最后处理根」的顺序,在这里有非常具体的物理意义——**得先把一棵子树的根的两个子树都释放掉,才能安全地释放这个根自己**。

为什么?因为根的 `left`/`right` 字段里存着「左子树和右子树在哪」的地址。如果你先 `free(root)` 把根释放了,那两块地址就跟着根的内存一起还给系统了,你再也读不到 `root->left`/`root->right`——可这时候左右子树里的节点还一个都没释放呢,你连「它们在哪」都查不到了。这就是 use-after-free。所以正确顺序只能是后序:**先递归把左子树整棵释放、再递归把右子树整棵释放、最后才 free 根自己**——等轮到 free 根的时候,它下面的子树已经全部处理完毕,根这个节点也就彻底没用了,安心归还。

```c
void free_tree(Node* root) {
    if (root == NULL) {
        return;
    }
    free_tree(root->left);  /* 先把左子树整棵释放掉 */
    free_tree(root->right); /* 再把右子树整棵释放掉 */
    free(root);             /* 最后才 free 根——此时左右已经处理完,不再需要它 */
}
```

来体会一下写错顺序的后果——如果你手滑写成了「先 free 根、再递归子树」(看着像前序,但前序用来释放就是灾难):

```c
/* 反例:先 free 根,再去 free 左右子——读不到左右了 */
void free_tree_bad(Node* root) {
    if (root == NULL) {
        return;
    }
    free(root);                 /* 根这块内存已释放 */
    free_tree_bad(root->left);  /* UB:读已释放内存里的 left */
    free_tree_bad(root->right); /* UB:读已释放内存里的 right */
}
```

`free(root)` 之后,`root` 指向的那块堆内存就已经还给系统了,接下来 `root->left` 是在**读一块已释放内存里的 left 字段**——这正是阶段2·Ch7 讲过的 use-after-free,是未定义行为。真跑用 ASan 抓:

```text
$ gcc -std=c11 -Wall -fsanitize=address uaf.c -o uaf && ./uaf
uaf.c: In function 'free_tree_bad':
uaf.c:28:23: warning: pointer 'root' used after 'free' [-Wuse-after-free]
   28 |     free_tree_bad(root->right); /* UB:读已释放内存里的 right */
      |                   ~~~~^~~~~~~~~
uaf.c:26:5: note: call to 'free' here
   26 |     free(root);                 /* 根这块内存已释放 */
      |     ^~~~~~~~~~
uaf.c:27:23: warning: pointer 'root' used after 'free' [-Wuse-after-free]
   27 |     free_tree_bad(root->left);  /* UB:读已释放内存里的 left */
      |                   ~~~~^~~~~~~~~
uaf.c:26:5: note: call to 'free' here
   26 |     free(root);                 /* 根这块内存已释放 */
      |     ^~~~~~~~~~
==366977==ERROR: AddressSanitizer: heap-use-after-free on address 0x6d6eaa9e0048
READ of size 8 at 0x6d6eaa9e0048 thread T0
    #0 in free_tree_bad (.../uaf_gcc+0x12a6)
    #1 in main           (.../uaf_gcc+0x1380)
SUMMARY: AddressSanitizer: heap-use-after-free (.../uaf_gcc+0x12a6) in free_tree_bad
==366977==ABORTING
```

gcc 在编译期就甩了两个 `-Wuse-after-free` 警告(`pointer 'root' used after 'free'`,分别指向第 27 行的 `root->left` 和第 28 行的 `root->right`,还贴心标注 `note: call to 'free' here` 指向第 26 行),ASan 在运行期直接 abort,报告 `heap-use-after-free`、`READ of size 8`(读 `left` 这个指针占 8 字节)。和阶段3·Ch1 释放单链表时的坑一模一样的根因——**先 free 了承载指针的内存,再去读那些指针**;只不过链表那里是「free 当前节点前先存 next」,树这里更优雅的解法是「后序递归」,连「手动存临时变量」都省了,递归帮你把顺序安排好。把正确的 `free_tree` 接到主程序里,ASan 复核一遍:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* left;
    struct Node* right;
} Node;

Node* new_node(int data) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        return NULL;
    }
    n->data = data;
    n->left = NULL;
    n->right = NULL;
    return n;
}

void free_tree(Node* root) {
    if (root == NULL) {
        return;
    }
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main(void) {
    /* 建和 traverse.c 同一棵树:1 / 2 3 / 4 . . 5 */
    Node* root = new_node(1);
    root->left = new_node(2);
    root->right = new_node(3);
    root->left->left = new_node(4);
    root->right->right = new_node(5);

    printf("tree built, freeing...\n");

    free_tree(root); /* 后序释放,ASan 复核应无泄漏 */
    root = NULL;     /* 防悬垂:释放完把根指针也置空 */

    printf("freed.\n");
    return 0;
}
```

```text
$ gcc -std=c11 -Wall -fsanitize=address free_tree.c -o fl && ./fl; echo $?
tree built, freeing...
freed.
0
```

退出码 `0`、ASan 没有任何 `detected memory leaks` 报告——五个节点都被后序递归妥善 free 掉了,这棵树清清爽爽地结束了它的生命周期。`root = NULL;` 是阶段2·Ch7 的好习惯:释放完立刻把根指针置空,杜绝之后手滑再用到悬垂的 `root`。顺带一提,`free_tree(NULL)` 是安全的——`if (root == NULL) return;` 直接命中基线,一次都不执行,所以「释放一棵空树」天然正确,调用者不必特判。

## 小结

二叉树是我们从单链表的「一个 next」升级到「left/right 两个分叉」的第一个非线性数据结构,节点定义和单链表几乎同构,只是把单个后继指针换成左右两个子指针——同样的自引用规矩,`left`/`right` 必须写全名 `struct Node*`(typedef 别名在结构体内部还没生效,§6.7.2.1),`malloc`(§7.22.3)之后立刻查 NULL 并把两个子指针都置 NULL(malloc 给的内存不初始化,漏置空就是野指针)。建树本章用手动连接(第 7 章 BST 才讲插入规则),`root->left->left = new_node(4)` 这种链式赋值本质是阶段2·Ch1「顺着指针解下去」的连用。核心是递归三态遍历:前序(根→左→右,真跑 `1 2 4 3 5`)、中序(左→根→右,`4 2 1 3 5`)、后序(左→右→根,`4 2 5 3 1`)——同一棵树三种输出,差别**只在 `printf` 那一行摆在递归左、递归右的前、中、后**,记住这一点三种顺序就不必死记;`if (root == NULL) return;` 是递归基线,空子树在这里收口。释放整棵树**必须后序**:先递归 free 左子树、再递归 free 右子树、最后 free 根——因为根的 `left`/`right` 存着子树在哪,先 free 根就丢了这些地址、再也找不到子树,直接 use-after-free(真跑 gcc 编译期 `-Wuse-after-free` + ASan 运行期 `heap-use-after-free READ of size 8` 双重抓,呼应阶段2·Ch7);正确后序写法 ASan 复核退出码 0 无泄漏。这一章把「定义、连接、遍历、释放」四件事真跑通,下一章我们给二叉树加上一条规则——左子都比根小、右子都比根——它就变成二叉搜索树 BST,查找插入删除都能在树高内完成(O(log n)),中序遍历还恰好输出有序序列。

## 参考资源

- ISO/IEC 9899:2011 §6.7.2.1(结构声明:成员、自引用指针——指向自身结构类型的指针因指针大小已知而合法)、§7.22.3(内存管理函数:`malloc`/`free`、返回 NULL 表示失败)
- K. N. King《C Programming: A Modern Approach》第 17 章 Linked Lists(17.1 节点声明与自引用结构,直接迁移到二叉树节点;17.5 释放整表的「先存 next 再 free」思想,树的后序释放是其推广)
- Brian W. Kernighan & Dennis M. Ritchie《The C Programming Language》第 6 章 Structures 第 6.5 节(self-referential structures,链表/树的共同根基)
- Robert Sedgewick《Algorithms in C》第 5 章 Recursion and Trees(5.x 递归遍历三种顺序、树的后序释放)
- 阶段3·第1章 单链表(节点、自引用 struct Node*、释放整表的 UAF 坑)、第7章 BST(插入/查找/删除、中序得有序)、第12章 大 O(树高 O(log n) vs 退化链表 O(n))
- 阶段2·第1章 指针是什么(left/right 装子节点地址)、第6章 malloc/free(malloc 节点、必查 NULL)、第7章 动态内存的坑(use-after-free、ASan)、第8章 多级指针(`a->b->c` 链式访问)
- 第 8 章 函数(递归与基线)
