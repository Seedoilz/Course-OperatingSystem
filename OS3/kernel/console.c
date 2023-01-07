
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/


#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE* p_con);
PUBLIC void exitSearch(CONSOLE* p_con);
PUBLIC void search(CONSOLE* p_con);

/*======================================================================*
			   init_screen
 *======================================================================*/
void push(CONSOLE *pConsole, unsigned int cursor);

unsigned int pop(CONSOLE *pConsole);

PUBLIC void init_screen(TTY* p_tty)
{
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	int v_mem_size = V_MEM_SIZE >> 1;	/* 显存总大小 (in WORD) */

	int con_v_mem_size                   = v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr      = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit        = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	if (nr_tty == 0) {
		/* 第一个控制台沿用原来的光标位置 */
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else {
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '#');
	}

	set_cursor(p_tty->p_console->cursor);
}


/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE* p_con)
{
	return (p_con == &console_table[nr_current_console]);
}


/*======================================================================*
			   exitSearch
*======================================================================*/

PUBLIC void exitSearch(CONSOLE* p_con) {

	//把关键字清除
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
    int len = p_con->cursor - p_con->start_cursor;
    clearKeyWords(p_vmem,len);
	//退出时，红色的都变回白色
	int len2 =  p_con->start_cursor * 2;
    changeKeyWordsColor(len2);
	//光标恢复到进入esc模式之前，ptr也复原
	restorePtr(p_con);
	flush(p_con);  //设置好要刷新才能显示
}

PUBLIC void clearKeyWords(u8* p_vmem, int len){
	for (int i = 0; i < len; i++) {
        *(p_vmem - 2 - 2 * i) = ' ';
        *(p_vmem - 1 - 2 * i) = DEFAULT_CHAR_COLOR;
    }
}

PUBLIC void changeKeyWordsColor(int len2){
	for (int i = 0; i < len2; i += 2) {
        *(u8*)(V_MEM_BASE + i + 1) = DEFAULT_CHAR_COLOR;                              
    }
}

PUBLIC void restorePtr(CONSOLE* p_con){
	p_con->cursor = p_con->start_cursor;
	p_con->pos_stk.ptr = p_con->pos_stk.start;
}

//#define	V_MEM_BASE	0xB8000	/* base of color video memory */

// typedef struct Sqstack{
// 	int ptr;
// 	int pos[10000];
// 	int start;   //esc模式开始时，ptr的位置
// }STK;

// typedef struct s_console
// {
// 	unsigned int	current_start_addr;	/* 当前显示到了什么位置	  */
// 	unsigned int	original_addr;		/* 当前控制台对应显存位置 */
// 	unsigned int	v_mem_limit;		/* 当前控制台占的显存大小 */
// 	unsigned int	cursor;			/* 当前光标位置 */
// 	unsigned int	start_cursor;			/* 原来光标位置 */
//     STK pos_stk;					/*新增*/
// }CONSOLE;

/*======================================================================*
			   search_key_word
*======================================================================*/
PUBLIC void search(CONSOLE* p_con) {
	int keyLen = (p_con->cursor - p_con->start_cursor) * 2;
	int i = 0;
	while(1){
		if(i>=p_con->start_cursor * 2){
			break;
		}
		int begin = i;
		int end = i;
		int found = 1;
		int wsOrChar = 0;
		int length = 0;
		while(1){
			if(length>=keyLen){
				break;
			}
		    // try to match
			//*((u8*)(V_MEM_BASE + end))当前搜索到的字符
			//*((u8*)(V_MEM_BASE + j+p_con->start_cursor*2))目标字符
			//int place = j+p_con->start_cursor*2;
			if (*((u8*)(V_MEM_BASE + end)) == *((u8*)(V_MEM_BASE + length + p_con->start_cursor*2))) {
				if(*((u8*)(V_MEM_BASE + end)) == ' '){
					if((*((u8*)(V_MEM_BASE + end + 1))+0x40) != *((u8*)(V_MEM_BASE + length + p_con->start_cursor*2 + 1))){
						found = 0;
						break;
					}
					wsOrChar = 1;
				}
                end += 2;
			}
			else {
				found = 0;
				break;
			}
			length = length + 2;
		}
		if(keyLen + begin > p_con->start_cursor * 2){
			break;
		}
		changeColorRed(p_con,found,begin,end,wsOrChar);
		i=i+2;
	}
}



PUBLIC void changeColorRed(CONSOLE* p_con,int found,int begin, int end,int wsOrChar){
	if (found==1) {
		if(wsOrChar){
			for (int m = begin; m < end; m += 2) {
				*(u8*)(V_MEM_BASE + m + 1) = RED_BACK_COLOR;//更改背景颜色
			}
		}
		else{
			//如果找到，就将颜色变为红
			for (int m = begin; m < end; m += 2) {
				*(u8*)(V_MEM_BASE + m + 1) = RED_CHAR_COLOR;//更改颜色，所以更改第二个字节
			}
		}
		
	}
}

/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE* p_con, char ch)
{
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);

	switch(ch) {
	case '\n':
		if (p_con->cursor < p_con->original_addr +
		    p_con->v_mem_limit - SCREEN_WIDTH) {
			push(p_con, p_con->cursor);
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * 
				((p_con->cursor - p_con->original_addr) /
				 SCREEN_WIDTH + 1);
		}
		break;
	case '\b':
		if (p_con->cursor > p_con->original_addr) {
			int tmp = p_con->cursor;
			p_con->cursor = pop(p_con);
			for(int i=0;i<tmp-p_con->cursor;i++){
				*(p_vmem - 2 - 2 * i) = ' ';
				*(p_vmem - 1 - 2 * i) = DEFAULT_CHAR_COLOR;
			}
		}
		break;
	case '\t':
		if (p_con->cursor < p_con->original_addr +
			p_con->v_mem_limit - TAB_WIDTH) {
			push(p_con, p_con->cursor);
			if(mode==0){
				for(int i=0;i<TAB_WIDTH;i++){
					*p_vmem++ = ' ';
					*p_vmem++ = TAB_COLOR;
				}
			}
			else{
				for(int i=0;i<TAB_WIDTH;i++){
					*p_vmem++ = ' ';
					*p_vmem++ = TAB_BACK_COLOR;
				}
			}
			p_con->cursor += TAB_WIDTH;
		}
		break;
	default:
		if (p_con->cursor <
		    p_con->original_addr + p_con->v_mem_limit - 1) {
			push(p_con, p_con->cursor);
			*p_vmem++ = ch;
			if(mode==0)
				if(ch == ' '){
					*p_vmem++ = SPACE_COLOR;
				}
				else{
					*p_vmem++ = DEFAULT_CHAR_COLOR;
				}
			else
				if(ch == ' '){
					*p_vmem++ = SPACE_BACK_COLOR;
				}
				else{
					*p_vmem++ = RED_CHAR_COLOR;
				}
			p_con->cursor++;
		}
		break;
	}

	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);
}


/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE* p_con)
{
	set_cursor(p_con->cursor);
	set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	//disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	//enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	//disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	//enable_int();
}



/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)	/* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
		return;
	}

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);
	set_video_start_addr(console_table[nr_console].current_start_addr);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE* p_con, int direction)
{
	if (direction == SCR_UP) {
		if (p_con->current_start_addr > p_con->original_addr) {
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN) {
		if (p_con->current_start_addr + SCREEN_SIZE <
		    p_con->original_addr + p_con->v_mem_limit) {
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else{
	}

	set_video_start_addr(p_con->current_start_addr);
	set_cursor(p_con->cursor);
}

/*======================================================================*
		push和pop
 *======================================================================*/
PUBLIC void push(CONSOLE* p_con, unsigned int pos) {
	p_con->pos_stk.pos[p_con->pos_stk.ptr++] = pos;
}


PUBLIC unsigned int pop(CONSOLE* p_con) {
	if (p_con->pos_stk.ptr == 0) return 0;
	else {
		(p_con->pos_stk.ptr)--;
		return p_con->pos_stk.pos[p_con->pos_stk.ptr];
	}
}