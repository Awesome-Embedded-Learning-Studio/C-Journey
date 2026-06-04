#ifndef __LRC_H__
#define __LRC_H__
typedef struct lrc{
	int  time;
	char lrc_buf[200];
	struct lrc *front;
	struct lrc *next;
}LRC;
typedef struct{     												 		//表示日期时间的数据结构
	char minute;
	char second;
}TIME;
extern char * read_src_file(char *src_file_name);
extern int strtok_lrc_buf(char *lrc_text[],char *file_buf);
extern void lrc_head_resolve(char *song_msg[],char * lrc_text[]);
extern void print_song_message(char * song_msg[]);
extern LRC* link_search(LRC *head,int m_time);
extern LRC* lrc_resolve(int lrc_line,char * lrc_text[]);
extern void double_link_insert(LRC **p_head,LRC *p_new);
extern void link_free(LRC *head);
extern void time_delay(int sec);
extern void  time_printf(TIME m_time);
extern void color_pos_printf(char *str,int x,int y,int color);
extern LRC * find_start_point(LRC *head,LRC*pb);
extern void lrc_printf(LRC *pb , LRC *expect_play_pointer);
extern void m_clock(TIME *p_m_time);
extern void song_msg_free(char *song_msg[]);

#endif