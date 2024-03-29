#include <inc/mmu.h>
#include <inc/trap.h>

/* Use TRAPHANDLER_NOEC for traps where the CPU doesn't push an error code.
 * It pushes a 0 in place of the error code, so the trap frame has the same
 * format in either case.
 */
#define TRAPHANDLER_NOEC(name, num)					\
	.globl name;							\
	.type name, @function;						\
	.align 2;							\
	name:								\
	pushl $0;							\
	pushl $(num);							\
	jmp _alltraps


.text

 TRAPHANDLER_NOEC(isr_kbd, IRQ_OFFSET + IRQ_KBD);
 TRAPHANDLER_NOEC(isr_timer, IRQ_OFFSET + IRQ_TIMER);

.globl default_trap_handler;
_alltraps:
	pushl %ds
	pushl %es
	pusha
	pushl %esp # Pass a pointer which points to the Trapframe as an argument to default_trap_handler()
	call default_trap_handler
	popl %esp
	popa
	popl %es
	popl %ds
	add $8, %esp # Cleans up the pushed error code and pushed ISR number
	iret # pops 5 things at once: CS, EIP, EFLAGS, SS, and ESP!

