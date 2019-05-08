#ifndef TASK_H
#define TASK_H

#include <inc/trap.h>
#include <kernel/mem.h>
#include <kernel/spinlock.h>
#define NR_TASKS	1024
#define TIME_QUANT	100

typedef enum
{
	TASK_FREE = 0,
	TASK_RUNNABLE,
	TASK_RUNNING,
	TASK_SLEEP,
	TASK_STOP,
} TaskState;

// Each task's user space
#define USR_STACK_SIZE	(40960)

typedef struct
{
	int task_id;
	int parent_id;
	struct Trapframe tf; //Saved registers
	int32_t remind_ticks;
	TaskState state;	//Task state
  pde_t *pgdir;  //Per process Page Directory
	
} Task;

typedef struct
{
	int index;
	int running[NR_TASKS];
	int len;
	struct spinlock lock;
} Runqueue;


void task_init();
void task_init_percpu();
void env_pop_tf(struct Trapframe *tf);

void sched_yield(void);
void sys_cls();
void sys_settextcolor(unsigned char forecolor, unsigned char backcolor);
void sys_kill(int pid);
int sys_fork();

#endif
