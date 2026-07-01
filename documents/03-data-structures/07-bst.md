---
title: "二叉搜索树 BST:左小右大、插入/查找/删除、中序得有序"
description: "阶段3·第7章。给第6章的二叉树加一条规矩——左子树所有值都比根小、右子树所有值都比根大——它就成了二叉搜索树(BST),插入/查找/删除平均 O(log n),是动态集合的经典结构。先真跑 bst_basic.c:插 `5 3 8 1 4 7 9`、中序遍历直接吐出 `1 3 4 5 7 8 9`(BST 的招牌——中序得有序序列),前序 `5 3 1 4 8 7 9` 看树形。再真跑 bst_search.c:查 4 命中(返回节点指针、能读到 data/左右空)、查 6 落空(一路走到 NULL 返回 NULL)、查 5 命中根节点。重点是 bst_delete.c 把删除的三种情况逐个真跑:删叶子 1(直接 free、父节点左指针置 NULL)、删单子节点 3(删 1 后 3 只剩右孩子 4,用 4 顶上 free 3)、删双子节点 8(左 7 右 9 都在,找右子树最小值 9 顶上再删那个后继),再加删根 5(也是双子,后继 9 顶上),每删一次中序验证仍有序。最后 bst_degenerate.c 真跑「升序插入退化成链表」——`{5,3,8,1,4,7,9}` 打乱插树高 2(接近 log2(7)),`{1,2,3,4,5,6,7}` 升序插树高 6(=节点数-1 退化),操作从 O(log n) 滑到 O(n),为第12章大 O 埋伏笔。释放复用第6章后序 free,ASan 复核无泄漏。全 gcc16+clang22 真跑 -std=c11 -Wall,free 加 -fsanitize=address。"
chapter: 3
order: 7
tags:
  - host
  - data-structures
  - pointers
difficulty: intermediate
reading_time_minutes: 14
platform: host
c_standard: [99, 11]
prerequisites:
  - "阶段3·第6章:二叉树基础(节点、前/中/后序遍历、后序释放)"
  - "阶段2·第6/7章:动态内存(malloc/free、ASan)"
  - "第 8 章:函数(递归)"
related:
  - "阶段3·第8章:哈希表(O(1) 查找对照 BST 的 O(log n))、第12章:大 O(平衡 O(log n) vs 退化 O(n))"
---

# 二叉搜索树 BST:左小右大、插入/查找/删除、中序得有序

## 引言:给二叉树加一条规矩

第 6 章我们把二叉树搭起来了——节点揣 `left`/`right` 两个子指针,前/中/后序走一遍,后序释放掉。但那棵树是**手动**连出来的:谁当左子、谁当右子,全靠我们一行行 `root->left = ...` 写死。这样的树能走、能释放,却干不了一件很自然的事——**「这个值在树里吗?」**。随便给你一棵手连的二叉树,你想知道某个值在不在这棵树里,只能把整棵树遍历一遍(O(n)),因为树的形状和值的大小没有任何关系,5 完全可以挂在 2 的左边。

这一章我们给二叉树加**一条规矩**——左子树里所有的值都比根小、右子树里所有的值都比根大,而且对每个节点都成立——它就变成了**二叉搜索树(Binary Search Tree,BST)**。这条规矩看着不起眼,威力却大:现在查找一个值,从根开始,比根小就往左钻、比根大就往右钻,每走一步都能甩掉一半的子树,平均 O(log n) 就能找到(或确认找不到);插入也一样,顺着「左小右大」一路找到该挂的空位;中序遍历更是直接吐出一串**有序序列**——这是 BST 的招牌。BST 是后续好几样东西的地基:第 8 章哈希表用 O(1) 对照它的 O(log n)、第 12 章大 O 收口要拿它当「平衡 O(log n) vs 退化 O(n)」的头号例子。这一章我们把插入、查找、删除(最难)三件事真跑通。

## 性质:左小右大,且层层成立

BST 的性质用一句话讲完:**对任意节点 N,N 左子树里所有节点的值都小于 N 的值,N 右子树里所有节点的值都大于 N 的值,而且这个性质对 N 的每个后代都成立**。注意是「**所有**」——不只是 N 的直接左孩子,而是整棵左子树里随便哪个节点的值都比 N 小。这一点是后面所有操作(插入、查找、删除)的依据。

这条性质带来一个直接的好处:**对一棵 BST 做中序遍历,得到的序列是排好序的(升序)**。回忆第 6 章的中序「左 → 根 → 右」:我们先走完左子树(里面所有值都比根小)、再访问根、再走右子树(所有值都比根大)——这个顺序天然就是「小的先、根居中、大的后」,递归到每一层都成立,所以整串输出就是升序。下面我们插七个数造一棵 BST,中序一跑就会看到这串有序序列,BST 的招牌就在这一刻亮出来。

## 插入:比根小往左、大往右、空了就挂新节点

插入的思路正是「左小右大」性质的自然推论。给一棵 BST 和一个要插的值 `value`:从根开始比,如果 `value < 根`,它就该进左子树(因为左边整个都该比根小),那就递归插进左子树;如果 `value > 根`,它该进右子树,递归插进右子树;递归到某个时刻,子树是空的(NULL)——这就意味着 `value` 找到了自己该挂的位置,新建一个节点挂上去。BST 的插入是「**找到那个唯一的空位**」,所以同一组值、按同一个顺序插入,长出来的树形状是确定的。

节点定义我们直接复用第 6 章那套——自引用的 `struct Node`(成员里必须写全名 `struct Node*`,理由和第 6 章、第 1 章单链表一模一样:typedef 别名在结构体内部还没生效,§6.7.2.1):

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* left;
    struct Node* right;
} Node;

Node* new_node(int value) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        fprintf(stderr, "malloc 失败\n");
        exit(1);
    }
    n->data = value;
    n->left = NULL; /* 新节点是叶子,左右都置 NULL,别让它揣野指针 */
    n->right = NULL;
    return n;
}

/* 插入:比根小往左、大(含相等)往右、空了就挂新节点 */
Node* insert(Node* root, int value) {
    if (root == NULL) {
        return new_node(value); /* 找到空位,这里就是新节点的家 */
    }
    if (value < root->data) {
        root->left = insert(root->left, value);
    } else {
        root->right = insert(root->right, value); /* 相等也放右边 */
    }
    return root;
}

/* 中序遍历:左 -> 根 -> 右(BST 做中序得有序序列) */
void inorder(Node* root) {
    if (root == NULL) {
        return;
    }
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
}

/* 前序遍历:根 -> 左 -> 右(看树的结构用) */
void preorder(Node* root) {
    if (root == NULL) {
        return;
    }
    printf("%d ", root->data);
    preorder(root->left);
    preorder(root->right);
}

/* 后序释放:先删两棵子树、最后删自己(第6章原样复用) */
void free_tree(Node* root) {
    if (root == NULL) {
        return;
    }
    free_tree(root->left);
    free_tree(root->right);
    free(root);
}

int main(void) {
    Node* root = NULL;
    int vals[] = {5, 3, 8, 1, 4, 7, 9};
    int n = sizeof(vals) / sizeof(vals[0]);

    for (int i = 0; i < n; i++) {
        root = insert(root, vals[i]);
    }

    printf("inorder : ");
    inorder(root);
    printf("\n");

    printf("preorder: ");
    preorder(root);
    printf("\n");

    free_tree(root);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall bst_basic.c -o b && ./b
inorder : 1 3 4 5 7 8 9
preorder: 5 3 1 4 8 7 9
```

`inorder : 1 3 4 5 7 8 9` ——我们插进去的顺序明明是 `5 3 8 1 4 7 9`,中序吐出来的却是**升序排列**。这就是 BST 的招牌。原理正是上面说的:中序「左→根→右」配合「左子树全小、右子树全大」,递归到每一层都先把小的走完、再走自己、再走大的,整串自然升序。要查一棵 BST 是不是合法,中序一跑看输出有没有序,是最快的体检办法。

`preorder: 5 3 1 4 8 7 9` 这串是看树形用的——前序「根→左→右」先打印根,所以**第一个数 5 就是整棵树的根**;接着递归打印左子树(`3 1 4`,以 3 为根)、再打印右子树(`8 7 9`,以 8 为根)。把这棵树画出来就是:

```text
          5
        /   \
       3     8
      / \   / \
     1   4 7   9
```

我们手动模拟一下插入过程,看清楚这棵树是怎么长出来的。先插 **5**:树空,`5` 当根。再插 **3**:从根 5 比,`3 < 5` 往左,左子树空,挂 `3` 当 5 的左孩子。再插 **8**:`8 > 5` 往右,右子树空,挂 `8` 当 5 的右孩子。再插 **1**:`1 < 5` 往左到 3,`1 < 3` 继续往左,空,挂 `1` 当 3 的左孩子。**4**:`4 < 5` 往左到 3,`4 > 3` 往右,空,挂 `4` 当 3 的右孩子。**7**:`7 > 5` 往右到 8,`7 < 8` 往左,空,挂 `7` 当 8 的左孩子。**9**:`9 > 5` 往右到 8,`9 > 8` 往右,空,挂 `9` 当 8 的右孩子。七步下来,正好长成上面那棵漂亮的平衡树。

代码里有几处值得停下来想一想。第一,`insert` 的签名是 `Node* insert(Node* root, int value)`、返回 `Node*`,这是 BST 代码的常见写法——它递归地把「插完后的新子树根」返回给上层,上层再用 `root->left = insert(root->left, value)` 接住。这么写的好处是「空树」和「非空树」用同一套逻辑处理(空树就是 `root == NULL`,直接返回新节点),不必特判「第一次插入要建根」这种边界。`main` 里 `root = insert(root, vals[i]);` 每次都接住返回值,正是为了处理「第一次 `root` 还是 NULL」的情形——这一句绝不能省。第二,相等的情况(`value == root->data`)这里走 `else` 分支放右边,意味着**允许重复值、且重复值落在右边**。严格意义上的 BST 通常不允许重复(查找/删除会更干净),教学版这样写够用了;真要做「集合」语义,把相等直接忽略不插就行,改一行 `else if (value > root->data)`。

## 查找:O(log n) 的二分

查找是 BST 最直白的操作,也是它得名「搜索树」的原因。思路和插入几乎一样:从根开始,`target == 根` 就命中、返回这个节点;`target < 根` 往左子树找;`target > 根` 往右子树找;递归下去,要么某一步相等命中、要么一路走到 NULL(「该在的位置是空的」,意味着没这个值,返回 NULL)。每比一次都能甩掉一半的子树——这和阶段1·第 12 章提过的二分查找本质上是同一件事,只不过二分查找的「有序」是数组连续排好的、BST 的「有序」是嵌在树形里的。

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* left;
    struct Node* right;
} Node;

Node* new_node(int value) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        fprintf(stderr, "malloc 失败\n");
        exit(1);
    }
    n->data = value;
    n->left = NULL;
    n->right = NULL;
    return n;
}

Node* insert(Node* root, int value) {
    if (root == NULL) {
        return new_node(value);
    }
    if (value < root->data) {
        root->left = insert(root->left, value);
    } else {
        root->right = insert(root->right, value);
    }
    return root;
}

/* 查找:相等命中、小往左、大往右、走到 NULL 就是没找到 */
Node* search(Node* root, int target) {
    if (root == NULL) {
        return NULL; /* 走到空,没这个值 */
    }
    if (target == root->data) {
        return root; /* 命中,返回指向这个节点的指针 */
    }
    if (target < root->data) {
        return search(root->left, target);
    }
    return search(root->right, target);
}

void inorder(Node* root) {
    if (root == NULL) {
        return;
    }
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
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
    Node* root = NULL;
    int vals[] = {5, 3, 8, 1, 4, 7, 9};
    int n = sizeof(vals) / sizeof(vals[0]);
    for (int i = 0; i < n; i++) {
        root = insert(root, vals[i]);
    }

    printf("inorder: ");
    inorder(root);
    printf("\n");

    /* 查 4:4 < 5 往左到 3,4 > 3 往右到 4,命中 */
    Node* hit4 = search(root, 4);
    printf("search(4): %s\n", hit4 ? "找到" : "没找到");
    if (hit4) {
        printf("  hit->data = %d\n", hit4->data);
        printf("  hit->left = %s, hit->right = %s\n", hit4->left ? "非空" : "空",
               hit4->right ? "非空" : "空");
    }

    /* 查 6:6 > 5 往右到 8,6 < 8 往左到 7,6 < 7 往左是 NULL,落空 */
    Node* hit6 = search(root, 6);
    printf("search(6): %s\n", hit6 ? "找到" : "没找到");

    /* 查 5:直接命中根节点 */
    Node* hit5 = search(root, 5);
    printf("search(5): %s (根节点)\n", hit5 ? "找到" : "没找到");

    free_tree(root);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall bst_search.c -o s && ./s
inorder: 1 3 4 5 7 8 9
search(4): 找到
  hit->data = 4
  hit->left = 空, hit->right = 空
search(6): 没找到
search(5): 找到 (根节点)
```

查 **4**:从根 5 比,`4 < 5` 往左到 3;`4 > 3` 往右到 4;`4 == 4` 命中,返回指向那个节点的指针。`hit->data = 4`、`hit->left/right` 都是空(因为 4 是叶子),印证我们确实拿到了节点本身、能隔着指针读到它的字段——这又一次是阶段2·Ch1「`*p`/`->` 顺着地址走」的实战。查 **6**:从根 5 比,`6 > 5` 往右到 8;`6 < 8` 往左到 7;`6 < 7` 继续往左——可 7 没有左孩子(NULL),`search` 命中 `root == NULL` 基线,返回 NULL,「没找到」。这正是「6 该在的位置是空的」——如果我们要插 6,它就会挂在 7 的左边;但现在那里是空,所以没找到。查 **5** 第一步就在根上相等,直接返回根节点。

这里有个工程上的小细节值得提一句:`search` 找到节点后返回的是指向那个节点的**指针**,调用者拿到它就能读写 `hit->data`(写就改了树里的值)。如果不想让调用者乱改树的内容,可以把返回类型写成 `const Node*`,把「只读契约」焊死(阶段2·Ch4 的 const 那套)。教学版用裸 `Node*` 够直白,真做库就会上 const。

## 删除:三种情况,逐个拆透

删除是 BST 三大操作里最绕的一个,因为删一个节点可能打破「左小右大」的结构,得想办法把缺口补上。难点完全集中在「被删的节点有两个孩子」这种情况。我们按难度从易到难,把三种情况逐个真跑。整体框架是一个递归 `delete(Node* root, int value)`:先像查找一样,顺着「左小右大」找到要删的那个节点;找到之后,根据它有几个孩子分情况处理;函数返回「删完之后的新子树根」,上层用 `root->left/right = delete(...)` 接住——和 `insert` 同款的「返回新根」写法。

**情况一:被删的是叶子(没有孩子)。** 这是最干脆的——叶子挂在树的最末端,删掉它不影响任何人,直接 `free` 掉、让它父亲指向它的那个指针变成 NULL 就行。代码上,`free(root); return NULL;`——返回 NULL 就是告诉上层「这个位置以后是空的了」,上层接住后父亲对应的 `left`/`right` 自然置空。

**情况二:被删的节点只有一个孩子(只有左孩子,或只有右孩子)。** 孩子不能跟着一起没爹——我们把这个孩子「顶上来」,替代被删节点的位置。具体做法:先用临时指针把孩子存住(`Node* child = root->right;`),再 `free(root)` 释放被删节点,然后 `return child;` 把孩子交还给上层、让孩子接替这个位置。为什么必须先用临时变量存住孩子?因为 `free(root)` 之后 `root->right` 那块内存已经还给系统了(那是 `root` 自己的字段,不是孩子的内存),再去读 `root->right` 就是阶段2·Ch7、第 6 章讲过的 use-after-free——孩子地址丢了。先存住再 free,顺序就对了。

**情况三:被删的节点有两个孩子。** 这是真正的难点。两个孩子都在,谁顶上来?随便挑一个孩子顶上来都会破坏「左小右大」——比如直接拿右孩子顶,那原来左孩子那整棵子树就挂到了一个比它大的根下面,性质破坏。正确做法是找一个**正好能填补这个位置**的值顶上:这个值既要大于左子树里所有的值、又要小于右子树里所有的值。这样的值有两个候选——**左子树里的最大值**(左子树里最靠右、最大的那个,比左子树其他都大、又比右子树全小),或**右子树里的最小值**(右子树里最靠左、最小的那个,比左子树全大、又比右子树其他都小)。两者都行,我们用「**右子树最小值**」(也叫被删节点的**后继**,successor)。

具体步骤是:先在右子树里一路向左找到最小值节点(它的值就是要顶上来的后继值),把这个值**拷贝**到被删节点里(`root->data = successor->data;`,被删节点的值被覆盖成后继值),然后**去右子树里删掉那个原本持有后继值的节点**(`root->right = delete(root->right, successor->data);`)。注意第二步是递归调用 `delete`,而那个后继节点在右子树里一定「最多只有一个右孩子」(它已经是右子树里最小的,所以它不可能还有左孩子)——于是它必然落进情况一或情况二,递归一层就能删掉。这是个很漂亮的化归:把最难的情况三,化解成已经解决的情况一或情况二。

把整套逻辑写出来,逐个情况真跑:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* left;
    struct Node* right;
} Node;

Node* new_node(int value) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        fprintf(stderr, "malloc 失败\n");
        exit(1);
    }
    n->data = value;
    n->left = NULL;
    n->right = NULL;
    return n;
}

Node* insert(Node* root, int value) {
    if (root == NULL) {
        return new_node(value);
    }
    if (value < root->data) {
        root->left = insert(root->left, value);
    } else {
        root->right = insert(root->right, value);
    }
    return root;
}

/* 找以 root 为根的子树里的最小值节点:一路向左走到底 */
Node* find_min(Node* root) {
    while (root->left != NULL) {
        root = root->left;
    }
    return root;
}

/* 删除值为 value 的节点,返回删完之后的新子树根 */
Node* delete(Node* root, int value) {
    if (root == NULL) {
        return NULL; /* 没找到,空子树原样返回 */
    }
    if (value < root->data) {
        root->left = delete(root->left, value);
    } else if (value > root->data) {
        root->right = delete(root->right, value);
    } else {
        /* 命中要删的节点,分三种情况 */
        if (root->left == NULL && root->right == NULL) {
            free(root); /* 情况一:叶子,直接删 */
            return NULL;
        }
        if (root->left == NULL) {
            Node* child = root->right; /* 情况二:只有右孩子,先存住 */
            free(root);
            return child; /* 用右孩子顶上来 */
        }
        if (root->right == NULL) {
            Node* child = root->left; /* 情况二:只有左孩子,先存住 */
            free(root);
            return child; /* 用左孩子顶上来 */
        }
        /* 情况三:两个孩子都在。找右子树最小值(后继)顶上来,再去右子树删掉它 */
        Node* successor = find_min(root->right);
        root->data = successor->data;                       /* 后继的值拷上来 */
        root->right = delete(root->right, successor->data); /* 再删掉后继节点 */
    }
    return root;
}

void inorder(Node* root) {
    if (root == NULL) {
        return;
    }
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
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
    /* 初始结构(和 bst_basic.c 同一棵):
     *          5
     *        /   \
     *       3     8
     *      / \   / \
     *     1   4 7   9
     * 1/4/7/9 是叶子,3/5/8 是双子节点。 */
    Node* root = NULL;
    int vals[] = {5, 3, 8, 1, 4, 7, 9};
    int n = sizeof(vals) / sizeof(vals[0]);
    for (int i = 0; i < n; i++) {
        root = insert(root, vals[i]);
    }

    printf("初始 inorder:     ");
    inorder(root);
    printf("\n");

    /* 情况一:删叶子 1 —— 没有孩子,直接 free,父节点 3 的左指针置 NULL */
    root = delete(root, 1);
    printf("删 1 (叶子):      ");
    inorder(root);
    printf("\n");

    /* 现在 3 只剩右孩子 4(1 刚被删),变成「单子节点」 */
    /* 情况二:删 3 —— 只有右孩子 4,用 4 顶上来,free 3 */
    root = delete(root, 3);
    printf("删 3 (单子):      ");
    inorder(root);
    printf("\n");

    /* 情况三:删 8 —— 8 还有左 7、右 9,是双子节点,
       找右子树最小值 9 顶上来,再去右子树删那个 9 */
    root = delete(root, 8);
    printf("删 8 (双子):      ");
    inorder(root);
    printf("\n");

    /* 再来一个情况三:删根 5 —— 此时 5 有左 4、右 9,双子节点,后继 9 顶上 */
    root = delete(root, 5);
    printf("删 5 (根,双子):   ");
    inorder(root);
    printf("\n");

    free_tree(root);
    return 0;
}
```

```text
$ gcc -std=c11 -Wall bst_delete.c -o d && ./d
初始 inorder:     1 3 4 5 7 8 9
删 1 (叶子):      3 4 5 7 8 9
删 3 (单子):      4 5 7 8 9
删 8 (双子):      4 5 7 9
删 5 (根,双子):   4 7 9
```

我们跟着 `inorder` 的输出逐个验证每次删除后树是不是还合法(中序仍有序,就说明「左小右大」没破)。初始 `1 3 4 5 7 8 9` 有序。**删叶子 1**(情况一):1 是叶子、没有孩子,直接 free、让父亲 3 的左指针置 NULL,树变成「3 有右孩子 4、无左孩子」。中序 `3 4 5 7 8 9`,仍有序——3 没了左子树,但中序照样先走空左子树(直接返回)、再打印 3、再走右子树 4,顺序不变。

**删 3**(情况二):因为上一步 1 被删了,现在 3 只剩右孩子 4,是「单子节点」。用 `Node* child = root->right;`(child 指向 4)存住孩子,`free(3)`,`return child;`——4 顶上来接替 3 的位置,成为 5 的左孩子。中序 `4 5 7 8 9`,有序。注意这一步把「先存孩子再 free」的纪律执行到位了:如果先 `free(3)` 再去读 `root->right` 想拿孩子地址,那就是 use-after-free——3 这块内存已还给系统,它的 `right` 字段读不到了。

**删 8**(情况三):8 还有左孩子 7、右孩子 9,是「双子节点」。按我们的策略,找右子树最小值顶上——8 的右子树只有 9 一个节点,`find_min` 一路向左走到底,最小值就是 **9**。把 9 这个值拷到 8 的位置(`root->data = 9`),再去右子树里删掉原本那个 9(`root->right = delete(root->right, 9);`)。那个原本的 9 是叶子(情况一),free 掉、返回 NULL,于是新顶上来的 9 节点(原 8 那块内存,值已改成 9)的右指针置空。中序 `4 5 7 9`,有序——少了 8,但 7 还在 9(原 8 位置)的左边,左小右大成立。

**删根 5**(再来一次情况三):此时 5 有左孩子 4、右孩子 9(原 8 那块、值已改成 9),还是双子。`find_min(右子树)`:从 9 往左,9 没有左孩子,最小值就是 **9** 自己。把 9 拷到 5 的位置,再去右子树删那个 9——又是一个叶子(情况一)。中序 `4 7 9`,有序,这棵树从头到尾删了四个节点都保持合法。

整套删除的真功夫就在情况三那一手「化归」:**找后继 → 拷值 → 递归删后继**,把「两个孩子都在」这个看似无从下手的情况,转成「叶子或单子」这种已经能处理的情况。代码看着不长,思想却很漂亮。

## 退化:有序插入,BST 退化成链表

到这里你可能会觉得 BST 简直完美——插入、查找、删除都 O(log n)。但这个 O(log n) 有个前提:**树得比较「平衡」**(每层的节点数接近填满,高度接近 log₂ n)。回想我们刚才插 `5 3 8 1 4 7 9` 长出的那棵树,高度是 2(根 5 是第 0 层,3/8 是第 1 层,1/4/7/9 是第 2 层),七个节点正好填满三层,高度等于 ⌊log₂ 7⌋ = 2,漂亮。但插入顺序如果换一下呢?

BST 的形状完全由**插入顺序**决定。如果数据本来就有序,你按升序一个一个插进去,每个新值都比之前所有值大,只能一路往右挂——于是树就长成了一条**向右倾斜的链表**,根本没有分叉。这时候「左小右大」性质虽然还成立,但 BST 的所有 O(log n) 优势全废了:查最后一个值得从根一路走到链表尾,走 n 步;插入新值(更大)也得走到链表尾才挂上去,也是 n 步。这就是 BST 的**退化(degenerate)**,操作从 O(log n) 滑到了 O(n)。我们真跑两种插入顺序对照,直接打印树高看差距:

```c
#include <stdio.h>
#include <stdlib.h>

typedef struct Node {
    int data;
    struct Node* left;
    struct Node* right;
} Node;

Node* new_node(int value) {
    Node* n = malloc(sizeof(Node));
    if (n == NULL) {
        fprintf(stderr, "malloc 失败\n");
        exit(1);
    }
    n->data = value;
    n->left = NULL;
    n->right = NULL;
    return n;
}

Node* insert(Node* root, int value) {
    if (root == NULL) {
        return new_node(value);
    }
    if (value < root->data) {
        root->left = insert(root->left, value);
    } else {
        root->right = insert(root->right, value);
    }
    return root;
}

/* 求树高:空树 -1,单节点 0,其余取左右子树较高者 +1 */
int height(Node* root) {
    if (root == NULL) {
        return -1;
    }
    int hl = height(root->left);
    int hr = height(root->right);
    return (hl > hr ? hl : hr) + 1;
}

void inorder(Node* root) {
    if (root == NULL) {
        return;
    }
    inorder(root->left);
    printf("%d ", root->data);
    inorder(root->right);
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
    /* 场景一:打乱顺序插入 7 个数,树比较「矮胖」 */
    Node* balanced = NULL;
    int shuffled[] = {5, 3, 8, 1, 4, 7, 9};
    int n1 = sizeof(shuffled) / sizeof(shuffled[0]);
    for (int i = 0; i < n1; i++) {
        balanced = insert(balanced, shuffled[i]);
    }
    int hb = height(balanced);
    printf("打乱顺序插入 {5,3,8,1,4,7,9}:\n");
    printf("  inorder: ");
    inorder(balanced);
    printf("\n");
    printf("  height  = %d (节点数 %d,接近 log2(%d)=%d)\n", hb, n1, n1, 2);
    free_tree(balanced);

    /* 场景二:按升序插入 1..7,树退化成向右倾斜的链表 */
    Node* degenerate = NULL;
    int sorted[] = {1, 2, 3, 4, 5, 6, 7};
    int n2 = sizeof(sorted) / sizeof(sorted[0]);
    for (int i = 0; i < n2; i++) {
        degenerate = insert(degenerate, sorted[i]);
    }
    int hd = height(degenerate);
    printf("升序插入 {1,2,3,4,5,6,7}:\n");
    printf("  inorder: ");
    inorder(degenerate);
    printf("\n");
    printf("  height  = %d (退化成链表,高度 = 节点数-1 = %d)\n", hd, n2 - 1);
    free_tree(degenerate);

    return 0;
}
```

```text
$ gcc -std=c11 -Wall bst_degenerate.c -o deg && ./deg
打乱顺序插入 {5,3,8,1,4,7,9}:
  inorder: 1 3 4 5 7 8 9
  height  = 2 (节点数 7,接近 log2(7)=2)
升序插入 {1,2,3,4,5,6,7}:
  inorder: 1 2 3 4 5 6 7
  height  = 6 (退化成链表,高度 = 节点数-1 = 6)
```

两组都是七个节点,形状却天差地别。打乱顺序的那组高度 **2**(七节点三层,接近 log₂ 7 ≈ 2.8 的理想值);升序那组高度 **6**——七个节点摞成一条 7 层的链(高度 = 节点数 − 1 = 6),完全没有分叉。`height` 的定义是「根到最远叶子的边数」,空树约定为 −1、单节点为 0,所以一条 7 节点的链高度是 6、一棵三层满的二叉树高度是 2。升序那组查最大的 7,得从根 1 一路向右走过 2、3、4、5、6,六步才到——和链表遍历一模一样,O(n)。

这个退化不是 BST 理论本身的锅,而是「裸 BST 没有任何自平衡机制」的锅。真实数据里,「大致有序」的输入太常见了(先来的数据往往偏小、后来的偏大),裸 BST 在生产环境基本不直接用——要么用**自平衡 BST**(AVL 树、红黑树),它们在插入删除时主动旋转、把树高钉死在 O(log n);要么干脆用第 8 章的哈希表(平均 O(1),但不保序)。BST 的价值在于它是「动态有序集合」最朴素的形态,把插入/查找/删除的逻辑讲清楚、把「中序得有序」这个招牌亮出来,后续的平衡树、数据库索引、C++ `std::map`/`std::set`(底层是红黑树)都是在这套骨架上加自平衡。这一章我们先把裸 BST 的所有操作真跑通,第 12 章大 O 收口时会回来拿这个「退化成 O(n)」当反面教材,和平衡的 O(log n) 做对比。

## 释放:后序 free,第 6 章原样复用

释放整棵 BST 的逻辑和第 6 章普通二叉树一模一样——**必须后序**:先递归 free 左子树、再递归 free 右子树、最后 free 根自己。原因第 6 章讲透过了:根的 `left`/`right` 字段存着子树在哪,先 free 根就丢了这些地址、再也找不到子树,直接 use-after-free(gcc 编译期 `-Wuse-after-free` + ASan 运行期 `heap-use-after-free READ of size 8` 双重抓)。后序写法把顺序安排好——等轮到 free 根的时候,它下面的子树已经全部处理完,根彻底没用了,安心归还。本章四个程序里的 `free_tree` 都是这一套,我们用 ASan 在 `bst_delete.c`(删删插插最容易出内存错)上复核一遍:

```text
$ gcc -std=c11 -Wall -fsanitize=address bst_delete.c -o d_asan && ./d_asan; echo $?
初始 inorder:     1 3 4 5 7 8 9
删 1 (叶子):      3 4 5 7 8 9
删 3 (单子):      4 5 7 8 9
删 8 (双子):      4 5 7 9
删 5 (根,双子):   4 7 9
0
```

退出码 `0`、没有任何 `detected memory leaks` 报告。`delete` 里 `free` 掉被删节点、`free_tree` 后序回收剩余节点,所有 `malloc` 来的节点都被妥善 `free`——包括删叶子时直接 free 的、删单子时 free 后用孩子接住的、删双子时通过递归 `delete` 顺带回收后继的,一个都没漏。`delete` 没有像 `insert` 那样在 `main` 结尾再统一 free,因为每次 `delete` 都即时 free 了它该删的那个节点(情况一/二直接 free、情况三在递归删后继时 free);最后 `free_tree` 把删完之后还留在树上的那些节点扫尾。这一节没什么新东西,关键是别忘了:树的生命周期以「最后一个 `free`」收尾,哪怕你做了再多删除操作,只要树还在用(还能遍历、还能查),就得在不用的时候 `free_tree(root)` 整棵回收。

## 小结

二叉搜索树就是第 6 章的二叉树加上「左子树所有值 < 根 < 右子树所有值」这一条规矩(且层层成立),插入/查找/删除平均都能在树高内完成,即 O(log n)。插入(`insert`)是「比根小往左、大往右、递归到空位就挂新节点」,递归返回新子树根、上层用 `root->left = insert(...)` 接住,同一套逻辑天然处理「空树建根」的边界;查找(`search`)和插入同构,相等命中返回节点指针、走到 NULL 返回 NULL,每比一次甩掉一半子树,本质是嵌在树形里的二分。中序遍历是 BST 的招牌——「左→根→右」配合「左全小、右全大」,递归到每层都先小后大,整串输出就是升序(真跑 `5 3 8 1 4 7 9` 中序得 `1 3 4 5 7 8 9`),这也是检验一棵 BST 是否合法的最快办法。删除(`delete`)最绕,分三种情况逐个拆透:叶子直接 free 置 NULL(情况一);单子节点先用临时变量存住孩子再 free、让孩子顶上(情况二,「先存孩子再 free」是纪律,否则 use-after-free);双子节点找右子树最小值(后继)拷上来、再递归删后继(情况三),后继在右子树里最多只有一个右孩子,于是化归成情况一或二——这一手「化归」是整个删除算法最漂亮的地方,把最难的状况转成已解决的状况。退化的坑要记牢:BST 的形状由插入顺序决定,有序数据按升序插入会让树退化成向右倾斜的链表(真跑升序插 `1..7` 树高 6 = 节点数−1,对照打乱插入树高 2 ≈ log₂ 7),所有操作从 O(log n) 滑到 O(n)——这是裸 BST 没有自平衡机制的固有问题,生产环境用 AVL/红黑树解决,第 12 章大 O 收口时回来拿它当反面教材。释放复用第 6 章的后序 `free_tree`(先递归 free 左右子树、最后 free 根,顺序反了就读不到 `root->left/right` 直接 UAF),ASan 在所有程序上复核退出码 0 无泄漏。节点结构是阶段3·Ch1 单链表那套自引用 `struct Node*` 的延续(§6.7.2.1),`malloc`(§7.22.3)之后立刻查 NULL 并把左右指针置 NULL,`free` 后置 NULL 防悬垂——这些第 6 章立下的规矩本章一条不少。下一章我们换一种思路做「查找」——哈希表,用空间换时间把平均复杂度压到 O(1),对照 BST 的 O(log n),看两种取舍各自的甜区。

## 参考资源

- ISO/IEC 9899:2011 §6.7.2.1(结构声明:成员、自引用指针 `struct Node*`——指向自身结构类型的指针因指针大小已知而合法,与第 6 章、第 1 章单链表同根)、§7.22.3(内存管理函数:`malloc`/`free`、返回 NULL 表示失败)
- K. N. King《C Programming: A Modern Approach》第 17 章 Advanced Uses of Pointers(17.5 节点声明与自引用结构、链表的「先存 next 再 free」思想——BST 的节点定义和后序释放是其直接迁移;King 用链表讲透自引用结构,BST 的递归插入/删除是同一套指针手艺的非线性推广)
- Brian W. Kernighan & Dennis M. Ritchie《The C Programming Language》第 6 章 Structures 第 6.5 节(self-referential structures,自引用结构是链表与树的共同根基)
- Robert Sedgewick《Algorithms in C》第 12 章 Symbol Tables / 第 13 章 Balanced Search Trees(BST 插入/查找/删除三情况、中序得有序、退化成链表、自平衡引子——算法侧的权威讲法,本章 C 实现对照它)
- 阶段3·第 1 章 单链表(节点、自引用 `struct Node*`、释放整表的 UAF 坑——BST 的节点和 `free_tree` 是其非线性推广)、第 6 章 二叉树基础(节点、前/中/后序遍历、后序释放——本章的奠基,直接复用)、第 8 章 哈希表(O(1) 查找对照 BST 的 O(log n))、第 12 章 大 O(平衡 O(log n) vs 退化 O(n),本章退化实锤的反面教材)
- 阶段2·第 1 章 指针是什么(`left`/`right` 装子节点地址、`root->left->right` 链式访问)、第 4 章 const 限定(`search` 返回 `const Node*` 焊死只读契约)、第 6/7 章 malloc/free 与 ASan(malloc 必查 NULL、`free` 后置 NULL、use-after-free 是情况二「先存孩子再 free」纪律的依据)
- 第 8 章 函数(递归与基线——`insert`/`search`/`delete`/`free_tree` 全是递归,`root == NULL` 是基线)
