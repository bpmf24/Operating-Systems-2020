#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "defs.h"
#include "x86.h"
#include "elf.h"

int exec(char *path, char **argv)
{
  char *s, *last;
  int i, off;
  uint argc, sz, sp, ustack[3 + MAXARG + 1];
  struct elfhdr elf;
  struct inode *ip;
  struct proghdr ph;
  pde_t *pgdir, *oldpgdir;
  struct proc *curproc = myproc();

  struct page swap_backup[MAX_PYSC_PAGES];
  struct page ram_backup[MAX_PYSC_PAGES];
  if (curproc->pid > 2)
  {
    for (int i = 0; i < MAX_PYSC_PAGES; i++)
    {
      swap_backup[i] = curproc->swap_arr[i];
      ram_backup[i] = curproc->ram_arr[i];

      curproc->swap_arr[i].occupied = 0;
      curproc->swap_arr[i].offset_in_swap_file = -1;
      curproc->swap_arr[i].pagedir = 0;
      curproc->swap_arr[i].virtual_adrr = 0;

      curproc->ram_arr[i].occupied = 0;
      curproc->ram_arr[i].offset_in_swap_file = -1;
      curproc->ram_arr[i].pagedir = 0;
      curproc->ram_arr[i].virtual_adrr = 0;
    }
  }

  begin_op();

  if ((ip = namei(path)) == 0)
  {
    end_op();
    cprintf("exec: fail\n");
    return -1;
  }
  ilock(ip);
  pgdir = 0;

  // Check ELF header
  if (readi(ip, (char *)&elf, 0, sizeof(elf)) != sizeof(elf))
    goto bad;
  if (elf.magic != ELF_MAGIC)
    goto bad;

  if ((pgdir = setupkvm()) == 0)
    goto bad;

  // Load program into memory.
  sz = 0;
  for (i = 0, off = elf.phoff; i < elf.phnum; i++, off += sizeof(ph))
  {
    if (readi(ip, (char *)&ph, off, sizeof(ph)) != sizeof(ph))
      goto bad;
    if (ph.type != ELF_PROG_LOAD)
      continue;
    if (ph.memsz < ph.filesz)
      goto bad;
    if (ph.vaddr + ph.memsz < ph.vaddr)
      goto bad;
    if ((sz = allocuvm(pgdir, sz, ph.vaddr + ph.memsz)) == 0)
      goto bad;
    if (ph.vaddr % PGSIZE != 0)
      goto bad;
    if (loaduvm(pgdir, (char *)ph.vaddr, ip, ph.off, ph.filesz) < 0)
      goto bad;
  }
  iunlockput(ip);
  end_op();
  ip = 0;

  // Allocate two pages at the next page boundary.
  // Make the first inaccessible.  Use the second as the user stack.
  sz = PGROUNDUP(sz);
  if ((sz = allocuvm(pgdir, sz, sz + 2 * PGSIZE)) == 0)
    goto bad;
  clearpteu(pgdir, (char *)(sz - 2 * PGSIZE));
  sp = sz;

  // Push argument strings, prepare rest of stack in ustack.
  for (argc = 0; argv[argc]; argc++)
  {
    if (argc >= MAXARG)
      goto bad;
    sp = (sp - (strlen(argv[argc]) + 1)) & ~3;
    if (copyout(pgdir, sp, argv[argc], strlen(argv[argc]) + 1) < 0)
      goto bad;
    ustack[3 + argc] = sp;
  }
  ustack[3 + argc] = 0;

  ustack[0] = 0xffffffff; // fake return PC
  ustack[1] = argc;
  ustack[2] = sp - (argc + 1) * 4; // argv pointer

  sp -= (3 + argc + 1) * 4;
  if (copyout(pgdir, sp, ustack, (3 + argc + 1) * 4) < 0)
    goto bad;

  // Save program name for debugging.
  for (last = s = path; *s; s++)
    if (*s == '/')
      last = s + 1;
  safestrcpy(curproc->name, last, sizeof(curproc->name));

  // if(curproc->pid > 2){ // }
  //     if(curproc->ram_arr[i].occupied){
  //     curproc->ram_arr[i].pagedir = pgdir;
  //     }
  //     if(curproc->swap_arr[i].occupied){
  //     curproc->swap_arr[i].pagedir = pgdir;
  //     }
  //   }
  //   removeSwapFile(curproc);
  //   createSwapFile(curproc);
  // }

  // if (curproc->pid > 2)
  // {
  //   removeSwapFile(curproc);
  //   createSwapFile(curproc);
  // }

  // Commit to the user image.
  oldpgdir = curproc->pgdir;
  curproc->pgdir = pgdir;
  curproc->sz = sz;
  curproc->tf->eip = elf.entry; // main
  curproc->tf->esp = sp;
  switchuvm(curproc);
  freevm(oldpgdir);
  return 0;

bad:
  if (pgdir)
  {
    freevm(pgdir);
    if (curproc->pid > 2)
    {
      for (int i = 0; i < MAX_PYSC_PAGES; i++)
      {
        curproc->swap_arr[i] = swap_backup[i];
        curproc->ram_arr[i] = ram_backup[i];
      }
    }
  }
  if (ip)
  {
    iunlockput(ip);
    end_op();
  }
  return -1;
}
