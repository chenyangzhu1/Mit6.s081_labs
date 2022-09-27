#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"
int main(int argc, char *argv[])
{
    char *myargv[MAXARG]; //创建新的参数组保存参数并声明长度

    for (int i = 0; i < argc - 1; i++)
    {
        myargv[i] = argv[i + 1]; //对于后序字符串而言，操作符是不需要的，因此把后序有效的参数存到新数组中
    }

    char buffer[MAXARG]; //存储操作参数

    while (1) //后续会不断读入
    {
        int tag = 0;                                                //标记长度
        while (read(0, &buffer[tag], 1) > 0 && buffer[tag] != '\n') //读取单独的输入行，直到出现换行符
        {
            tag++;
        }
        if (!tag)
        {
            break;
        }
        buffer[tag] = 0; //最后一位置为0
        myargv[argc - 1] = buffer;

        int ret = fork();

        if (ret == 0)
        {
            exec(myargv[0], myargv);
            exit(0); //运行子进程
        }
        else
        {
            wait(0); //等待子进程结束
        }
    }
    exit(0);
}