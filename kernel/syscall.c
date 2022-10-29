#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"
#include "defs.h"


//The kernel-space code

// Fetch the uint64 at addr from the current process.
int
fetchaddr(uint64 addr, uint64 *ip)
{
  struct proc *p = myproc();
  if(addr >= p->sz || addr+sizeof(uint64) > p->sz)
    return -1;
  if(copyin(p->pagetable, (char *)ip, addr, sizeof(*ip)) != 0)
    return -1;
  return 0;
}

// Fetch the nul-terminated string at addr from the current process.
// returns length of string, not including nul, or -1 for error.
int
fetchstr(uint64 addr, char *buf, int max)
{
  struct proc *p = myproc();
  int err = copyinstr(p->pagetable, buf, addr, max);
  if(err < 0)
    return err;
  return strlen(buf);
}

static uint64
argraw(int n)
{
  struct proc *p = myproc();
  switch (n) {
  case 0:
    return p->trapframe->a0;
  case 1:
    return p->trapframe->a1;
  case 2:
    return p->trapframe->a2;
  case 3:
    return p->trapframe->a3;
  case 4:
    return p->trapframe->a4;
  case 5:
    return p->trapframe->a5;
  }
  panic("argraw");
  return -1;
}

// Fetch the nth 32-bit system call argument.
int
argint(int n, int *ip)
{
  *ip = argraw(n);
  return 0;
}

// my_retrieve an argument as a pointer.
// Doesn't check for legality, since
// copyin/copyout will do that.
int
argaddr(int n, uint64 *ip)
{
  *ip = argraw(n);
  return 0;
}

// Fetch the nth word-sized system call argument as a null-terminated string.
// Copies into buf, at most max.
// returns string length if OK (including nul), -1 if error.
int
argstr(int n, char *buf, int max)
{
  uint64 addr;
  if(argaddr(n, &addr) < 0)
    return -1;
  return fetchstr(addr, buf, max);
}
//函数接口
/*前者实际上声明了这些函数，
这些函数的实现不必在这个文件中，
而是分布在各个相关的代码文件中（一般放在sys开头的文件中，
包括sysproc.c与sysfile.c），
我们在这些代码文件中实现好对应的函数，
最后就可以编译出对应名字的汇编代码函数，
 extern 就会找到对应的函数实现了。*/
extern uint64 sys_chdir(void);
extern uint64 sys_close(void);
extern uint64 sys_dup(void);
extern uint64 sys_exec(void);
extern uint64 sys_exit(void);
extern uint64 sys_fork(void);
extern uint64 sys_fstat(void);
extern uint64 sys_getpid(void);
extern uint64 sys_kill(void);
extern uint64 sys_link(void);
extern uint64 sys_mkdir(void);
extern uint64 sys_mknod(void);
extern uint64 sys_open(void);
extern uint64 sys_pipe(void);
extern uint64 sys_read(void);
extern uint64 sys_sbrk(void);
extern uint64 sys_sleep(void);
extern uint64 sys_unlink(void);
extern uint64 sys_wait(void);
extern uint64 sys_write(void);
extern uint64 sys_uptime(void);
extern uint64 sys_trace(void);
extern uint64 sys_sysinfo(void);//添加trace和info
//数组

/*后者则是将这些函数的指针都放在统一的数组里，
并且数组下标就是系统调用号，
这样我们在分辨不同系统调用的时候就可以很方便地用数组来进行操作了。
kernel/syscall.c中的 syscall() 函数就根据这一方法实现了系统调用的分发
（通过不同系统调用号调用不同系统调用函数）*/
static uint64 (*syscalls[])(void) = {
[SYS_fork]    sys_fork,
[SYS_exit]    sys_exit,
[SYS_wait]    sys_wait,
[SYS_pipe]    sys_pipe,
[SYS_read]    sys_read,
[SYS_kill]    sys_kill,
[SYS_exec]    sys_exec,
[SYS_fstat]   sys_fstat,
[SYS_chdir]   sys_chdir,
[SYS_dup]     sys_dup,
[SYS_getpid]  sys_getpid,
[SYS_sbrk]    sys_sbrk,
[SYS_sleep]   sys_sleep,
[SYS_uptime]  sys_uptime,
[SYS_open]    sys_open,
[SYS_write]   sys_write,
[SYS_mknod]   sys_mknod,
[SYS_unlink]  sys_unlink,
[SYS_link]    sys_link,
[SYS_mkdir]   sys_mkdir,
[SYS_close]   sys_close,
[SYS_trace]   sys_trace, 
[SYS_sysinfo] sys_sysinfo,
};

/*这里有几个点，一个是mask是按位判断的，第二个是proc结构体里的name是整个线程的名字，
不是函数调用的函数名称，所以我们不能用p->name，而要自己定义一个数组：*/

static char *syscallname[] = {
[SYS_fork]    "sys_fork",//添加一个系统调用对应名字的数组
[SYS_exit]    "sys_exit",
[SYS_wait]    "sys_wait",
[SYS_pipe]    "sys_pipe",
[SYS_read]    "sys_read",
[SYS_kill]    "sys_kill",
[SYS_exec]    "sys_exec",
[SYS_fstat]   "sys_fstat",
[SYS_chdir]   "sys_chdir",
[SYS_dup]     "sys_dup",
[SYS_getpid]  "sys_getpid",
[SYS_sbrk]    "sys_sbrk",
[SYS_sleep]   "sys_sleep",
[SYS_uptime]  "sys_uptime",
[SYS_open]    "sys_open",
[SYS_write]   "sys_write",
[SYS_mknod]   "sys_mknod",
[SYS_unlink]  "sys_unlink",
[SYS_link]    "sys_link",
[SYS_mkdir]   "sys_mkdir",
[SYS_close]   "sys_close",
[SYS_trace]   "sys_trace",
[SYS_sysinfo] "sys_sysinfo",
};



/*在进入内核模式之后，
应该会调用kernel/syscall.c中的syscall函数，
在这里，我们会通过一个函数数组执行对应在sysfile.c里定义的各种系统调用函数，
我们通过保存在进程的trapframe中的寄存器a7和a0在进程的内核态与用户态之间进行交流。
为了具体实现trace，我们可以将掩码信息保存在进程控制块里：*/
void
syscall(void)
{
  int num;
  struct proc *p = myproc();
  //何时调用trace  看user
  num = p->trapframe->a7;//获取系统调用号

  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
    //divide former sentence
    int my_ret=syscalls[num]();//get the return value
    //进行系统调用
    
    int my_arg=p->trapframe->a0;//get the first parameter from the sys
    p->trapframe->a0=my_ret;//store the fire parameter
    if(1<<num & p->mask)//进程的执行过程有很多的系统调用，我们这里需要判断是否是我们需要trace的那一个调用
    /*判断其是否在当前进程的mask中，
    根据mask的掩码特性，使用与运算就能轻松实现这个过程
    1左移，然后和mask那个位与，如果是1则在*/
    {
      printf("%d: %s(%d) -> %d\n",p->pid,syscallname[num],my_arg,my_ret);
      //<pid>: syscall <syscall_name> -> <return_value>
      //PID: sys_read(read系统调用的arg0) -> read系统调用的return_value
    }//print
    // p->trapframe->a0 = syscalls[num]();
  } else {
    printf("%d %s: unknown sys call %d\n",
            p->pid, p->name, num);
    p->trapframe->a0 = -1;
  }
}




/*实验二整体思路
trace
trace做的事情其实就是把输入的第一个参数 trapframe->a0放到pcb的mask中
然后在syscall的时候利用mask来判断进程是否需要追踪，需要追踪就打印输出

运行
从user/trace开始 利用ecall进入内核态，usertrap 去syscall
在syscall里面

先获取调用号

  if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
判断是否有效

利用这条语句执行系统调用
    int my_ret=syscalls[num]();//get the return value


首先应该是trace，因为trace是父进程，在trace里面还开了grep的子进程

调用了trace的系统调用，到sysproc里面，就是给mask赋值为a0。
对于其他的系统调用，利用这个判断是否是我们所需要的
    if(1<<num & p->mask)//进程的执行过程有很多的系统调用，我们这里需要判断是否是我们需要trace的那一个调用
如果是就打印


sysinfo

基本思路就是实现三个函数，然后在sysproc里面把他们用起来，
加上核心态到用户态的传递

kalloc.c 中添加 freemem()
proc.c 中添加 nproc()、freefd()
def.h 中补充 freemem()、nproc()、freefd() 函数定义
*/










// void
// syscall(void)
// {
//   int num;
//   struct proc *p = myproc();

//   num = p->trapframe->a7;//读取系统调用号
//   if(num > 0 && num < NELEM(syscalls) && syscalls[num]) {
//     p->trapframe->a0 = syscalls[num]();
  /*这一行就是调用了系统调用命令，
  并且把返回值保存在了a0寄存器中（RISCV的C规范是把返回值放在a0中)，
  所以我们只要在调用系统调用时判断是不是mask规定的输出函数，如果是就输出。*/
//   } else {
//     printf("%d %s: unknown sys call %d\n",
//             p->pid, p->name, num);
//     p->trapframe->a0 = -1;
//   }
// }
