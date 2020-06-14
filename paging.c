#include "types.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "mmu.h"
#include "x86.h"
#include "proc.h"

static char buffer[PGSIZE];

    /*************** TASK - 1.1 ***************/
int find_free_or_occupied_page(struct proc *p, int condition, int swap_or_ram)
{
  if (condition == 1)
  { //in this case we want to find free page
    for (int i = 0; i < 16; i++)
    {
      if (swap_or_ram == 1)
      { //find in ram_arr
        if (p->ram_arr[i].occupied)
        {
          continue;
        }
        return i;
      }
      else
      { //find in swap_arr
        if (p->swap_arr[i].occupied)
        {
          continue;
        }
        return i;
      }
    }
  }
  else
  { //in this case we want to find_occupied page
    for (int i = 0; i < 16; i++)
    {
      if (swap_or_ram == 1)
      { //find in ram_arr
        return find_ram_by_policy();
      }
      else
      { //find in swap_arr
        if (!p->swap_arr[i].occupied)
        {
          continue;
        }
        return i;
      }
    }
  }
  return -1;
}
    /******************************************/

/********************************************* Flag Actions *********************************************/
    /*************** TASK - 1.2 ***************/
int get_pte_flags(int va, pde_t *pgdir)
{
  pde_t *pte = accessable_walkpgdir(pgdir, (int *)va, 0);
  if (!pte)
    return -1;
  return PTE_FLAGS(*pte);
}

int isValidPage(pde_t *pgdir)
{
  uint va = rcr2();
  pte_t *pte = accessable_walkpgdir(pgdir, (char *)va, 0);
  return (*pte & PTE_PG);
}
    /******************************************/

    /*************** TASK - 2 ***************/
int isReadOnlyPage(pde_t *pgdir)
{
  uint va = rcr2();
  pte_t *pte = accessable_walkpgdir(pgdir, (char *)va, 0);
  return !(*pte & PTE_W);
}
    /******************************************/
/********************************************************************************************************/

/******************************************* Read\Write Pages *******************************************/
    /*************** TASK - 1.2 ***************/
void disk_to_ram(uint start_pfault_va, char *ka)
{
  struct proc *p = myproc();
  if (p->pid > 2 && !is_none_paging_policy())
  {
    int perm = get_pte_flags(start_pfault_va, p->pgdir);

    //map all virtual addresses to the apropriate physical ones
    accessable_mappages(p->pgdir, (void *)start_pfault_va, PGSIZE, V2P(ka), perm);

    pde_t *pfault_pte = accessable_walkpgdir(p->pgdir, (void *)start_pfault_va, 0);
    *pfault_pte |= PTE_P | PTE_W | PTE_U; //declare that page is present, writable and owned by the user
    *pfault_pte &= ~PTE_PG;               //turn on paged_out flag
    lcr3(V2P(p->pgdir));                  //refresh TLB

    for (int i = 0; i < 16; i++)
    {
      if (p->swap_arr[i].virtual_adrr == start_pfault_va)
      {
        readFromSwapFile(p, buffer, i * PGSIZE, PGSIZE);
        int ram_index = find_free_or_occupied_page(p, FREE, 1);
        cprintf("disk_to_ram - Free ram index: %d\n", ram_index);
        p->ram_arr[ram_index].offset_in_swap_file = p->swap_arr[i].offset_in_swap_file;
        p->ram_arr[ram_index].virtual_adrr = p->swap_arr[i].virtual_adrr;
        p->ram_arr[ram_index].pagedir = p->swap_arr[i].pagedir;
        p->ram_arr[ram_index].occupied = 1;
        p->swap_arr[i].occupied = 0;
        break;
      }
    }
  }
}
    /******************************************/

    /*************** TASK - 1.1 ***************/
void swap(struct proc *p, pde_t *pagedir, uint mem)
{
  cprintf("\nProccess with pid: %d status - \n", p->pid);
  int k;
  cprintf("RAM Array before changing by policy:\n");
  for(k = 0; k< MAX_PYSC_PAGES; k++){
     cprintf("ram_arr[%d]: occupied = %d, virtual_adrr = %x, offset_in_swap_file = %d\t\n",k, p->ram_arr[k].occupied, p->ram_arr[k].virtual_adrr, p->ram_arr[k].offset_in_swap_file);
  }

  cprintf("not RAM Array:\n");
  for(k = 0; k< MAX_PYSC_PAGES; k++){
     cprintf("swap_arr[%d]: occupied = %d, virtual_adrr = %x, offset_in_swap_file = %d\t\n",k, p->swap_arr[k].occupied, p->swap_arr[k].virtual_adrr, p->swap_arr[k].offset_in_swap_file);
  }
  if (p->pid > 2 && !is_none_paging_policy())
  {
    /*find avaiable and occupied spaces in ram_arr and swap_arr */
    cprintf("swap - Start\n");
    int index_ram = find_ram_by_policy();
  cprintf("RAM Array after changing by policy:\n");
  for(k = 0; k< MAX_PYSC_PAGES; k++){
     cprintf("ram_arr[%d]: occupied = %d, virtual_adrr = %x, offset_in_swap_file = %d\t\n",k, p->ram_arr[k].occupied, p->ram_arr[k].virtual_adrr, p->ram_arr[k].offset_in_swap_file);
  }
    if (index_ram == -1)
      panic("no occupied cell in ram_arr\n");
    int index_swap = find_free_or_occupied_page(p, FREE, 0);
    if (index_swap == -1)
      panic("swap_arr is occupied\n");

    /*write to swapFile and update swap_arr*/
    cprintf("swap - Write to file in offset %d the ram of index %d (va = %x)\n", index_swap, index_ram, p->ram_arr[index_ram].virtual_adrr);
    if ((writeToSwapFile(p, (char *)p->ram_arr[index_ram].virtual_adrr, index_swap * PGSIZE, PGSIZE)) == -1)
    {
      panic("wrtie to swapFile failed..\n");
    }
    p->swap_arr[index_swap].virtual_adrr =(uint) p->ram_arr[index_ram].virtual_adrr;
    p->swap_arr[index_swap].offset_in_swap_file = index_swap * PGSIZE;
    p->swap_arr[index_swap].pagedir = pagedir;
    p->swap_arr[index_swap].occupied = 1;


    /*clear physical address of ram_arr[index] PTE*/
    cprintf("swap - Free PA of ram_arr[%d]\n", index_ram);
    pte_t *pte = accessable_walkpgdir(pagedir, (int *)p->ram_arr[index_ram].virtual_adrr, 0);
    uint pa = PTE_ADDR(*pte);
    kfree(P2V(pa));

    p->total_paged_out += 1;
    p->current_paged_out += 1;

    p->ram_arr[index_ram].occupied = 0;
    *pte |= PTE_PG;
    *pte &= ~PTE_P;
    *pte &= PTE_FLAGS(*pte); //clear flags for pte
    lcr3(V2P(p->pgdir));     //flush the TLB

    p->ram_arr[index_ram].virtual_adrr = mem;
    p->ram_arr[index_ram].pagedir = pagedir;
    p->ram_arr[index_ram].occupied = 1;
    p->ram_arr[index_ram].offset_in_swap_file = -1;
    cprintf("swap - Done\n");
  }
}
    /******************************************/

void removePages(struct proc *p)
{
  if (p->pid > 2 && !is_none_paging_policy())
  {
    for (int i = 0; i < 16; i++)
    {
      p->ram_arr[i].occupied = 0;
      p->ram_arr[i].virtual_adrr = 0;
      p->ram_arr[i].offset_in_swap_file = 0;
      p->ram_arr[i].pagedir = 0;

      p->swap_arr[i].occupied = 0;
      p->swap_arr[i].virtual_adrr = 0;
      p->swap_arr[i].offset_in_swap_file = 0;
      p->swap_arr[i].pagedir = 0;
    }
  }
}
/********************************************************************************************************/

/******************************************* Trap Functions *********************************************/
        /*************** TASK - 2 ***************/
void read_only_page_fault()
{
  uint va = rcr2();
  pte_t *pte = accessable_walkpgdir(myproc()->pgdir, (void *)va, 0);
  if ((va >= KERNBASE) || (pte == 0) || !(*pte & PTE_P) || !(*pte & PTE_U))
  {
    myproc()->killed = 1;
    return;
  }
  uint pa = PTE_ADDR(*pte);                  // Get physical address out from the pte
  uint refCount = get_reference_count(pa);   // Get reference count of current page
  char *mem;

  if (refCount > 1)                          // First try to write to this page
  {
    if ((mem = kalloc()) == 0)               // Allocate new memory page for the process
    {
      myproc()->killed = 1;
      return;
    }
    memmove(mem, (char *)P2V(pa), PGSIZE);   // Copy the content of the original memory page to mem
    *pte = V2P(mem) | PTE_P | PTE_U | PTE_W; // Make the pte to point to the new page

    decrement_reference_count(pa);           // The process does not point to the original page anymore
  }

  else if (refCount == 1)                    // Last try to write to this page - No need to allocate new page
  {
    *pte |= PTE_W;                           // Make the page writeable
  }
  else
  {
    panic("pagefault reference count wrong\n");
  }
  lcr3(V2P(myproc()->pgdir));                // Flush TLB

  //TODO: add the writable page to the ram_arr or swap_arr
}
    /******************************************/

    /*************** TASK - 1.2 ***************/
void handle_page_fault()
{
  struct proc *p = myproc();
  if (p->pid > 2 && !is_none_paging_policy())
  {
    uint start_pfault_va = PGROUNDDOWN(rcr2());
    p->total_paged_out += 1;
    p->current_paged_out += 1;
    /*allocate physical address and reset it*/
    char *new_physical_adrr = kalloc();
    if (new_physical_adrr == 0)
      panic("allocuvm out of memory\n");
    memset(new_physical_adrr, 0, PGSIZE);

    int ram_index = find_free_or_occupied_page(p, FREE, 1);
    if (ram_index >= 0)
    {
      cprintf("handle_pfualt - Found free index in ram - %d\n", ram_index);
      disk_to_ram(start_pfault_va, new_physical_adrr);
      memmove((void *)start_pfault_va, buffer, PGSIZE);
    }
    else
    {
      cprintf("handle_pfualt - No free index in ram\n");
      struct page exile_ram = p->ram_arr[find_ram_by_policy()];
      disk_to_ram(start_pfault_va, new_physical_adrr);
      memmove(new_physical_adrr, buffer, PGSIZE);
      int index = find_free_or_occupied_page(p, FREE, 0);
      if (index == -1)
        panic("swap_arr is occupied\n");
              cprintf("handle_pfualt -ram_arr[%d]: occupied=%d, va=%x\n",ram_index, exile_ram.occupied, exile_ram.virtual_adrr);

      cprintf("the index is:%d\n", exile_ram.virtual_adrr);
      if ((writeToSwapFile(p, (char *)exile_ram.virtual_adrr, index * PGSIZE, PGSIZE)) == -1)
        panic("cant write file\n");
      p->swap_arr[index].virtual_adrr = exile_ram.virtual_adrr;
      p->swap_arr[index].offset_in_swap_file = index * PGSIZE;
      p->swap_arr[index].pagedir = exile_ram.pagedir;
      p->swap_arr[index].occupied = 1;

      pde_t *helper = accessable_walkpgdir(exile_ram.pagedir, (int *)exile_ram.virtual_adrr, 0);
      uint ramPa = PTE_ADDR(*helper);

      *helper |= PTE_PG;
      *helper &= ~PTE_P;
      *helper &= PTE_FLAGS(*helper);
      lcr3(V2P(p->pgdir));
      kfree((char *)P2V(ramPa));
    }
  }
}
    /******************************************/
/********************************************************************************************************/

