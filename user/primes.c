#include "kernel/types.h"
#include "user.h"
// read会返回0，当读到文件末尾

void myprime(int p[2])
{
    int prime;
    close(p[1]); //准备从管道中读入数字到prime
    if (!read(p[0], &prime, sizeof(int)))
    {
        close(p[0]);
        exit(0);
    }
    else
    {
        printf("prime %d\n", prime);
        int pc[2]; //创建子管道
        pipe(pc);
        if (fork() == 0)
        {
            close(p[0]);
            myprime(pc);
        }
        else//注意 每次调用fork()函数都会新建一次子进程，因此不能随意调用fork函数
        {
            close(pc[0]); //后面会需要想子管道中输入数据，因此先关闭
            int n_prime;
            while (read(p[0], &n_prime, sizeof(int))!=0) //读入上一级管道中的所有剩余数字
            {
                if (n_prime % prime != 0) //如果该数字没有被排除
                {                         //则加入下一级管道
                    write(pc[1], &n_prime, sizeof(int));
                }
            }

            close(p[0]);
            close(pc[1]); 
            wait(0);      //等待子进程
            exit(0);
        }
    }
    exit(0);
}
/*
管道
*/
int main(int argc, char *argv[])
{
    int p[2];
    pipe(p); //创建管道
    if (fork() == 0)
    { //子进程
        myprime(p);
    }
    else
    {                //父进程
        close(p[0]); //父进程需要向管道中写入数据
        for (int i = 2; i <= 35; i++)
        {
            write(p[1], &i, sizeof(int)); //写入所有备选数字
        }
        close(p[1]);
        wait(0);
        exit(0);
    }
    exit(0);
}