#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"
#include "vm.h"
#include "sysinfo.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  kexit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return kfork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return kwait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int t;
  int n;

  argint(0, &n);
  argint(1, &t);
  addr = myproc()->sz;

  if(t == SBRK_EAGER || n < 0) {
    if(growproc(n) < 0) {
      return -1;
    }
  } else {
    // Lazily allocate memory for this process: increase its memory
    // size but don't allocate memory. If the processes uses the
    // memory, vmfault() will allocate it.
    if(addr + n < addr)
      return -1;
    if(addr + n > TRAPFRAME)
      return -1;
    myproc()->sz += n;
  }
  return addr;
}

uint64
sys_pause(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  if(n < 0)
    n = 0;
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
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

  argint(0, &pid);
  return kkill(pid);
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

// trace system call.
uint64
sys_trace(void)
{
  int mask;
  argint(0, &mask);
  if (mask < 0) return -1;
  myproc()->trace_mask = mask;
  return 0;
}

// sysinfo system call.
uint64
sys_sysinfo(void)
{
  struct sysinfo info;
  info.freemem = kfreemem();
  info.nproc = count_active_procs();
  info.nopenfiles = count_open_files();

  uint64 addr;
  argaddr(0, &addr);
  if (addr < 0) {
    return -1;
  }
  if (copyout(myproc()->pagetable, addr, (char *)&info, sizeof(info)) < 0) {
    return -1;
  }
  return 0;
}

// pgaccess
uint64
sys_pgaccess(void)
{
  uint64 base;
  int len;
  uint64 mask_addr;

  argaddr(0, &base); // base virtual address
  argint(1, &len); // number of pages
  argaddr(2, &mask_addr); // user address of the mask

  if (len < 0) return -1;
  if (len > 64) return -1;
  
  uint64 mask = 0;
  for (int i = 0; i < len; i++) {
    uint64 va = base + i * PGSIZE;
    pte_t *pte = walk(myproc()->pagetable, va, 0);
    if (pte && (*pte & PTE_V) && (*pte & PTE_U) && (*pte & PTE_A)) {
      mask |= (1ULL << i); // set bit i if page is accessed
      *pte &= ~PTE_A; // clear the accessed bit
    }
  }
  if (copyout(myproc()->pagetable, mask_addr, (char *)&mask, sizeof(mask)) < 0) {
    return -1;
  }
  return 0;
}

uint64
sys_vmprint(void)
{
  vmprint(myproc()->pagetable);
  return 0;
}