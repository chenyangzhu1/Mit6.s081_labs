// Saved registers for kernel context switches.
struct context {
  uint64 ra;
  uint64 sp;

  // callee-saved
  uint64 s0;
  uint64 s1;
  uint64 s2;
  uint64 s3;
  uint64 s4;
  uint64 s5;
  uint64 s6;
  uint64 s7;
  uint64 s8;
  uint64 s9;
  uint64 s10;
  uint64 s11;
};

// Per-CPU state.
struct cpu {
  struct proc *proc;          // The process running on this cpu, or null.
  struct context context;     // swtch() here to enter scheduler().
  int noff;                   // Depth of push_off() nesting.
  int intena;                 // Were interrupts enabled before push_off()?
};

extern struct cpu cpus[NCPU];

// per-process data for the trap handling code in trampoline.S.
// sits in a page by itself just under the trampoline page in the
// user page table. not specially mapped in the kernel page table.
// the sscratch register points here.
// uservec in trampoline.S saves user registers in the trapframe,
// then initializes registers from the trapframe's
// kernel_sp, kernel_hartid, kernel_satp, and jumps to kernel_trap.
// usertrapret() and userret in trampoline.S set up
// the trapframe's kernel_*, restore user registers from the
// trapframe, switch to the user page table, and enter user space.
// the trapframe includes callee-saved user registers like s0-s11 because the
// return-to-user path via usertrapret() doesn't return through
// the entire kernel call stack.
struct trapframe {
  /*   0 */ uint64 kernel_satp;   // kernel page table
  /*   8 */ uint64 kernel_sp;     // top of process's kernel stack
  /*  16 */ uint64 kernel_trap;   // usertrap()
  /*  24 */ uint64 epc;           // saved user program counter
  /*  32 */ uint64 kernel_hartid; // saved kernel tp
  /*  40 */ uint64 ra;
  /*  48 */ uint64 sp;
  /*  56 */ uint64 gp;
  /*  64 */ uint64 tp;
  /*  72 */ uint64 t0;
  /*  80 */ uint64 t1;
  /*  88 */ uint64 t2;
  /*  96 */ uint64 s0;
  /* 104 */ uint64 s1;
  /* 112 */ uint64 a0;
  /* 120 */ uint64 a1;
  /* 128 */ uint64 a2;
  /* 136 */ uint64 a3;
  /* 144 */ uint64 a4;
  /* 152 */ uint64 a5;
  /* 160 */ uint64 a6;
  /* 168 */ uint64 a7;
  /* 176 */ uint64 s2;
  /* 184 */ uint64 s3;
  /* 192 */ uint64 s4;
  /* 200 */ uint64 s5;
  /* 208 */ uint64 s6;
  /* 216 */ uint64 s7;
  /* 224 */ uint64 s8;
  /* 232 */ uint64 s9;
  /* 240 */ uint64 s10;
  /* 248 */ uint64 s11;
  /* 256 */ uint64 t3;
  /* 264 */ uint64 t4;
  /* 272 */ uint64 t5;
  /* 280 */ uint64 t6;
};

enum procstate { UNUSED, SLEEPING, RUNNABLE, RUNNING, ZOMBIE };

// Per-process state
struct proc {
  struct spinlock lock;

  // p->lock must be held when using these:
  enum procstate state;        // Process state
  struct proc *parent;         // Parent process
  void *chan;                  // If non-zero, sleeping on chan
  int killed;                  // If non-zero, have been killed
  int xstate;                  // Exit status to be returned to parent's wait
  int pid;                     // Process ID

  // these are private to the process, so p->lock need not be held.
  uint64 kstack;               // Virtual address of kernel stack
  uint64 sz;                   // Size of process memory (bytes)
  pagetable_t pagetable;       // User page table
  struct trapframe *trapframe; // data page for trampoline.S
  struct context context;      // swtch() here to run process
  struct file *ofile[NOFILE];  // Open files
  struct inode *cwd;           // Current directory
  char name[16];               // Process name (debugging)
  int ticks;
  int handler;
  int ticks_sum;
  // uint64 epc_temp;

  //   /*   0 */ uint64 kernel_satp_temp;   // kernel page table
  // /*   8 */ uint64 kernel_sp_temp;     // top of process's kernel stack
  // /*  16 */ uint64 kernel_trap_temp;   // usertrap()
  // /*  24 */ uint64 epc_temp;           // saved user program counter
  // /*  32 */ uint64 kernel_hartid_temp; // saved kernel tp
  // /*  40 */ uint64 ra_temp;
  // /*  48 */ uint64 sp_temp;
  // /*  56 */ uint64 gp_temp;
  // /*  64 */ uint64 tp_temp;
  // /*  72 */ uint64 t0_temp;
  // /*  80 */ uint64 t1_temp;
  // /*  88 */ uint64 t2_temp;
  // /*  96 */ uint64 s0_temp;
  // /* 104 */ uint64 s1_temp;
  // /* 112 */ uint64 a0_temp;
  // /* 120 */ uint64 a1_temp;
  // /* 128 */ uint64 a2_temp;
  // /* 136 */ uint64 a3_temp;
  // /* 144 */ uint64 a4_temp;
  // /* 152 */ uint64 a5_temp;
  // /* 160 */ uint64 a6_temp;
  // /* 168 */ uint64 a7_temp;
  // /* 176 */ uint64 s2_temp;
  // /* 184 */ uint64 s3_temp;
  // /* 192 */ uint64 s4_temp;
  // /* 200 */ uint64 s5_temp;
  // /* 208 */ uint64 s6_temp;
  // /* 216 */ uint64 s7_temp;
  // /* 224 */ uint64 s8_temp;
  // /* 232 */ uint64 s9_temp;
  // /* 240 */ uint64 s10_temp;
  // /* 248 */ uint64 s11_temp;
  // /* 256 */ uint64 t3_temp;
  // /* 264 */ uint64 t4_temp;
  // /* 272 */ uint64 t5_temp;
  // /* 280 */ uint64 t6_temp;
  struct trapframe* t_trap;
  int flag;
};
