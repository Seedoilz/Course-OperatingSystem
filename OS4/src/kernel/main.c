
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                            main.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
                                                    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "proto.h"


int strategy;
int state[5] = {0,0,0,0,0};
PRIVATE void init_tasks()
{
	init_screen(tty_table);
	clean(console_table);

	// 表驱动，对应进程0, 1, 2, 3, 4, 5, 6
	int prior[7] = {1, 1, 1, 1, 1, 1, 1};
	for (int i = 0; i < 7; ++i) {
        proc_table[i].ticks    = prior[i];
        proc_table[i].priority = prior[i];
	}

	// initialization
	k_reenter = 0;
	ticks = 0;
	readers = 0;
	writers = 0;
	writing = 0;

	strategy = 2; // 切换策略

	p_proc_ready = proc_table;
}
/*======================================================================*
                            kernel_main
 *======================================================================*/
PUBLIC int kernel_main()
{
	disp_str("-----\"kernel_main\" begins-----\n");

	TASK*		p_task		= task_table;
	PROCESS*	p_proc		= proc_table;
	char*		p_task_stack	= task_stack + STACK_SIZE_TOTAL;
	u16		selector_ldt	= SELECTOR_LDT_FIRST;
	int i;
    u8              privilege;
    u8              rpl;
    int             eflags;
	for (i = 0; i < NR_TASKS + NR_PROCS; i++) {
        if (i < NR_TASKS) {     /* 任务 */
                        p_task    = task_table + i;
                        privilege = PRIVILEGE_TASK;
                        rpl       = RPL_TASK;
                        eflags    = 0x1202; /* IF=1, IOPL=1, bit 2 is always 1 */
                }
                else {                  /* 用户进程 */
                        p_task    = user_proc_table + (i - NR_TASKS);
                        privilege = PRIVILEGE_USER;
                        rpl       = RPL_USER;
                        eflags    = 0x202; /* IF=1, bit 2 is always 1 */
                }
                
		strcpy(p_proc->p_name, p_task->name);	// name of the process
		p_proc->pid = i;			// pid
		p_proc->sleeping = 0; // 初始化结构体新增成员
		p_proc->blocked = 0;
		
		p_proc->ldt_sel = selector_ldt;

		memcpy(&p_proc->ldts[0], &gdt[SELECTOR_KERNEL_CS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[0].attr1 = DA_C | privilege << 5;
		memcpy(&p_proc->ldts[1], &gdt[SELECTOR_KERNEL_DS >> 3],
		       sizeof(DESCRIPTOR));
		p_proc->ldts[1].attr1 = DA_DRW | privilege << 5;
		p_proc->regs.cs	= (0 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ds	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.es	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.fs	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.ss	= (8 & SA_RPL_MASK & SA_TI_MASK) | SA_TIL | rpl;
		p_proc->regs.gs	= (SELECTOR_KERNEL_GS & SA_RPL_MASK) | rpl;

		p_proc->regs.eip = (u32)p_task->initial_eip;
		p_proc->regs.esp = (u32)p_task_stack;
		p_proc->regs.eflags = eflags;

		p_proc->nr_tty = 0;

		p_task_stack -= p_task->stacksize;
		p_proc++;
		p_task++;
		selector_ldt += 1 << 3;
	}

	init_tasks();
	init_clock();
    init_keyboard();
	restart();

	while(1){}
}

PRIVATE readProc(char proc, int slices){
	if(proc=='B'){
		state[0] = 1;
	}
	else if(proc=='C'){
		state[1] = 1;
	}
	else if(proc=='D'){
		state[2] = 1;
	}
	sleep_ms(slices * TIME_SLICE); // 读耗时slices个时间片
	if(proc=='B'){
		state[0] = 0;
	}
	else if(proc=='C'){
		state[1] = 0;
	}
	else if(proc=='D'){
		state[2] = 0;
	}
}

PRIVATE	writeProc(char proc, int slices){
	if(proc=='E'){
		state[3] = 1;
	}
	else if(proc=='F'){
		state[4] = 1;
	}
	sleep_ms(slices * TIME_SLICE); // 写耗时slices个时间片
	if(proc=='E'){
		state[3] = 0;
	}
	else if(proc=='F'){
		state[4] = 0;
	}
}

//读写公平方案
void read_gp(char proc, int slices){
	if(proc=='B'){
		state[0] = 2;
	}
	else if(proc=='C'){
		state[1] = 2;
	}
	else if(proc=='D'){
		state[2] = 2;
	}
	P(&queue);
	P(&readerMutex);
	if (readers==0)
		P(&RWMutex); // 有读者，写者不能写
	readers++;
	V(&readerMutex);
	V(&queue);
	readProc(proc, slices);
	P(&readerMutex);
	readers--;
	if (readers==0)
		V(&RWMutex); // 没有读者，开始写
	V(&readerMutex);
}

void write_gp(char proc, int slices){
	if(proc=='E'){
		state[3] = 2;
	}
	else if(proc=='F'){
		state[4] = 2;
	}
	P(&queue);
	P(&RWMutex);
	writing = 1;
	V(&queue);
	// 写过程
	writeProc(proc, slices);
	writing = 0;
	V(&RWMutex);
}

// 读者优先
void read_rf(char proc, int slices){
	if(proc=='B'){
		state[0] = 2;
	}
	else if(proc=='C'){
		state[1] = 2;
	}
	else if(proc=='D'){
		state[2] = 2;
	}
	P(&NRMutex); //修改。
    P(&readerMutex);
    if (readers==0)
        P(&RWMutex);
    readers++;
    V(&readerMutex);

    readProc(proc, slices);

    V(&NRMutex);
    P(&readerMutex);
    readers--;
    if (readers==0)
        V(&RWMutex); // 没有读者，开始写
    V(&readerMutex);

}

void write_rf(char proc, int slices){
	if(proc=='E'){
		state[3] = 2;
	}
	else if(proc=='F'){
		state[4] = 2;
	}
    P(&RWMutex);
    writing = 1;
    // 写过程
    writeProc(proc, slices);
    writing = 0;
    V(&RWMutex);
}

// 写者优先
void read_wf(char proc, int slices){
	if(proc=='B'){
		state[0] = 2;
	}
	else if(proc=='C'){
		state[1] = 2;
	}
	else if(proc=='D'){
		state[2] = 2;
	}
    P(&NRMutex);
    P(&queue);
    P(&readerMutex);
    if (readers==0)
        P(&RWMutex);
    readers++;
    V(&readerMutex);
    V(&queue);
    //读过程开始
    readProc(proc, slices);
    P(&readerMutex);
    readers--;
    if (readers==0)
        V(&RWMutex); // 没有读者，开始写
    V(&readerMutex);
    V(&NRMutex);
}

void write_wf(char proc, int slices){
	if(proc=='E'){
		state[3] = 2;
	}
	else if(proc=='F'){
		state[4] = 2;
	}
    P(&writerMutex);
    // 写过程
    if (writers==0)
        P(&queue);
    writers++;
    V(&writerMutex);

    P(&RWMutex);
    writing = 1;
    writeProc(proc, slices);
    writing = 0;
    V(&RWMutex);

    P(&writerMutex);
    writers--;
    if (writers==0)
        V(&queue);
    V(&writerMutex);
}

read_f read_funcs[3] = {read_gp, read_rf, read_wf};
write_f write_funcs[3] = {write_gp, write_rf, write_wf};

/*======================================================================*
                               ReaderB
 *======================================================================*/
void ReaderB()
{	
	state[0] =0;
	sleep_ms(TIME_SLICE);
	while(1){
		read_funcs[strategy]('B', 2);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               ReaderC
 *======================================================================*/
void ReaderC()
{	
	state[1] =0;
	sleep_ms(TIME_SLICE);
	while(1){
		read_funcs[strategy]('C', 3);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               ReaderD
 *======================================================================*/
void ReaderD()
{
	state[2] =0;
	sleep_ms(TIME_SLICE);
	while(1){
		read_funcs[strategy]('D',3);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               WriterE
 *======================================================================*/
void WriterE()
{	
	state[3] =0;
	sleep_ms(TIME_SLICE);
	while(1){
		write_funcs[strategy]('E', 3);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               WriterF
 *======================================================================*/
void WriterF()
{	
	state[4] = 0;
	sleep_ms(TIME_SLICE);
	while(1){
		write_funcs[strategy]('F', 4);
		sleep_ms(TIME_SLICE);
	}
}

/*======================================================================*
                               Reporter
 *======================================================================*/
void ReporterA()
{	
	sleep_ms(TIME_SLICE);
	int time=1;
	while(1){
		if(time>=10){
			if(time==21){
				break;
			}
			if(time==20){
				printf("%c%d",'\06',2);
				printf("%c%d ",'\06',0);
			}
			else{
				printf("%c%d",'\06',1);
				printf("%c%d ",'\06',time-10);
			}
		}
		else{
			printf("%c%d ",'\06',time);
		}
		
		time++;

		for (int i = 0; i < 5; i++) {
        	if(i<4){
				if(state[i]==0){
					printf("%c%c ",'\03','Z');
				}
				else if(state[i]==1){
					printf("%c%c ",'\02','O');
				}
				else if(state[i]==2){
					printf("%c%c ",'\01','X');
				}
			}
			else{
				if(state[i]==0){
					printf("%c%c\n",'\03','Z');
				}
				else if(state[i]==1){
					printf("%c%c\n",'\02','O');
				}
				else if(state[i]==2){
					printf("%c%c\n",'\01','X');
				}
			}
		}
		sleep_ms(TIME_SLICE);
	}
	while(1){
		sleep_ms(TIME_SLICE);
	}
}
