#include <kernel/task.h>
#include <kernel/cpu.h>
#include <inc/x86.h>

#define ctx_switch(ts) \
  do { env_pop_tf(&((ts)->tf)); } while(0)

void sched_yield(void)
{
	extern Task tasks[];
	int i;
	for(i = thiscpu->cpu_rq.index + 1; i != thiscpu->cpu_rq.index; i = (i+1) % thiscpu->cpu_rq.len) {
		if(tasks[thiscpu->cpu_rq.running[i]].state == TASK_RUNNABLE) {
			break;
		}
	}
	thiscpu->cpu_task = &tasks[thiscpu->cpu_rq.running[i]];
	thiscpu->cpu_task->state = TASK_RUNNING;
	thiscpu->cpu_task->remind_ticks = TIME_QUANT;
	//if(cpunum() == 0) printk("CPU %d running pid %d\n", cpunum(), thiscpu->cpu_task->task_id);
	lcr3(PADDR(thiscpu->cpu_task->pgdir));
	ctx_switch(thiscpu->cpu_task);
}
