#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include "./console/console.h"
#include"lrc.h"

//读出文件内容
char* read_src_file(char *src_file_name){
    unsigned long int FILE_SIZE=0;
    char *src_file_text=NULL;
    FILE  *fp= NULL;
    fp=fopen(src_file_name,"rb");
    if(fp==NULL){
        printf("open %s failed!\n",src_file_name);
        return 0;           
    }
    fseek(fp,0,SEEK_END);
    FILE_SIZE=ftell(fp);
    rewind(fp);//将文件指针重新定位到文件开头
   src_file_text=(char *)malloc(FILE_SIZE+1);
    memset(src_file_text,'\0',FILE_SIZE+1);
    if(fread(src_file_text,FILE_SIZE,1,fp)==0){
        printf("read from src_file faild\n");
        return NULL;
        }
    fclose(fp);//关闭文件
    return src_file_text;//读出字符串首地址
    }

//切割歌词保存到数组
int strtok_lrc_buf(char *lrc_text[],char *file_buf)
{
    int i=0;
    lrc_text[i]=strtok(file_buf,"\r\n");
    while(lrc_text[i]!=NULL){
        lrc_text[++i]=strtok(NULL,"\r\n");
    }
    return i;//切割出来的字符串个数
}
//函数功能
void lrc_head_resolve(char *song_msg[],char *lrc_text[]){
    int i=0;
    char* message[]={"[ti","[ar","[al","[by"};
    for(i=0;i<4;i++){
        if(strncmp(lrc_text[i],message[i],2)==0){
        song_msg[i]=(char *)malloc(strlen(lrc_text[i])-4);
        //
        sscanf(lrc_text[i],"%*4s%[^]]",song_msg[i]); 
        }else{
            song_msg[i]=NULL;
        }
    }
}
//解析歌曲信息
LRC* lrc_resolve(int lrc_line,char * lrc_text[]){
    //头节点 插入的新节点
    LRC *head=NULL,*p_new=NULL;
    int i=0,j=0;
    char *lrc_str;
    int time_buf[10];
    int time_num=0;
    int min,sec;


for(i=4;i<lrc_line;i++){
     lrc_str=lrc_text[i];
    time_num=0;//解析每句歌词时，要把本句歌词对应的时间初始化为0
    while(*lrc_str=='['){
        sscanf(lrc_str,"[%2d:%2d",&min,&sec);
        time_buf[time_num++]=min*60+sec;
        lrc_str=lrc_str+10;
    }
for(j=0;j<time_num;j++){
    p_new=(LRC *)malloc(sizeof(LRC));
    p_new->time=time_buf[j];
    strcpy(p_new->lrc_buf,lrc_str);
    double_link_insert(&head,p_new);
}
}
return head;
}

//插入链表排序
void double_link_insert(LRC **p_head,LRC *p_new){
    LRC *pb,*pf;
    pb=*p_head;
    if(*p_head==NULL){
    *p_head=p_new;
    p_new->front=NULL;
    p_new->next=NULL;
    return ;
}
    

while((p_new->time>=pb->time)&&(pb->next!=NULL))
{
    pb=pb->next; 
}


if(p_new->time<pb->time){
    if(pb==*p_head){
        p_new->next=*p_head;
        (*p_head)->front=p_new;
        p_new->front=NULL;
        *p_head=p_new;
    }else{
          pf=pb->front;
          p_new->next=pb;
          p_new->front=pf;
          pf->next=p_new;
          pb->front=p_new;
    }
}else{
    pb->next=p_new;
    p_new->front=pb;
    p_new->next=NULL;
}
}
//回收链表
void link_free(LRC *head){
    LRC *pb;
    while (head!=NULL)
    {
        pb=head;
        head=head->next;
        free(pb);
    }
    
}

//查询节点
LRC * link_search(LRC *head,int m_time){
    LRC *pb=head;
    while(pb!=NULL){
        if(pb->time==m_time){
            return pb;
        }else{
            pb=pb->next;
        }
    }
    return NULL;
}

    


//打印歌曲信息
void print_song_message(char * song_msg[]){
    int i=0;
    char *name[4]={"歌名:","歌手:","专辑:","制作:"};
    clear_screen();
    cusor_hide();
    set_fg_color(COLOR_BLUE);
    for(i=0;i<4;i++){
        if(song_msg[i]!=NULL){
            cusor_moveto(35,i+1);//移动位置
            printf("%s %s\n",name[i],song_msg[i]);
    }
}
}
//在屏幕上打印时间
void time_printf(TIME m_time){
    char str[30];
    sprintf(str,"%02d:%02d",m_time.minute,m_time.second);
    color_pos_printf(str,40,5,COLOR_BLUE);
}
//打印的第一个节点
void lrc_printf(LRC *start_point,LRC *pb){
    int i;
    for(i=0;i<5;i++){
        if(start_point==pb){
            color_pos_printf(start_point->lrc_buf,30,6+i,COLOR_RED);
        }
            else
            {
            color_pos_printf(start_point->lrc_buf,30,6+i,COLOR_BLUE);    
            }
            start_point=start_point->next;
        }
    }
    void color_pos_printf(char *str,int x,int y,int color){
        cusor_moveto(x,y);
        set_fg_color(color);
        printf("%s\n",str);
    }


//功能在head指向的链表中，pb指向正在唱的节点
LRC *find_start_point(LRC *head,LRC*pb){
    if(pb==head){//第一个节点
        return head;

    }else if((pb->front)==head){
        return head;
    }else if(pb->next==NULL){
        //判断 最后几个节点，如果是，返回倒数第五个节点的地址
        return(((pb->front)->front)->front)->front;
    }else if(pb->next->next==NULL){
        return (((pb->front)->front)->front);
    }else{
        return (pb->front)->front;
    }
}
//时钟成员
void m_clock(TIME *p_m_time){
    (*p_m_time).second++;
    if((*p_m_time).second==60){
        (*p_m_time).second=0;
        ++(*p_m_time).minute;
        if((*p_m_time).second==60){
        (*p_m_time).second=0;
    }
}
}
//释放歌曲信息
void song_msg_free(char *song_msg[]){
    int i;
    for(i=0;i<4;i++){
        if(song_msg[i]!=NULL){
            free(song_msg[i]);
        }
    }
}

    