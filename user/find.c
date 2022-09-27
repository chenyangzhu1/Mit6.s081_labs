#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"
#include "kernel/fs.h"

char *fmtname(char *path)
{
    char *p;

    // Find first character after last slash.
    for (p = path + strlen(path); p >= path && *p != '/'; p--)
        ;
    p++;

    return p;
    //这里仅仅需要返回斜杠后面的第一个字符串即可
}

void myfind(char *path, char *filename)
{
    char buf[512], *p;
    int fd;
    struct dirent de;
    struct stat st;

    if ((fd = open(path, 0)) < 0)
    {
        fprintf(2, "find: cannot open %s\n", path);
        return;
    }

    if (fstat(fd, &st) < 0)
    {
        fprintf(2, "find: cannot stat %s\n", path);
        close(fd);
        return;
    }

    switch (st.type)
    {
    case T_FILE:
        // printf("%s %d %d %l\n", fmtname(path), st.type, st.ino, st.size);
        if (strcmp(fmtname(path), filename) == 0) //如果说路径下的文件名就是我们要找的，直接输出
        {
            printf("%s\n", path);
        }
        break;

    case T_DIR:
        if (strlen(path) + 1 + DIRSIZ + 1 > sizeof buf) //处理path过长
        {
            printf("ls: path too long\n");
            break;
        }
        strcpy(buf, path);
        p = buf + strlen(buf);
        *p++ = '/';
        while (read(fd, &de, sizeof(de)) == sizeof(de))
        {
            if (de.inum == 0 || strcmp(de.name, ".") == 0 || strcmp(de.name, "..") == 0) //为了不递归
                continue;
            memmove(p, de.name, DIRSIZ);
            p[DIRSIZ] = 0; //最后一位置为0
            // if (stat(buf, &st) < 0)
            // {
            //     printf("ls: cannot stat %s\n", buf);
            //     continue;
            // }
            // printf("%s %d %d %d\n", fmtname(buf), st.type, st.ino, st.size);
            myfind(buf, filename);
        }
        break;
    }
    close(fd);
}

int main(int argc, char *argv[])
{
    if (argc < 2)
    {
        printf("Two parameters are needed\n");
        exit(1);
    }
    else
    {
        myfind(argv[1], argv[2]);
        // argv[0]存的是操作符的字符串
    }
    exit(0);
}