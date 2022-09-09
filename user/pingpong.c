#include "kernel/types.h"
#include "user.h"

int main(int argc, char *argv[])
{
    int pp[2]; //父进程接受 子进程写
    int pc[2]; //父进程写 子进程接受
    //考虑到需要来回通信，因此我们建立两个管道

    pipe(pp);
    pipe(pc);

    int id = fork();
    //创建进程id

    char buffer[20];
    //创建管道

    if (id == 0)
    {                                        //子进程
        close(pc[1]);                        //关闭子进程管道写端
        read(pc[0], buffer, sizeof(buffer)); //子进程从子进程管道读入
        printf("%d: received %s\n", getpid(), buffer);
        close(pc[0]);//读取完成，关闭读端

        close(pp[0]); //关闭父进程管道读端
        write(pp[1], "pong", sizeof(buffer));
        close(pp[1]);
        exit(0);
    }
    //管道的读取是阻塞的，父子进程同时进行，子进程运行一开始是读pc，而此时父进程还没有写入
    //如果父进程不去处理pc，转而处理pp，那么两个进程就会同时阻塞
    //因此父进程应该一开始就去处理pc，与子进程保持一致
    else
    { //父进程
        close(pc[0]);
        write(pc[1], "ping", sizeof(buffer));
        close(pc[1]);

        close(pp[1]); //关闭父进程管道写端
        read(pp[0], buffer, sizeof(buffer));
        close(pp[0]);
        
        printf("%d: received %s\n", getpid(), buffer);
        wait(0);
        exit(0);
    }
}