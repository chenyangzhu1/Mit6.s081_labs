#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "elf.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "fs.h"
// #include "vmcopyin.c"
// #ifndef _TEST_H_//文件名大写
// #define _TEST_H_//文件名大写
// #include "vmcopyin.c"
// // int
// // copyin_new(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len);
// // int
// // copyinstr_new(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max);
// #endif
/*
 * the kernel's page table.
 */
pagetable_t kernel_pagetable;

extern char etext[]; // kernel.ld sets this to end of kernel code.i

extern char trampoline[]; // trampoline.S

/*
 * create a direct-map page table for the kernel.
 */
void kvminit()
{
  //为内核创建页表
  kernel_pagetable = (pagetable_t)kalloc();
  memset(kernel_pagetable, 0, PGSIZE);
  //对每个io进行映射
  // uart registers
  kvmmap(UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio disk interface
  kvmmap(VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // CLINT
  kvmmap(CLINT, CLINT, 0x10000, PTE_R | PTE_W);

  // PLIC
  kvmmap(PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // map kernel text executable and read-only.
  kvmmap(KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  // map kernel data and the physical RAM we'll make use of.
  kvmmap((uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  kvmmap(TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
}
//fixme
//要写在调用者上面qaq
//仿照kvmmap，只需要多一个参数，因为我们是手动初始化传入参数，而非全局变量
void mykvmmap(pagetable_t k, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if(mappages(k, va, sz, pa, perm) != 0)
    panic("uvmmap");
}

pagetable_t init_pagetable_of_kernel()
{
  pagetable_t pagetable_of_kernel;
  pagetable_of_kernel=(pagetable_t)kalloc();//按照kvminit进行初始化
  if(pagetable_of_kernel==0)
  {
    panic("init pagetable error");
  }
  memset(pagetable_of_kernel,0,PGSIZE);
  // uart registers
  mykvmmap(pagetable_of_kernel, UART0, UART0, PGSIZE, PTE_R | PTE_W);
  // virtio mmio disk interface
  mykvmmap(pagetable_of_kernel, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);
  // CLINT 按照指导书所说，这里先不包含CLINT
  // mykvmmap(pagetable_of_kernel, CLINT, CLINT, 0x10000, PTE_R | PTE_W);
  // PLIC
  mykvmmap(pagetable_of_kernel, PLIC, PLIC, 0x400000, PTE_R | PTE_W);
  // map kernel text executable and read-only.
  mykvmmap(pagetable_of_kernel, KERNBASE, KERNBASE, (uint64)etext-KERNBASE, PTE_R | PTE_X);
  // map kernel data and the physical RAM we'll make use of.
  mykvmmap(pagetable_of_kernel, (uint64)etext, (uint64)etext, PHYSTOP-(uint64)etext, PTE_R | PTE_W);
  // map the trampoline for trap entry/exit to
  // the highest virtual address in the kernel.
  mykvmmap(pagetable_of_kernel, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);
  return pagetable_of_kernel;
}





// Switch h/w page table register to the kernel's page table,
// and enable paging.
void kvminithart()
{
  w_satp(MAKE_SATP(kernel_pagetable));
  sfence_vma();
}

// Return the address of the PTE in page table pagetable
// that corresponds to virtual address va.  If alloc!=0,
// create any required page-table pages.
//
// The risc-v Sv39 scheme has three levels of page-table
// pages. A page-table page contains 512 64-bit PTEs.
// A 64-bit virtual address is split into five fields:
//   39..63 -- must be zero.
//   30..38 -- 9 bits of level-2 index.
//   21..29 -- 9 bits of level-1 index.
//   12..20 -- 9 bits of level-0 index.
//    0..11 -- 12 bits of byte offset within the page.
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if (va >= MAXVA)
    panic("walk");

  for (int level = 2; level > 0; level--)
  {
    pte_t *pte = &pagetable[PX(level, va)];
    if (*pte & PTE_V)
    {
      pagetable = (pagetable_t)PTE2PA(*pte);
    }
    else
    {
      if (!alloc || (pagetable = (pde_t *)kalloc()) == 0)
        return 0;
      memset(pagetable, 0, PGSIZE);
      *pte = PA2PTE(pagetable) | PTE_V;
    }
  }
  return &pagetable[PX(0, va)];
}

// Look up a virtual address, return the physical address,
// or 0 if not mapped.
// Can only be used to look up user pages.
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;
  if ((*pte & PTE_V) == 0)
    return 0;
  if ((*pte & PTE_U) == 0)
    return 0;
  pa = PTE2PA(*pte);
  return pa;
}

// add a mapping to the kernel page table.
// only used when booting.
// does not flush TLB or enable paging.
void kvmmap(uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(kernel_pagetable, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// translate a kernel virtual address to
// a physical address. only needed for
// addresses on the stack.
// assumes va is page aligned.
// 虚实地址转换函数，walk使用了全局内核页表，这里需要换成当前内核页表
//不知道为啥加了还是报错
//这个点指导书上写的好像没有写。。。。。。
uint64
kvmpa(uint64 va)
{
  uint64 off = va % PGSIZE;
  pte_t *pte;
  uint64 pa;
//fixme
  pte = walk(myproc()->kernel_pagetable, va, 0);
  if (pte == 0)
    panic("kvmpa");
  if ((*pte & PTE_V) == 0)
    panic("kvmpa");
  pa = PTE2PA(*pte);
  return pa + off;
}

// Create PTEs for virtual addresses starting at va that refer to
// physical addresses starting at pa. va and size might not
// be page-aligned. Returns 0 on success, -1 if walk() couldn't
// allocate a needed page-table page.
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 a, last;
  pte_t *pte;

  a = PGROUNDDOWN(va);
  last = PGROUNDDOWN(va + size - 1);
  for (;;)
  {
    if ((pte = walk(pagetable, a, 1)) == 0)
      return -1;
    if (*pte & PTE_V)
      panic("remap");
    *pte = PA2PTE(pa) | perm | PTE_V;
    if (a == last)
      break;
    a += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// Remove npages of mappings starting from va. va must be
// page-aligned. The mappings must exist.
// Optionally free the physical memory.
// If do_free != 0, physical memory will be freed
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 a;
  pte_t *pte;

  if ((va % PGSIZE) != 0)
    panic("uvmunmap: not aligned");

  for (a = va; a < va + npages * PGSIZE; a += PGSIZE)
  {
    if ((pte = walk(pagetable, a, 0)) == 0)
      panic("uvmunmap: walk");
    if ((*pte & PTE_V) == 0)
      panic("uvmunmap: not mapped");
    if (PTE_FLAGS(*pte) == PTE_V)
      panic("uvmunmap: not a leaf");
    if (do_free)
    {
      uint64 pa = PTE2PA(*pte);
      kfree((void *)pa);
    }
    *pte = 0;
  }
}

// create an empty user page table.
// returns 0 if out of memory.
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// Load the user initcode into address 0 of pagetable,
// for the very first process.
// sz must be less than a page.
void uvminit(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;

  if (sz >= PGSIZE)
    panic("inituvm: more than a page");
  mem = kalloc();
  memset(mem, 0, PGSIZE);
  mappages(pagetable, 0, PGSIZE, (uint64)mem, PTE_W | PTE_R | PTE_X | PTE_U);
  memmove(mem, src, sz);
}

// Allocate PTEs and physical memory to grow process from oldsz to
// newsz, which need not be page aligned.  Returns new size or 0 on error.
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  char *mem;
  uint64 a;

  if (newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE)
  {
    mem = kalloc();
    if (mem == 0)
    {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_W | PTE_X | PTE_R | PTE_U) != 0)
    {
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// Deallocate user pages to bring the process size from oldsz to
// newsz.  oldsz and newsz need not be page-aligned, nor does newsz
// need to be less than oldsz.  oldsz can be larger than the actual
// process size.  Returns the new process size.
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if (newsz >= oldsz)
    return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz))
  {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// void
// freewalk(pagetable_t pagetable)
// {
//   // there are 2^9 = 512 PTEs in a page table.
//   for(int i = 0; i < 512; i++){
//     pte_t pte = pagetable[i];
//     if((pte & PTE_V) && (pte & (PTE_R|PTE_W|PTE_X)) == 0){
//       // this PTE points to a lower-level page table.
//       uint64 child = PTE2PA(pte);
//       freewalk((pagetable_t)child);
//       pagetable[i] = 0;
//     } else if(pte & PTE_V){
//       panic("freewalk: leaf");
//     }
//   }
//   kfree((void*)pagetable);
// }

// Recursively free page-table pages.
// All leaf mappings must already have been removed.

void freewalk(pagetable_t pagetable)
{
  // there are 2^9 = 512 PTEs in a page table.
  // 遍历一个页表页的PTE表项
  for (int i = 0; i < 512; i++)
  {
    pte_t pte = pagetable[i]; //获取第i条PTE

    /*判断PTE的Flag位，如果还有下一级页表(即当前是根页表或次页表)，
    则递归调用freewalk释放页表项，并将对应的PTE清零*/
    if ((pte & PTE_V) && (pte & (PTE_R | PTE_W | PTE_X)) == 0)
    {
      // this PTE points to a lower-level page table.
      uint64 child = PTE2PA(pte);   // 将PTE转为为物理地址
      freewalk((pagetable_t)child); // 递归调用freewalk
      pagetable[i] = 0;             // 清零
    }
    else if (pte & PTE_V)
    {
      /*如果叶子页表的虚拟地址还有映射到物理地址，报错panic。
      因为调用freewalk之前应该会先uvmunmap释放物理内存*/
      panic("freewalk: leaf");
    }
  }
  kfree((void *)pagetable); // 释放pagetable指向的物理页
}
// Free user memory pages,
// then free page-table pages.
void uvmfree(pagetable_t pagetable, uint64 sz)
{
  if (sz > 0)
    uvmunmap(pagetable, 0, PGROUNDUP(sz) / PGSIZE, 1);
  freewalk(pagetable);
}

// Given a parent process's page table, copy
// its memory into a child's page table.
// Copies both the page table and the
// physical memory.
// returns 0 on success, -1 on failure.
// frees any allocated pages on failure.
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, i;
  uint flags;
  char *mem;

  for (i = 0; i < sz; i += PGSIZE)
  {
    if ((pte = walk(old, i, 0)) == 0)
      panic("uvmcopy: pte should exist");
    if ((*pte & PTE_V) == 0)
      panic("uvmcopy: page not present");
    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);
    if ((mem = kalloc()) == 0)
      goto err;
    memmove(mem, (char *)pa, PGSIZE);
    if (mappages(new, i, PGSIZE, (uint64)mem, flags) != 0)
    {
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  uvmunmap(new, 0, i / PGSIZE, 1);
  return -1;
}

// mark a PTE invalid for user access.
// used by exec for the user stack guard page.
void uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("uvmclear");
  *pte &= ~PTE_U;
}

// Copy from kernel to user.
// Copy len bytes from src to virtual address dstva in a given page table.
// Return 0 on success, -1 on error.
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 n, va0, pa0;

  while (len > 0)
  {
    va0 = PGROUNDDOWN(dstva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (dstva - va0);
    if (n > len)
      n = len;
    memmove((void *)(pa0 + (dstva - va0)), src, n);

    len -= n;
    src += n;
    dstva = va0 + PGSIZE;
  }
  return 0;
}

// Copy from user to kernel.
// Copy len bytes to dst from virtual address srcva in a given page table.
// Return 0 on success, -1 on error.
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  // uint64 n, va0, pa0;

  // while (len > 0)
  // {
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);
  //   if (pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if (n > len)
  //     n = len;
  //   memmove(dst, (void *)(pa0 + (srcva - va0)), n);

  //   len -= n;
  //   dst += n;
  //   srcva = va0 + PGSIZE;
  // }
  // return 0;
  //fixme
  if(len)
  {
    return copyin_new(pagetable,dst,srcva,len);
  }
  else
  {
    panic("wrong len");
  }
}

// Copy a null-terminated string from user to kernel.
// Copy bytes to dst from virtual address srcva in a given page table,
// until a '\0', or max.
// Return 0 on success, -1 on error.
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{//fixme
  if(max)
  {
    return copyinstr_new(pagetable,dst,srcva,max);
  }
  else
  {
    panic("over max");
  }
  // uint64 n, va0, pa0;
  // int got_null = 0;

  // while (got_null == 0 && max > 0)
  // {
  //   va0 = PGROUNDDOWN(srcva);
  //   pa0 = walkaddr(pagetable, va0);
  //   if (pa0 == 0)
  //     return -1;
  //   n = PGSIZE - (srcva - va0);
  //   if (n > max)
  //     n = max;

  //   char *p = (char *)(pa0 + (srcva - va0));
  //   while (n > 0)
  //   {
  //     if (*p == '\0')
  //     {
  //       *dst = '\0';
  //       got_null = 1;
  //       break;
  //     }
  //     else
  //     {
  //       *dst = *p;
  //     }
  //     --n;
  //     --max;
  //     p++;
  //     dst++;
  //   }

  //   srcva = va0 + PGSIZE;
  // }
  // if (got_null)
  // {
  //   return 0;
  // }
  // else
  // {
  //   return -1;
  // }
  
}

// check if use global kpgtbl or not
int test_pagetable()
{
  uint64 satp = r_satp();
  uint64 gsatp = MAKE_SATP(kernel_pagetable);
  return satp != gsatp;
}

void vmprint(pagetable_t pgtbl)
{
  printf("page table %p\n", pgtbl);
//fixme
  for (int i = 0; i < 512; i++)
  {
    pte_t pte_root = pgtbl[i];
    // pte是一个页表条目
    //这里我没有采用递归的方法，因为感觉太难理解了
    /*考虑到页表一共就三层，就用笨一点的手写三层循环的方式来打印*/
    /*
    PTE_V 即 页表项的Valid，标识该页表项是否有效
    对于叶子页表来说，这个位置为1说明虚拟地址有映射到物理地址，否则说明没有该映射
    对于次页表来说，为1说明有对应的叶子页表，对于根页表说明有对应次页表。
    */
    if ((pte_root & PTE_V)) //有进一步的映射，只打印有效的pte。
    {
      uint64 child_root = PTE2PA(pte_root);
      printf("||%d: pte %p pa %p\n", i, pte_root, child_root);
      for (int j = 0; j < 512; j++)
      {
        pte_t pte_sub = ((pagetable_t)child_root)[j];
        //就是对子页表进行遍历
        //这一步是有点难想，但其实看了指导书就知道这个child root就是下一级的物理地址，j就是遍历过程的偏移，实际上也就是向下一级进行定位
        //后面每一层的逻辑都和第一层一样
        if (pte_sub & PTE_V)
        {
          uint64 child_sub = PTE2PA(pte_sub);
          printf("|| ||%d: pte %p pa %p\n", j, pte_sub, child_sub);

          for (int k = 0; k < 512; k++)
          {
            pte_t pte_leaf = ((pagetable_t)child_sub)[k];
            if (pte_leaf & PTE_V)
            {
              uint64 child_leaf = PTE2PA(pte_leaf);
              printf("|| || ||%d: pte %p pa %p\n", k, pte_leaf, child_leaf);
            }
          }
        }
      }
    }
  }
}