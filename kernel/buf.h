struct buf
{
  int valid; // has data been read from disk?
  int disk;  // does disk "own" buf?
  uint dev;
  uint blockno;
  struct sleeplock lock;
  uint refcnt;
  struct buf *prev; // LRU cache list
  struct buf *next;
  uchar data[BSIZE];
};

/*
    字段valid是缓存区包含了一个块的复制（即该buffer包含对应磁盘块的数据）;
    字段disk是缓存区的内容是否已经被提交到了磁盘;
    字段dev是设备号;
    字段blockno是缓存的磁盘块号;
    字段refcnt是该块被引用次数（即被多少个进程拥有）;
    lock是睡眠锁。
*/