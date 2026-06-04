#include<stdio.h>
#include"./console/console.h"
#include"start_mplayer.h"
#include<stdlib.h>
#include"lrc.h"
#include<time.h>
#include<unistd.h>

TIME m_time={0,0};
char* song_msg[4];
int main(){
    char *lrc_text[200]={NULL};
    char* lrc_str=read_src_file("./lrc/鲜花.lrc");
    if  (lrc_str ==NULL){
        printf("错误：无法读取歌词文件！\n");
        return -1;
    }
    int line_num=strtok_lrc_buf(lrc_text,lrc_str);
    //处理lrc前四行
    lrc_head_resolve(song_msg,lrc_text);
    LRC *head =lrc_resolve(line_num,lrc_text);
    free(lrc_str);

    print_song_message(song_msg);//打印歌曲信息
    time_printf(m_time);
    lrc_printf(head,NULL);


    printf("正在启动音乐播放器...\n"); 
    mplayer_play("./song/鲜花.mp3");
  

    usleep(500000);
    LRC *start_point;
    LRC *pb=NULL;
    int s_time = 0;
    while(1){
        time_printf(m_time);
        pb=link_search(head,s_time);
        if(pb!=NULL){
            start_point=find_start_point(head,pb);
            print_song_message(song_msg);
            time_printf(m_time);

            lrc_printf(start_point,pb);
            if(pb->next==NULL)
            break;
        }
        usleep(1000*1000);
        s_time=s_time+1;
        m_clock(&m_time);
    }
    link_free(head);
    song_msg_free(song_msg);
    cusor_show();

    printf("歌曲播放完成！按任意键退出...\n");
    getchar();
    return 0;
    }



