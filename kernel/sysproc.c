#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "date.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "sysinfo.h"
uint64
sys_exit(void)
{
  int n;
  if (argint(0, &n) < 0)
    return -1;
  exit(n);
  return 0; // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  if (argaddr(0, &p) < 0)
    return -1;
  return wait(p);
}

uint64
sys_sbrk(void)
{
  int addr;
  int n;

  if (argint(0, &n) < 0)
    return -1;
  addr = myproc()->sz;
  if (growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  if (argint(0, &n) < 0)
    return -1;
  acquire(&tickslock);
  ticks0 = ticks;
  while (ticks - ticks0 < n)
  {
    if (myproc()->killed)
    {
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  if (argint(0, &pid) < 0)
    return -1;
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}
/*具体实现
分两步进行：
    获取trace的参数，即待追踪系统调用集合mask，
    它保存在当前进程的a0寄存器中，使用argint函数获取修改当前进程状态
trace 做的其实就是把a0放到mask位置，a0就代表33，也就是我们在命令行输入的系统调用*/
uint64
sys_trace(void) // if there exists error,return -1 ,else return 0
{
  int mask;
  struct proc *p = myproc(); // get pcb
  /*关键的函数是 myproc() ，
  这个函数将返回指向当前进程的PCB（也就是进程控制块）的指针（struct proc *），
  里面有程序的各种信息。*/
  if (argint(0, &mask) < 0)  //获取mask参数如果有异常则输出-1   参数从内核态传入用户态传过来，
  {
    return -1;
  }
  p->mask = mask; //把传进来的参数给到现有进程的mask
  return 0;
}

uint64
sys_sysinfo(void)
{
  uint64 addr;
  if (argaddr(0, &addr) < 0)
    return -1;
    //a0给到addr，是用户的虚拟地址
  struct proc *p = myproc();
  struct sysinfo info;
  info.freemem = free_mem();
  info.nproc = n_proc();
  info.freefd = free_fd();
  if (copyout(p->pagetable, addr, (char *)&info, sizeof(info)) < 0) //如果有异常则输出-1
    return -1;
  return 0;
  /*它其实就是把在内核地址src开始的len大小的数据拷贝到用户进程pagetable的虚地址dstva处，
  所以sysinfo实现里先用argaddr读进来我们要保存的在用户态的数据sysinfo的指针地址，
  然后再把从内核里得到的sysinfo开始的内容以sizeof(sysinfo)大小的的数据复制到这个指针上，
  其实就是同一个数据结构  将内核空间数据拷贝的用户空间  相当于一个中转
  把info拷贝到用户进程p的pagetable的虚地址dstva处
  这个地方就是用来存相关的信息
*/
}

// 步骤总结：

    // Makefile 添加
    // user/user.h 中添加系统调用接口（还有补充一个 struct sysinfo）
    // user/usys.pl 添加
    // kernel/syscall.h 添加系统调用号
    // kernel/proc.h：proc 结构体定义添加 mask
    // kernel/proc.c:fork() 代码中添加 mask 的复制
    // kernel/syscall.c 添加 extern 声明和 (*syscalls[])(void)，修改 syscall()，添加打印追踪的代码
    // kernel/sysproc.c 中添加 sys_trace 系统调用
    // kernel/sysproc.c 中添加 sys_sysinfo 系统调用（补充 #include）
    // kernel/kalloc.c 中添加 freemem()
    // kernel/proc.c 中添加 nproc()、freefd()
    // kernel/def.h 中补充 freemem()、nproc()、freefd() 函数定义
