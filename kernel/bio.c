// Buffer cache.
//
// The buffer cache is a linked list of buf structures holding
// cached copies of disk block contents.  Caching disk blocks
// in memory reduces the number of disk reads and also provides
// a synchronization point for disk blocks used by multiple processes.
//
// Interface:
// * To get a buffer for a particular disk block, call bread.
// * After changing buffer data, call bwrite to write it to disk.
// * When done with the buffer, call brelse.
// * Do not use the buffer after calling brelse.
// * Only one process at a time can use a buffer,
//     so do not keep them longer than necessary.

#include "types.h"
#include "param.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "riscv.h"
#include "defs.h"
#include "fs.h"
#include "buf.h"
#define NBUC 13

//根据指导书的说法定义为13，但感觉定义为23 29，会有更好的效果

/*依据hint
It is OK to use a fixed number of buckets and not resize the hash table dynamically.
Use a prime number of buckets (e.g., 13) to reduce the likelihood of hashing conflicts.
*/
struct
{
  struct spinlock lock;
  struct buf head;
} buckets[NBUC];
//桶结构   数组
/*各自的bcaches[i].head维护一条双向链表*/

struct
{
  struct spinlock lock;
  struct buf buf[NBUF];

  // Linked list of all buffers, through prev/next.
  // Sorted by how recently the buffer was used.
  // head.next is most recent, head.prev is least.
  // struct buf head;
  //去掉head，因为我们这里为每个桶上锁，因此不再需要head，直接使用bucket中的head即可
} bcache;

/*将buffer cache分为若干桶，每个桶加一个单独的锁来保护*/

void binit(void)
{ //初始化
  struct buf *b;

  initlock(&bcache.lock, "bcache");

  // Create linked list of buffers
  for (int i = 0; i < NBUC; i++)
  {
    initlock(&buckets[i].lock, "bcache.buckets");
    //除了bcache本身的锁以外
    //所有的13个桶也要上锁
    buckets[i].head.prev = &buckets[i].head;
    buckets[i].head.next = &buckets[i].head;
    //上锁方式仿照原本的bcache，就下面被注释的两行
  }

  // bcache.head.prev = &bcache.head;
  // bcache.head.next = &bcache.head;
  for (b = bcache.buf; b < bcache.buf + NBUF; b++)
  {
    int hashnum = b->blockno % NBUC;
    b->next = buckets[hashnum].head.next;
    b->prev = &buckets[hashnum].head;

    // b->next = bcache.head.next;
    // b->prev = &bcache.head;
    initsleeplock(&b->lock, "buffer");
    buckets[hashnum].head.next->prev = b;
    buckets[hashnum].head.next = b;

    // bcache.head.next->prev = b;
    // bcache.head.next = b;
  }
}

// Look through buffer cache for block on device dev.
// If not found, allocate a buffer.
// In either case, return locked buffer.
static struct buf *
bget(uint dev, uint blockno)
{ /*为device dev返回一个序号为blockno的buffer，并更新其在buffer cache中的位置。*/
  struct buf *b;
  int hashnum = blockno % NBUC;
  //直接用序号对桶的个数取模，作为准备进入的哈希桶的序号
  //这一段代码就是仿照下一段代码进行修改，把进入bcache改成进入哈希桶，基本原理都是一样的
  // Is the block already cached?
  acquire(&buckets[hashnum].lock); //先获取锁
  for (b = buckets[hashnum].head.next; b != &buckets[hashnum].head; b = b->next)
  { //利用head进行链表遍历
    if (b->dev == dev && b->blockno == blockno)
    { //找到对应的blockno，则标记引用，并返回
      b->refcnt++;
      release(&buckets[hashnum].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  // acquire(&bcache.lock);//操作之前先上锁

  // // Is the block already cached?
  // for(b = bcache.head.next; b != &bcache.head; b = b->next){
  //   if(b->dev == dev && b->blockno == blockno){
  //     b->refcnt++;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     return b;
  //     //如果匹配到了，就返回
  //   }
  // }

  //如果没有命中，第一种想法是不麻烦别人，自己先考虑解决
  //简单的说就是看看自己的桶里面有没有空着的块，也就是refcnt=0，把它拿过来，修改成现在我们需要的块即可
  for (b = buckets[hashnum].head.next; b != &buckets[hashnum].head; b = b->next)
  {
    if (b->refcnt == 0)
    {
      b->dev = dev;
      b->blockno = blockno;
      b->valid = 0;
      b->refcnt = 1; //把这一块改成我们需要的块，并返回
      release(&buckets[hashnum].lock);
      acquiresleep(&b->lock);
      return b;
    }
  }
  //自己也没找到，那没办法，只能问别人借
  //直接去别的桶里面找
  for (int i = 0; i < NBUC; i++)
  {
    if (i != hashnum) //确保是进了别人的桶
    {
      acquire(&buckets[i].lock);
      for (b = buckets[i].head.prev; b != &buckets[i].head; b = b->prev) //遍历桶中的每个块
      {
        if (b->refcnt == 0) //如果找到了一个没被占用的块
        {
          b->dev = dev;
          b->blockno = blockno;
          b->valid = 0;
          b->refcnt = 1; //首先把这个块改造成我们需要的样子
          b->next->prev = b->prev;
          b->prev->next = b->next; //把这个b从原来的链表中断开
          b->next = buckets[hashnum].head.next;
          b->prev = &buckets[hashnum].head;
          buckets[hashnum].head.next->prev = b;
          buckets[hashnum].head.next = b; //把这个b放到我们原本的桶的链表表头
          release(&buckets[hashnum].lock);
          release(&buckets[i].lock); //这个部分我们其实同时获取了两把锁
          acquiresleep(&b->lock);
          return b; //返回b
        }
      }
      release(&buckets[i].lock); //也有可能到这里还是没找到，还得解锁
    }
  }
  release(&buckets[hashnum].lock); //也有可能到这里还是没找到，还得解锁

  // Not cached.
  // Recycle the least recently used (LRU) unused buffer.
  // for(b = bcache.head.prev; b != &bcache.head; b = b->prev){
  //   if(b->refcnt == 0) {//refcnt为0表示没有被别的设备引用过
  //     b->dev = dev;
  //     b->blockno = blockno;
  //     b->valid = 0;
  //     b->refcnt = 1;
  //     release(&bcache.lock);
  //     acquiresleep(&b->lock);
  //     /*acquiresleep()：查询b.lock是否被锁，如果被锁了，就睡眠，让出CPU，
  //     直到wakeup()唤醒后，获取到锁，并将b.lock置1*/
  //     return b;
  //     //找到别的地方的空闲区，返回之
  // }
  // }
  panic("bget: no buffers");
}

// Return a locked buf with the contents of the indicated block.
struct buf *
bread(uint dev, uint blockno)
{
  struct buf *b;

  b = bget(dev, blockno);
  if (!b->valid)
  {
    virtio_disk_rw(b, 0);
    b->valid = 1;
  }
  return b;
}

// Write b's contents to disk.  Must be locked.
void bwrite(struct buf *b)
{
  if (!holdingsleep(&b->lock))
    panic("bwrite");
  virtio_disk_rw(b, 1);
}

// Release a locked buffer.
// Move to the head of the most-recently-used list.
void brelse(struct buf *b)
{ /*它实现的是告知buffer cache某个buffer不再被设备调用，
 实现方式在于将该buffer对应的refcnt减一，
 而当这个refcnt被减小到0时，则没有任何调用该buffer的设备，
 它处于空闲状态，于是要将这个buffer从buffer cache中主动移到链表最前位，
 从而实现对buffer cache双向链表性质的维护。
 */
  if (!holdingsleep(&b->lock))
    panic("brelse");

  releasesleep(&b->lock);
  int hashnum = b->blockno % NBUC;

  // acquire(&bcache.lock);
  //修改成对应的bucket操作即可
  acquire(&buckets[hashnum].lock);
  b->refcnt--;
  if (b->refcnt == 0)
  {
    // no one is waiting for it.
    b->next->prev = b->prev;
    b->prev->next = b->next;
    b->next = buckets[hashnum].head.next;
    b->prev = &buckets[hashnum].head;
    buckets[hashnum].head.next->prev = b;
    buckets[hashnum].head.next = b;
  }
  release(&buckets[hashnum].lock);
  // release(&bcache.lock);
}

void bpin(struct buf *b)
{
  //修改成对应的bucket操作即可
  int hashnum = b->blockno % NBUC;
  acquire(&buckets[hashnum].lock);
  b->refcnt++;
  release(&buckets[hashnum].lock);
}

void bunpin(struct buf *b)
{
  //修改成对应的bucket操作即可
  int hashnum = b->blockno % NBUC;

  acquire(&buckets[hashnum].lock);
  b->refcnt--;
  release(&buckets[hashnum].lock);
}
