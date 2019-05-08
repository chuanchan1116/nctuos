#include <inc/mmu.h>
#include <inc/types.h>
#include <inc/string.h>
#include <inc/x86.h>
#include <inc/memlayout.h>
#include <kernel/task.h>
#include <kernel/mem.h>
#include <kernel/cpu.h>
#include <kernel/spinlock.h>

// Global descriptor table.
//
// Set up global descriptor table (GDT) with separate segments for
// kernel mode and user mode.  Segments serve many purposes on the x86.
// We don't use any of their memory-mapping capabilities, but we need
// them to switch privilege levels. 
//
// The kernel and user segments are identical except for the DPL.
// To load the SS register, the CPL must equal the DPL.  Thus,
// we must duplicate the segments for the user and the kernel.
//
// In particular, the last argument to the SEG macro used in the
// definition of gdt specifies the Descriptor Privilege Level (DPL)
// of that descriptor: 0 for kernel and 3 for user.
//
struct Segdesc gdt[NCPU + 5] =
{
	// 0x0 - unused (always faults -- for trapping NULL far pointers)
	SEG_NULL,

	// 0x8 - kernel code segment
	[GD_KT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 0),

	// 0x10 - kernel data segment
	[GD_KD >> 3] = SEG(STA_W, 0x0, 0xffffffff, 0),

	// 0x18 - user code segment
	[GD_UT >> 3] = SEG(STA_X | STA_R, 0x0, 0xffffffff, 3),

	// 0x20 - user data segment
	[GD_UD >> 3] = SEG(STA_W , 0x0, 0xffffffff, 3),

	// First TSS descriptors (starting from GD_TSS0) are initialized
	// in task_init()
	[GD_TSS0 >> 3] = SEG_NULL
	
};

struct Pseudodesc gdt_pd = {
	sizeof(gdt) - 1, (unsigned long) gdt
};



static struct tss_struct tss;
Task tasks[NR_TASKS];

extern char bootstack[];

extern char UTEXT_start[], UTEXT_end[];
extern char UDATA_start[], UDATA_end[];
extern char UBSS_start[], UBSS_end[];
extern char URODATA_start[], URODATA_end[];
/* Initialized by task_init */
uint32_t UTEXT_SZ;
uint32_t UDATA_SZ;
uint32_t UBSS_SZ;
uint32_t URODATA_SZ;


extern void sched_yield(void);


int task_create()
{
	static struct spinlock task_lock;
	Task *ts = NULL;
	int task_id = thiscpu->cpu_task ? thiscpu->cpu_task->task_id : NR_TASKS-1;
	int new_id;
	/* Find a free task structure */
	spin_lock(&task_lock);
	for(new_id = (task_id+1) % NR_TASKS; new_id != task_id; new_id = (new_id+1) % NR_TASKS) {
		if(tasks[new_id].state == TASK_FREE) {
			ts = &tasks[new_id];
			break;
		}
	}
	if(!ts) {
		spin_unlock(&task_lock);
		return -1;
	}
	ts->state = TASK_RUNNABLE;
	spin_unlock(&task_lock);
  /* Setup Page Directory and pages for kernel*/
  if (!(ts->pgdir = setupkvm()))
    panic("Not enough memory for per process page directory!\n");

  /* Setup User Stack */
	for(int i = USTACKTOP - PGSIZE; i >= USTACKTOP - USR_STACK_SIZE; i -= PGSIZE) {
		struct PageInfo *pp = page_alloc(ALLOC_ZERO);
		if(!pp) {
			ts->state = TASK_FREE;
			return -1;
		}
		if(page_insert(ts->pgdir, pp, i, PTE_U | PTE_W)) {
			ts->state = TASK_RUNNABLE;
			return -1;
		}
	}

	/* Setup Trapframe */
	memset( &(ts->tf), 0, sizeof(ts->tf));

	ts->tf.tf_cs = GD_UT | 0x03;
	ts->tf.tf_ds = GD_UD | 0x03;
	ts->tf.tf_es = GD_UD | 0x03;
	ts->tf.tf_ss = GD_UD | 0x03;
	ts->tf.tf_esp = USTACKTOP-PGSIZE;

	/* Setup task structure (task_id and parent_id) */
	ts->task_id = new_id;
	ts->parent_id = thiscpu->cpu_task ? thiscpu->cpu_task->task_id : 0;
	ts->remind_ticks = TIME_QUANT;
	return new_id;
}



static void task_free(int pid)
{
	lcr3(PADDR(kern_pgdir));
	for(int i = USTACKTOP - PGSIZE; i >= USTACKTOP - USR_STACK_SIZE; i -= PGSIZE) {
		page_remove(tasks[pid].pgdir, i);
	}
	ptable_remove(tasks[pid].pgdir);
	pgdir_remove(tasks[pid].pgdir);
	tasks[pid].state = TASK_FREE;
}

void sys_kill(int pid)
{

	if (pid > 0 && pid < NR_TASKS)
	{
		// prevent CPU from shutting down
		if(pid == thiscpu->cpu_rq.running[0]) return;
		spin_lock(&thiscpu->cpu_rq.lock);
		for(int i = 1; i < thiscpu->cpu_rq.len; i++) {
			if(thiscpu->cpu_rq.running[i] == pid) {
				for(int j = i+1; j < thiscpu->cpu_rq.len; j++) {
					thiscpu->cpu_rq.running[i] = thiscpu->cpu_rq.running[j];
				}
				thiscpu->cpu_rq.len--;
				break;
			}
		}
		spin_unlock(&thiscpu->cpu_rq.lock);
		task_free(pid);
		if(thiscpu->cpu_task->task_id == pid) sched_yield();
	}
}

int sys_fork()
{
  /* pid for newly created process */
  int pid = task_create();
	if(pid < 0) return -1;
	if ((uint32_t)thiscpu->cpu_task)
	{
		tasks[pid].tf = thiscpu->cpu_task->tf;
		for(int i = USTACKTOP - PGSIZE; i >= USTACKTOP - USR_STACK_SIZE; i -= PGSIZE) {
			pte_t *src = pgdir_walk(thiscpu->cpu_task->pgdir, i, 0);
			if(src) {
				pte_t *dst = pgdir_walk(tasks[pid].pgdir, i, 0);
				memcpy(KADDR(PTE_ADDR(*dst)), KADDR(PTE_ADDR(*src)), PGSIZE);
			}
		}
    /* Step 4: All user program use the same code for now */
    setupvm(tasks[pid].pgdir, (uint32_t)UTEXT_start, UTEXT_SZ);
    setupvm(tasks[pid].pgdir, (uint32_t)UDATA_start, UDATA_SZ);
    setupvm(tasks[pid].pgdir, (uint32_t)UBSS_start, UBSS_SZ);
    setupvm(tasks[pid].pgdir, (uint32_t)URODATA_start, URODATA_SZ);
	thiscpu->cpu_task->tf.tf_regs.reg_eax = pid;
	tasks[pid].tf.tf_regs.reg_eax = 0;
	}
	int selected = 0;
	for(int i = 0, min = NR_TASKS + 1; i < ncpu; i++) {
		spin_lock(&cpus[i].cpu_rq.lock);
		if(cpus[i].cpu_rq.len < min){
			min = cpus[i].cpu_rq.len;
			selected = i;
		}
		spin_unlock(&cpus[i].cpu_rq.lock);
	}
	spin_lock(&cpus[selected].cpu_rq.lock);
	cpus[selected].cpu_rq.running[cpus[selected].cpu_rq.len++] = pid;
	spin_unlock(&cpus[selected].cpu_rq.lock);
	return pid;
}


void task_init()
{
  extern int user_entry();
	int i;
  UTEXT_SZ = (uint32_t)(UTEXT_end - UTEXT_start);
  UDATA_SZ = (uint32_t)(UDATA_end - UDATA_start);
  UBSS_SZ = (uint32_t)(UBSS_end - UBSS_start);
  URODATA_SZ = (uint32_t)(URODATA_end - URODATA_start);

	/* Initial task sturcture */
	for (i = 0; i < NR_TASKS; i++)
	{
		memset(&(tasks[i]), 0, sizeof(Task));
		tasks[i].state = TASK_FREE;

	}
	task_init_percpu();
}

void task_init_percpu()
{
	

	int i;
	extern int user_entry();
	extern int idle_entry();
	
	// Setup a TSS so that we get the right stack
	// when we trap to the kernel.
	memset(&thiscpu->cpu_tss, 0, sizeof(thiscpu->cpu_tss));
	thiscpu->cpu_tss.ts_esp0 = (uint32_t)percpu_kstacks[cpunum()] + KSTKSIZE;
	thiscpu->cpu_tss.ts_ss0 = GD_KD;

	// fs and gs stay in user data segment
	thiscpu->cpu_tss.ts_fs = GD_UD | 0x03;
	thiscpu->cpu_tss.ts_gs = GD_UD | 0x03;

	/* Setup TSS in GDT */
	gdt[(GD_TSS0 >> 3) + cpunum()] = SEG16(STS_T32A, (uint32_t)(&thiscpu->cpu_tss), sizeof(struct tss_struct), 0);
	gdt[(GD_TSS0 >> 3) + cpunum()].sd_s = 0;

	/* Setup first task */
	i = task_create();
	thiscpu->cpu_task = &(tasks[i]);
	thiscpu->cpu_rq.index = 0;
	thiscpu->cpu_rq.running[0] = i;
	thiscpu->cpu_rq.len = 1;

	/* For user program */
	setupvm(thiscpu->cpu_task->pgdir, (uint32_t)UTEXT_start, UTEXT_SZ);
	setupvm(thiscpu->cpu_task->pgdir, (uint32_t)UDATA_start, UDATA_SZ);
	setupvm(thiscpu->cpu_task->pgdir, (uint32_t)UBSS_start, UBSS_SZ);
	setupvm(thiscpu->cpu_task->pgdir, (uint32_t)URODATA_start, URODATA_SZ);
	if(bootcpu->cpu_id == cpunum()) thiscpu->cpu_task->tf.tf_eip = (uint32_t)user_entry;
	else thiscpu->cpu_task->tf.tf_eip = (uint32_t)idle_entry;
	

	/* Load GDT&LDT */
	lgdt(&gdt_pd);


	lldt(0);

	// Load the TSS selector 
	ltr(GD_TSS0 + (cpunum() << 3));

	thiscpu->cpu_task->state = TASK_RUNNING;
}
