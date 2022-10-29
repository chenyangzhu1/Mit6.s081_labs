// Physical memory allocator, for user processes,
// kernel stacks, page-table pages,
// and pipe buffers. Allocates whole 4096-byte pages.

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "riscv.h"
#include "defs.h"

void freerange(void *pa_start, void *pa_end);

extern char end[]; // first address after kernel.
                   // defined by kernel.ld.

struct run
{
  struct run *next;
};

struct
{
  struct spinlock lock;
  struct run *freelist;
} kmem[NCPU]; //我们需要多个锁

void kinit() //由于我们拥有了多个锁，自然需要对多个锁同时进行初始化
{
  for (int i = 0; i < NCPU; i++)
  {
    initlock(&kmem[i].lock, "kmem");
    //可以将所有锁命名为kmem。
  }
  // initlock(&kmem.lock, "kmem");
  freerange(end, (void *)PHYSTOP);
/*
查阅资料，这里只用一个freerange的原因是
只有 cpu0 会执行 kinit 以及 freerange，
一开始把所有物理内存分给 cpu0 即可。

这样别的 CPU 执行 kalloc 时会从cpu0 “偷取” 物理内存，kfree 时直接还给这个 CPU 的 freelist，
即谁分配就退还给谁。而且不存在的 CPU 对应的 freelist 一直为空。*/



  // kinit()调用freerange()来把空闲内存页加到链表里，freerange()则是通过调用kfree()把每个空闲页（地址范围从pa_start至pa_end）逐一加到链表里来实现此功能的
}

void freerange(void *pa_start, void *pa_end)
{
  char *p;
  p = (char *)PGROUNDUP((uint64)pa_start);
  for (; p + PGSIZE <= (char *)pa_end; p += PGSIZE)
    kfree(p);
}

// Free the page of physical memory pointed at by v,
// which normally should have been returned by a
// call to kalloc().  (The exception is when
// initializing the allocator; see kinit above.)
void kfree(void *pa)
{
  /*用于释放指定的物理内存页，将其添加至freelist中，
  参数pa为需要释放的物理页页号，即物理页的首地址，它被看作一个没有类型的指针。
  在kfree()中，pa被 强制转换 为run类型的指针，进而可以放入freelist中。
  因为空闲页里什么都没有，所以结构体run的成员可以直接 保存在空闲页自身里 。*/
  struct run *r;

  if (((uint64)pa % PGSIZE) != 0 || (char *)pa < end || (uint64)pa >= PHYSTOP)
    panic("kfree");

  // Fill with junk to catch dangling refs.
  memset(pa, 1, PGSIZE);
  //首先将 void *pa 开始的物理页的内容全部置为1
  r = (struct run *)pa;
  //参考源代码针对多个锁的情况进行修改
  push_off(); // 关中断
  /*使用cpuid()函数会返回当前CPU的id号（范围0~NCPU-1）。
  在调用cpuid()并使用其返回值的过程中需要关闭中断。
  使用push_off()可以关闭中断，使用pop_off()可以打开中断。 */
  int id = cpuid();
  acquire(&kmem[id].lock);
  //获取锁为了操作freelist
  r->next = kmem[id].freelist;
  //将这空闲页物理内存加到链表头。
  kmem[id].freelist = r;
  release(&kmem[id].lock);
  pop_off(); //恢复中断

  // acquire(&kmem.lock);
  // r->next = kmem.freelist;
  // //然后将这空闲页物理内存加到链表头。
  // kmem.freelist = r;
  // release(&kmem.lock);
}

// Allocate one 4096-byte page of physical memory.
// Returns a pointer that the kernel can use.
// Returns 0 if the memory cannot be allocated.
void *
kalloc(void)
{ /*void* kalloc(void *)用来分配内存物理页，功能很简单，
 就是移除并返回空闲链表头的第一个元素，即给调用者分配1页物理内存。*/
  struct run *r;
  push_off(); // disable interrupts to avoid deadlock.
  int id = cpuid();
  acquire(&kmem[id].lock);
  //每次操作kmem.freelist时都需要先申请kmem.lock，此后再进行内存页面的操作。
  r = kmem[id].freelist;
  //当前CPU freelist为空时，从其他CPU freelist处获取

  //先从 CPU 自己对应的 freelist 开始找，然后如果不够再去其他的 CPU 的 freelist 找

  if (r) //当前CPU不为空
  {
    kmem[id].freelist = r->next; //取走表头第一个元素
  }
  else //当前为空
  {
    for (int i = 0; i < NCPU; i++)
    {
      if (i != id) //确保从其他CPU中获取
      {
        acquire(&kmem[i].lock);
        //还是先获取锁
        r = kmem[i].freelist;
        if (r)
        {
          kmem[i].freelist = r->next; //获取内存
          release(&kmem[i].lock);     //解锁并结束for循环
          break;
        }
        else //被遍历的CPU也没有空间，解锁以后继续遍历
        {
          release(&kmem[i].lock);
          //总之得解锁
        }
      }
    }
  }
  release(&kmem[id].lock);
  pop_off(); //恢复中断

  // acquire(&kmem.lock);
  // r = kmem.freelist;
  // if(r)
  //   kmem.freelist = r->next;
  // release(&kmem.lock);

  if (r)
    memset((char *)r, 5, PGSIZE); // fill with junk
  return (void *)r;
}



/*
这个实验干了啥呢
修改内存分配器（主要修改kernel/kalloc.c） ，使每个CPU核使用独立的内存链表，而不是现在的共享链表。

怎么做的
就是给每个cpu分配一段空间，并上一把锁

初始化，对每把锁命名等初始化
然后对每把锁的空间进行分配
kinit()调用freerange()来把空闲内存页加到链表里，freerange()则是通过调用kfree()把每个空闲页（地址范围从pa_start至pa_end）逐一加到链表里来实现此功能的

kfree
这一块修改的逻辑基本参照源代码单锁的逻辑，扩展到多锁的情况下而已。
首先获取当前访问的是哪个cpu，用id作为标识。
将这个cpu对应的kmem[id]锁上，
利用链表指针将空闲页加到表头
链表操作就是，将空闲页的next指向原本链表的表头，然后将表头（freelist对应的地址）设置为该空闲页
从获取id到解锁的整个过程都是关中断，结束以后再开中断


kalloc的设计是  先找自己的freelist，不够的话再去别的cpu的freelist中找

和free类似，首先获取当前访问的是哪个cpu，用id作为标识。
将这个cpu对应的kmem[id]锁上，
用r指向当前的表头
如果不为空，就直接取走（把freelist的首元素设置为下一个）
如果为空，就要解锁然后遍历其他的cpu
重复与上述过程类似的过程，当前访问cpu的序号为i
将这个cpu对应的kmem[i]锁上，
考察这个freelist的表头，如果不为空就取走，为空就解锁看下一个cpu
从获取id，一直到结束遍历所有的cpu，这之间的都是关中断，结束之后再开中断。
*/