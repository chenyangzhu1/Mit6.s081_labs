#include "kernel/types.h"
#include "user.h"

void myprime(int p[2])
{
    int prime;   //创建本进程的基准素数
    close(p[1]); //准备从管道中读入数字到prime
    if (!read(p[0], &prime, sizeof(int)))
    // read会返回0，当读到文件末尾
    {
        close(p[0]); //关闭描述符
        exit(0);     //子进程退出
    }
    else
    {
        printf("prime %d\n", prime);
        int pc[2]; //创建子管道
        pipe(pc);
        if (fork() == 0)
        {
            close(p[0]);
            myprime(pc); //开启下一级管道
            //由于myprime里面已经写好了exit，因此子进程这里写不写都一样
        }
        else //注意 每次调用fork()函数都会新建一次子进程，因此不能随意调用fork函数
        {
            close(pc[0]);                                  //后面会需要想子管道中输入数据，因此先关闭
            int n_prime;                                   //需要被筛选的数字
            while (read(p[0], &n_prime, sizeof(int)) != 0) //读入上一级管道中的所有剩余数字
            {
                if (n_prime % prime != 0) //如果该数字没有被排除
                {                         //则加入下一级管道
                    write(pc[1], &n_prime, sizeof(int));
                }
            }

            close(p[0]);  //与第7行成对
            close(pc[1]); //与第26行成对
            wait(0);      //等待子进程
            exit(0);      //主进程等待子进程结束以后就可以退出
        }
    }
    exit(0);
}
/*
管道在父进程中关闭描述符对子进程没有影响？
*/
int main(int argc, char *argv[])
{
    int p[2];
    pipe(p); //创建管道
    if (fork() == 0)
    { //子进程
        myprime(p);
        //由于myprime里面已经写好了exit，因此子进程这里写不写都一样
    }
    else
    {                //父进程
        close(p[0]); //父进程需要向管道中写入数据
        for (int i = 2; i <= 35; i++)
        {
            write(p[1], &i, sizeof(int)); //写入所有备选数字
        }
        close(p[1]); //关闭描述符
        wait(0);     //等待子进程结束
        exit(0);//如果if-else之外还有exit，那么这里的exit可以去掉
    }
    exit(0);
    //这里的exit不能去掉，会返回error return type,下面加一个return 0 就可以了
    // return 0;
}