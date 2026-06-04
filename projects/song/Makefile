CC = gcc
# 添加 -fPIE 以解决链接错误，添加 -I./console 以找到头文件
CFLAGS = -Wall -g -pthread -I./console -fPIE

TARGET = play

# 显式列出所有生成的 .o 文件路径
OBJS = main.o lrc.o start_mplayer.o ./console/console.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

# 编译当前目录下的 .c 文件
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 专门针对 console 文件夹下的编译规则
console/console.o: console/console.c
	$(CC) $(CFLAGS) -c console/console.c -o console/console.o

# 清理时，连同 console 文件夹里的 .o 也一起删掉
clean:
	rm -f *.o console/*.o $(TARGET)