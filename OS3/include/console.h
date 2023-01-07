
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.h
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

#ifndef _ORANGES_CONSOLE_H_
#define _ORANGES_CONSOLE_H_

//新增
typedef struct Sqstack{
	int ptr;
	int pos[10000];
	int start;   //esc模式开始时，ptr的位置
}STK;

/* CONSOLE */
typedef struct s_console
{
	unsigned int	current_start_addr;	/* 当前显示到了什么位置	  */
	unsigned int	original_addr;		/* 当前控制台对应显存位置 */
	unsigned int	v_mem_limit;		/* 当前控制台占的显存大小 */
	unsigned int	cursor;			/* 当前光标位置 */
	unsigned int	start_cursor;			/* 原来光标位置 */
    STK pos_stk;					/*新增*/
}CONSOLE;

#define SCR_UP	1	/* scroll forward */
#define SCR_DN	-1	/* scroll backward */

#define SCREEN_SIZE		(80 * 25)
#define SCREEN_WIDTH		80

#define DEFAULT_CHAR_COLOR	0x07	/*  黑底白字 */
//新增
#define RED_CHAR_COLOR	0x04	/*  黑底红字 */
#define SPACE_COLOR 0x03 /* 随便取了一个内容标识空格的颜色 */
#define TAB_COLOR 0x02 /* 随便取了一个内容标识TAB的颜色 */
#define RED_BACK_COLOR	0x40	/*  红底 */
#define SPACE_BACK_COLOR 0x43
#define TAB_BACK_COLOR 0x42
#define TAB_WIDTH  4        /*TAB的长度是4*/

#endif /* _ORANGES_CONSOLE_H_ */
