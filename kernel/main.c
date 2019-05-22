#include <inc/stdio.h>
#include <inc/kbd.h>
#include <inc/shell.h>
#include <inc/x86.h>
#include <kernel/mem.h>
#include <kernel/trap.h>
#include <kernel/picirq.h>
#include <kernel/task.h>
#include <kernel/syscall.h>
#include <kernel/timer.h>
#include <kernel/cpu.h>

#include <fs.h>

extern void fs_test();
extern void init_video(void);
static void boot_aps(void);
extern int disk_init();
extern void disk_test();

void kernel_main(void)
{
	extern char stext[];
	extern char etext[], end[], data_start[],rdata_end[];
	extern void task_job();

	init_video();
  	mem_init();
	mp_init();
	lapic_init();
  	task_init();
	trap_init();
	pic_init();
	kbd_init();

  	timer_init();
  	syscall_init();
	disk_init();
	disk_test();
	fs_test();
	fs_init();
	boot_aps();

  printk("Kernel code base start=0x%08x to = 0x%08x\n", stext, etext);
  printk("Readonly data start=0x%08x to = 0x%08x\n", etext, rdata_end);
  printk("Kernel data base start=0x%08x to = 0x%08x\n", data_start, end);

  /* Enable interrupt */
  __asm __volatile("sti");

  lcr3(PADDR(thiscpu->cpu_task->pgdir));

  /* Move to user mode */
  asm volatile("movl %0,%%eax\n\t" \
  "pushl %1\n\t" \
  "pushl %%eax\n\t" \
  "pushfl\n\t" \
  "pushl %2\n\t" \
  "pushl %3\n\t" \
  "iret\n" \
  :: "m" (thiscpu->cpu_task->tf.tf_esp), "i" (GD_UD | 0x03), "i" (GD_UT | 0x03), "m" (thiscpu->cpu_task->tf.tf_eip)
  :"ax");
}

// While boot_aps is booting a given CPU, it communicates the per-core
// stack pointer that should be loaded by mpentry.S to that CPU in
// this variable.
void *mpentry_kstack;

// Start the non-boot (AP) processors.
static void
boot_aps(void)
{
	extern char mpentry_start[], mpentry_end[];
	memmove(KADDR(MPENTRY_PADDR), mpentry_start, (mpentry_end - mpentry_start));
	for(int i = 1; i < ncpu; i++) {
		mpentry_kstack = percpu_kstacks[i] + KSTKSIZE;
		lapic_startap(i, MPENTRY_PADDR);
		while(cpus[i].cpu_status != CPU_STARTED);
	}
}

// Setup code for APs
void
mp_main(void)
{
	/* 
	 * Here is the per-CPU state you should be aware of
	 *
	 * 1. Per-CPU kernel stack 
	 *    
	 *    Because multiple CPUs can trap into the kernel simultaneously,
	 *    we need a separate kernel stack for each processor to prevent
	 *    them from interfering with each other's execution. The array
	 *    percpu_kstacks[NCPU][KSTKSIZE] reserves space for NCPU's worth
	 *    of kernel stacks.
	 *    
	 *    In previous Lab, you mapped the physical memory that bootstack
	 *    refers to as the BSP's kernel stack just below KSTACKTOP.
	 *    Similarly, in this lab, you will map each CPU's kernel stack 
	 *    into this region with guard pages acting as a buffer between them.
	 *    CPU 0's stack will still grow down from KSTACKTOP; CPU 1's stack
	 *    will start KSTKGAP bytes below the bottom of CPU 0's stack, and
	 *    so on. inc/memlayout.h shows the mapping layout.
	 *
	 * 2. Per-CPU TSS and TSS descriptor
	 *
	 *    A per-CPU task state segment (TSS) is also needed in order to
	 *    specify where each CPU's kernel stack lives. The TSS for CPU i
	 *    is stored in cpus[i].cpu_ts, and the corresponding TSS descriptor
	 *    is defined in the GDT entry gdt[(GD_TSS0 >> 3) + i]. The global 
	 *    tss variable defined in kern/task.c will no longer be useful.
	 *
	 * 3. Per-CPU current task pointer
	 *   
	 *    Since each CPU can run different user process simultaneously,
	 *    we redefined the symbol cur_task to refer to cpus[cpunum()].
	 *    cpu_task (or thiscpu->cpu_task), which points to the task
	 *    currently executing on the current CPU (the CPU on which the 
	 *    code is running) 
	 *
	 * 4. Per-CPU system registers
	 *    
	 *    All registers, including system registers, are private to a CPU.
	 *    Therefore, instructions that initialize these registers, such as
	 *    lcr3(), ltr(), lgdt(), lidt(), etc., must be executed once on each
	 *    CPU.
	 *
	 * 5. Per-CPU Runqueue       
	 */
	
	// We are in high EIP now, safe to switch to kern_pgdir 
	lcr3(PADDR(kern_pgdir));
	printk("SMP: CPU %d starting\n", cpunum());
	
	// Your code here:
	lapic_init();
	task_init_percpu();
	lidt(&idt_pd);

	xchg(&thiscpu->cpu_status, CPU_STARTED);


	/* Enable interrupt */
	__asm __volatile("sti");

	lcr3(PADDR(thiscpu->cpu_task->pgdir));

	/* Move to user mode */
	asm volatile("movl %0,%%eax\n\t" \
			"pushl %1\n\t" \
			"pushl %%eax\n\t" \
			"pushfl\n\t" \
			"pushl %2\n\t" \
			"pushl %3\n\t" \
			"iret\n" \
			:: "m" (thiscpu->cpu_task->tf.tf_esp), "i" (GD_UD | 0x03), "i" (GD_UT | 0x03), "m" (thiscpu->cpu_task->tf.tf_eip)
			:"ax");

}
