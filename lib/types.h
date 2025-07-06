#define _GNU_SOURCE
#ifndef TYPES_H
#define TYPES_H

#include "hunk.h"
#include "liburing.h"

#define IOURING_QUEUE_LIMIT (4096)

#define DEF_HTTP_PORT    (80)  // Default http port
#define DEF_HTTP_TLCPORT (443) // Default https port

#define MP_BLOCK    (4)
#define KB          (1024)
#define ZC_RES      (KB * 64)
#define BITMAP_SIZE (64)

#define STRLEN(s)      (sizeof(s) - 1)
#define PTR_DIFF(x, y) ((ptr_diff((uintptr_t)x, (uintptr_t)y)))

#define BIT_IS_FREE(word, bit)   (((word) >> (bit)) & 1)
#define MARK_BIT_USED(word, bit) ((word) &= ~(1ULL << (bit)))
#define MARK_BIT_FREE(word, bit) ((word) |= (1ULL << (bit)))

#define GETBI(ptr) (PTR_DIFF(ptr, pool.bpool) / MP_BLOCK)
#define GETCI(ptr) (PTR_DIFF(ptr, pool.cpool) / sizeof(Conn))

#define USEC(cindex)       (MARK_BIT_USED(pool.freecs[(cindex) / BITMAP_SIZE], (cindex)))
#define FREEC(cindex)      (MARK_BIT_FREE(pool.freecs[(cindex) / BITMAP_SIZE], (cindex)))
#define IS_FREEC(cindex)   (BIT_IS_FREE(pool.freecs[(cindex) / BITMAP_SIZE], (cindex)))
#define GETCW(cindex)      (pool.freecs[(cindex) / BITMAP_SIZE])
#define FREECW(cindex)     (GETCW((cindex)) = UINT64_MAX)
#define USECW(cindex)      (GETCW((cindex)) = 0)
#define CW_IS_FREE(cindex) (GETCW((cindex)) == UINT64_MAX)
#define CW_IS_USED(cindex) (GETCW((cindex)) == 0)

#define MAX_SEND_IOV       (config.max_writes_per_handler + 1)
#define GET_CIOV(cindex)   (&pool.iovpool[(cindex)*MAX_SEND_IOV])
#define GET_CREC(cindex)   (&pool.recpool[(cindex)*MAX_SEND_IOV])
#define GETBW(bindex)      (pool.freebs[(bindex) / BITMAP_SIZE])
#define FREEBW(bindex)     (GETBW((bindex)) = UINT64_MAX)
#define USEBW(bindex)      (GETBW((bindex)) = 0)
#define BW_IS_FREE(bindex) (GETBW((bindex)) == UINT64_MAX)
#define BW_IS_USED(bindex) (GETBW((bindex)) == 0)

#define GET_POOL_BY_INDEX(bindex) (pool.bpool + ((bindex)*MP_BLOCK))
#define BLOCKS_TO_BYTES(nblocks)  ((nblocks)*MP_BLOCK);
#define BITMAP_ELEMENTS(elements) (((elements) + (BITMAP_SIZE - 1)) / BITMAP_SIZE)

#define IN_POOL(ptr)                 ((void *)ptr >= pool.bpool && (void *)ptr <= (pool.bpool + (pool.npages * pagesize)))
#define ALIGN_TO_PAGESIZE(buff_size) (((buff_size) + (pagesize - 1)) & ~(pagesize - 1))
#define BYTES_TO_PAGES(size)         (ALIGN_TO_PAGESIZE((size)) / pagesize)
#define BYTES_TO_BLOCKS(buff_size)   (((buff_size) + (MP_BLOCK - 1)) / MP_BLOCK)
#define CEIL_BLOCKS(nblocks)         (((nblocks) + 63) & ~63)

typedef struct iovec  IOV;
typedef struct msghdr MSG;

typedef enum UOP {
  POLL = IORING_OP_POLL_ADD,
  SENDMSG = IORING_OP_SENDMSG,
  ACCEPT = IORING_OP_ACCEPT,
  SEND = IORING_OP_SEND,
  RECV = IORING_OP_RECV,
  SENDMSGZC = IORING_OP_SENDMSG_ZC,
  PEEK = 50,
  FRECV,
  CANCEL,
  TIMEOUT,
} UOP;

typedef struct Conntimeout {
  uint8_t  op;
  uint64_t last_used;
  void    *conn;
} Conntimeout;

typedef struct Conn {
  uint8_t op;

  int fd;
  struct {
    IOV      iov[2];
    IOV      rec[2];
    uint64_t len;
  } recv;

  struct {
    IOV *iov;
    IOV *rec;
    MSG  msg;

    uint16_t iovlen;
    uint16_t reclen;
    uint16_t zc_notifs;
    uint64_t len;
  } send;

  Conntimeout timeout;
} Conn;

typedef struct MPool {
  // The allocated pool of blocks
  void *bpool;

  // The allocated pool of connections
  Conn *cpool;

  IOV *iovpool;

  IOV *recpool;

  // The size of the pool in pages
  uint32_t npages;

  // The size of the pool in blocks
  uint32_t nblocks;

  // The availability of each block
  uint64_t *freebs;

  // The availability of each conn
  uint64_t *freecs;

  // Socket read timeout
  uint32_t timeout;
} MPool;

extern struct io_uring ring;

extern ServConfig config;

extern int pipe_sz;
extern int pipe_in, pipe_out, nullfd;

extern MPool    pool;
extern Conn    *current_conn;
extern Request *current_req;
extern size_t   pagesize;
#endif
