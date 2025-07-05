#define _GNU_SOURCE
#ifndef POOL_H
#define POOL_H

#include "types.h"
#include "utils.h"
#include <sys/mman.h>

static inline int find_freec() {
  uint64_t *word;
  for (size_t i = 0; i < config.max_concurrent_clients; i++) {
    word = &GETCW(i);

    if (*word == 0) {
      i += (BITMAP_SIZE - 1);
      continue;
    }

    i += __builtin_ctzll(*word);
    USEC(i);
    return i;
  }

  return -1;
}

static inline int MP_init(size_t npages) {
  if (!npages)
    return -1;
  size_t   bitmap_size, pool_size, iov_pool_size;
  uint32_t cmax;

  /*** Setup ***/
  pool.npages = npages;
  cmax = config.max_concurrent_clients;
  pool_size = pool.npages * pagesize;
  iov_pool_size = (sizeof(IOV) * MAX_SEND_IOV) * cmax;

  /*** Pool ***/
  pool.bpool = new_mem(pool_size);
  if (!pool.bpool)
    return -1;
  pool.nblocks = pool_size / MP_BLOCK;

  /*** Cpool ***/
  pool.cpool = (Conn *)malloc(sizeof(Conn) * cmax);
  if (!pool.cpool)
    return -1;

  /*** IOVpool ***/
  pool.iovpool = (IOV *)malloc(iov_pool_size);
  if (!pool.iovpool)
    return -1;

  /*** RECpool ***/
  pool.recpool = (IOV *)malloc(iov_pool_size);
  if (!pool.recpool)
    return -1;

  for (size_t i = 0; i < cmax; i++) {
    pool.cpool[i].send.iov = GET_CIOV(i);
    pool.cpool[i].send.rec = GET_CREC(i);
  }

  /*** Freebs ***/
  bitmap_size = BITMAP_ELEMENTS(pool.nblocks);
  pool.freebs = malloc(sizeof(uint64_t) * bitmap_size);
  if (!pool.freebs)
    return -1;
  for (size_t i = 0; i < bitmap_size; i++)
    pool.freebs[i] = UINT64_MAX;

  /*** Freecs ***/
  bitmap_size = BITMAP_ELEMENTS(cmax);
  pool.freecs = malloc(sizeof(uint64_t) * bitmap_size);
  if (!pool.freecs)
    return -1;
  for (size_t i = 0; i < bitmap_size; i++)
    pool.freecs[i] = UINT64_MAX;

  return 0;
}

static inline int MP_use_blks(size_t nblocks) {
  if (!nblocks || nblocks > pool.nblocks)
    return -1;

  nblocks = CEIL_BLOCKS(nblocks);
  size_t max_start = pool.nblocks - nblocks;
  size_t full_chunks = (nblocks / BITMAP_SIZE) * BITMAP_SIZE;

  for (size_t i = 0; i < max_start; i++) {
    bool free = true;

    if (BW_IS_USED(i)) {
      i += (BITMAP_SIZE - 1);
      continue;
    }

    for (size_t j = 0; j < full_chunks; j += BITMAP_SIZE) {
      if (!BW_IS_FREE(i + j)) {
        free = false;
        i += (BITMAP_SIZE - 1);
        break;
      }
    }

    if (free) {
      for (size_t j = 0; j < full_chunks; j += 64)
        USEBW(i + j);

      return i;
    }
  }

  return -1;
}

static inline int MP_free_blks(size_t bindex, size_t nblocks) {
  if (!nblocks || nblocks > pool.nblocks)
    return -1;

  nblocks = CEIL_BLOCKS(nblocks);
  size_t full_chunks = (nblocks / BITMAP_SIZE) * BITMAP_SIZE;
  for (size_t i = 0; i < full_chunks; i += BITMAP_SIZE)
    FREEBW(bindex + i);

  void *blk = GET_POOL_BY_INDEX(bindex);
  memset(blk, 0, nblocks * MP_BLOCK);
  return 0;
}

static inline Conn *MP_use(int fd, size_t recv_nblocks, size_t send_nblocks) {
  int cindex = find_freec();
  if (cindex < 0)
    return NULL;

  Conn *conn = &pool.cpool[cindex];
  memset(conn, 0, sizeof(Conn));

  conn->fd = fd;
  conn->cindex = cindex;
  conn->timeout.op = TIMEOUT;
  conn->timeout.conn = conn;
  conn->send.iov = GET_CIOV(cindex);
  conn->send.rec = GET_CREC(cindex);

  void *mem, *recv_mem, *send_mem;
  int   bindex = MP_use_blks(recv_nblocks + send_nblocks);
  if (bindex < 0) {
    if (config.pool_only || (mem = new_mem(recv_nblocks + send_nblocks)) == NULL)
      return NULL;

    recv_mem = mem;
    send_mem = mem + recv_nblocks;
  } else {
    recv_mem = GET_POOL_BY_INDEX(bindex);
    send_mem = GET_POOL_BY_INDEX(bindex + recv_nblocks);
  }

  IOV *iov, *rec;
  if (recv_nblocks > 0) {
    rec = &conn->recv.rec[0];
    iov = &conn->recv.iov[0];
    rec->iov_base = recv_mem;
    rec->iov_len = BLOCKS_TO_BYTES(recv_nblocks);
    memcpy(iov, rec, sizeof(IOV));
  }

  if (send_nblocks > 0) {
    conn->send.iovlen++, conn->send.reclen++;
    rec = &conn->send.rec[0];
    iov = &conn->send.iov[0];
    rec->iov_base = send_mem;
    rec->iov_len = BLOCKS_TO_BYTES(send_nblocks);
    memcpy(iov, rec, sizeof(IOV));
  }

  conn->timeout.last_used = get_time();
  return conn;
}

static inline void MP_shed(IOV *rec, size_t iovlen) {
  size_t bindex, nblocks;

  for (size_t i = 0; i < iovlen; i++) {
    rec = &rec[i];
    if (!rec || !rec->iov_base)
      continue;
    if (!IN_POOL(rec->iov_base)) {
      munmap(rec->iov_base, rec->iov_len / pagesize);
    } else {
      bindex = GETBI(rec->iov_base);
      nblocks = rec->iov_len / MP_BLOCK;
      MP_free_blks(bindex, nblocks);
    }
    memset(rec, 0, sizeof(IOV));
  }
}

static inline int MP_expand(Conn *conn, size_t nblocks, bool once) {
  if (!conn || conn->fd == -1 || !nblocks)
    return -1;

  IOV  *iov, *rec;
  void *bptr;
  int   iov_index, bindex;

  iov_index = conn->send.reclen;
  conn->send.iovlen++, conn->send.reclen++;
  iov = &conn->send.iov[iov_index];
  rec = &conn->send.rec[iov_index];

  if (once) {
    bptr = new_mem(nblocks);
    if (!bptr)
      return -1;
    rec->iov_base = bptr;
    rec->iov_len = ALIGN_TO_PAGESIZE(nblocks * MP_BLOCK);
    memcpy(iov, rec, sizeof(IOV));
    return iov_index;
  }

  bindex = MP_use_blks(nblocks);
  if (bindex < 0) {
    bptr = new_mem(nblocks);
    if (!bptr)
      return -1;
    iov->iov_len = ALIGN_TO_PAGESIZE(nblocks * MP_BLOCK);
    rec->iov_len = iov->iov_len;
  } else {
    bptr = pool.bpool + (bindex * MP_BLOCK);
    iov->iov_len = nblocks * MP_BLOCK;
    rec->iov_len = iov->iov_len;
  }

  iov->iov_base = bptr;
  rec->iov_base = iov->iov_base;
  return iov_index;
}

static inline int MP_free(Conn *conn) {
  if (!conn)
    return -1;

  size_t cindex = conn->cindex;
  MP_shed(conn->recv.rec, 2);
  MP_shed(conn->send.rec, conn->send.reclen);
  if (IS_FREEC(cindex))
    return 0;

  if (conn->fd != -1 && !(conn->flags & CF_SKIP_SOCK))
    close(conn->fd);

  FREEC(cindex);
  memset(conn, 0, sizeof(Conn));
  conn->fd = -1;
  return 0;
}

static inline void MP_clear(Conn *conn) {
  if (!conn || conn->fd == -1)
    return;

  if (!(conn->flags & CF_CANCEL)) {
    if (conn->cqe_count > 0)
      ucancel(conn);
    conn->flags |= CF_CANCEL;
  }

  if (conn->cqe_count > 0)
    return;

  shutdown(conn->fd, SHUT_RDWR);
  MP_free(conn);
}

static inline void MP_exit(MPool *pool) {
  munmap(pool->bpool, pool->npages);
  free((void *)pool->cpool);
  free((void *)pool->freebs);
  free((void *)pool->freecs);
}

#endif
