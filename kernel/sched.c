#include <kernel/task.h>
#include <inc/x86.h>

#define ctx_switch(ts) \
  do { env_pop_tf(&((ts)->tf)); } while(0)

void sched_yield(void)
{
	extern Task tasks[];
	extern Task *cur_task;
	int task_id = cur_task->task_id;
	int new_id;
	for(new_id = (task_id+1) % NR_TASKS; new_id != task_id; new_id = (new_id+1) % NR_TASKS) {
		if(tasks[new_id].state == TASK_RUNNABLE) {
			break;
		}
	}
	cur_task = &tasks[new_id];
	cur_task->state = TASK_RUNNING;
	cur_task->remind_ticks = TIME_QUANT;
	lcr3(PADDR(cur_task->pgdir));
	ctx_switch(cur_task);
}
