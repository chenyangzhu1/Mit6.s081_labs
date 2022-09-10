#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"
#include "kernel/param.h"
int main(int argc, char *argv[])
{
    char *argv_new[MAXARG]; //创建新的参数组保存参数并声明长度

    for (int i = 0; i < argc - 1; i++)
    {
        argv_new[i] = argv[i + 1];
    }

    char buf[MAXARG];
    while (1)
    {
        int i = 0;
        while (read(0, &buf[i], 1) > 0 && buf[i] != '\n')
        {
            i++;
        }
        if (!i)
        {
            break;
        }
        buf[i] = 0;
        argv_new[argc - 1] = buf;

        int ret = fork();

        if (ret == 0)
        {
            exec(argv_new[0], argv_new);
            exit(0);
        }
        else
        {
            wait(0);
        }
    }
    exit(0);
}