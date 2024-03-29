#include <inc/mmu.h>

.set PROT_MODE_CSEG, 0x8         # kernel code segment selector
.set PROT_MODE_DSEG, 0x10        # kernel data segment selector
.set CR0_PE_ON,      0x1         # protected mode enable flag

.globl start
start:
	.code16                     # Assemble for 16-bit mode

	# Set up the important data segment registers (DS, ES, SS).
	xorw    %ax,%ax             # Segment number zero
	movw    %ax,%ds             # -> Data Segment
	movw    %ax,%es             # -> Extra Segment
	movw    %ax,%ss             # -> Stack Segment

	cli                         # Disable interrupts
	cld                         # String operations increment

	# Switch from real to protected mode, using a bootstrap GDT
	# and segment translation that makes virtual addresses 
	# identical to their physical addresses, so that the 
	# effective memory map does not change during the switch.
	lgdt    gdtdesc
	movl    %cr0, %eax
	orl     $CR0_PE_ON, %eax
	movl    %eax, %cr0

	# Jump to next instruction, but in 32-bit code segment.
	# Switches processor into 32-bit mode.
	ljmp    $PROT_MODE_CSEG, $protcseg

.code32                     # Assemble for 32-bit mode
protcseg:
	# Set up the protected-mode data segment registers
	movw    $PROT_MODE_DSEG, %ax    # Our data segment selector
	movw    %ax, %ds                # -> DS: Data Segment
	movw    %ax, %es                # -> ES: Extra Segment
	movw    %ax, %fs                # -> FS
	movw    %ax, %gs                # -> GS
	movw    %ax, %ss                # -> SS: Stack Segment

	# Set up the stack pointer and call into C.
	  movl    $start, %esp
	  call bootmain
loop:
	jmp loop

# Bootstrap GDT
	.p2align 2                                # force 4 byte alignment
gdt:
# Access type:
# Present: 1bit
# Privilage: 2bit, 0 for system space, 1 for user space
# Reserved: 1bit
# Executable: 1bit
# Direction Conforming: 1bit
# (code segment) allowing execution from lower privilege level
# (data segment) allowing to be read
# RW: 1bit
# (code segment) content of memory can be read
# (data segment) content of memory can be written to
# Accessed: 1bit, set to 0 initially, CPU will change to 1 when accessed
	SEG_NULL    # NULL segment
	SEG(0x9a, 0, 0xffffffff)     # Kernel code segment
	SEG(0x92, 0, 0xffffffff)     # Kernel data segment

gdtdesc: 
	.word	(gdtdesc-gdt)-1	# gdt limit
	.long	gdt	# gdt base
