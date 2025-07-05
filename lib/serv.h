#define _GNU_SOURCE
#ifndef SERV_H
#define SERV_H

#include "pool.h"
#include "uring.h"
#include <sys/sysinfo.h>

static inline void new_conn(int connfd) {
  size_t nblocks = round_to_blocks(config.max_headers_size);
  current_conn = MP_use(connfd, nblocks, nblocks);
  if (!current_conn)
    return (void)close(connfd);

  if (ufrecv(current_conn) < 0 || (pool.timeout > 0 && utimeout(current_conn) < 0))
    return MP_clear(current_conn);
}

static inline int resubmit_sendmsg(Conn *conn, int res) {
  IOV   *iov;
  size_t nbytes = res;
  size_t iov_index = 0;
  while (conn->send.iov[iov_index].iov_base == NULL)
    iov_index++;

  for (size_t i = iov_index; nbytes > 0; i++) {
    iov = &conn->send.iov[i];

    if (nbytes >= iov->iov_len) {
      nbytes -= iov->iov_len;
      memset(iov, 0, sizeof(IOV));
      continue;
    }

    iov->iov_base += nbytes;
    iov->iov_len -= nbytes;
    iov_index = i;
    break;
  }

  return usendmsg(conn, iov_index, conn->send.iovlen - iov_index);
}

static inline int handle_sendmsg_complete(Conn *conn) {
  IOV *iov, *rec;

  MP_shed(&conn->send.rec[1], conn->send.reclen - 1);
  conn->send.reclen = 1;
  memset(conn->send.iov, 0, sizeof(IOV) * conn->send.iovlen);
  conn->send.iovlen = conn->send.reclen;
  iov = &conn->send.iov[0];
  rec = &conn->send.rec[0];
  memcpy(iov, rec, sizeof(IOV));

  MP_shed(&conn->recv.rec[1], 1);
  memset(conn->recv.iov, 0, sizeof(IOV));
  iov = &conn->recv.iov[0];
  rec = &conn->recv.rec[0];
  memcpy(iov, rec, sizeof(IOV));

  return ufrecv(conn);
}

static inline int handle_sendmsg(Conn *conn, int res, bool zc) {
  if (!conn || conn->fd == -1)
    return -1;

  conn->send.len -= res;
  if (conn->send.len > 0)
    return resubmit_sendmsg(conn, res);

  if (zc)
    return 0;

  return handle_sendmsg_complete(conn);
}

static inline int find_route(Server *serv, char *bptr) {
  Request req;
  memset(&req, 0, sizeof(Request));
  if (read_method(&req.method, bptr) < 0)
    return -1;

  char *space = strchr(bptr, ' ');
  req.path = (space + 1);
  space = strchr(req.path, ' ');
  *space = '\0';
  int route_index = match_route(serv, &req);
  *space = ' ';
  if (route_index == -1)
    return -1;

  return route_index;
}

static inline int run_handler(Server *serv, Conn *conn) {
  Param   params[config.max_nparams];
  Header  headers[config.max_nheaders];
  Request req;
  memset(&req, 0, sizeof(Request));
  req.params = (Param *)params;
  req.headers = (Header *)headers;

  ResWriter res = {0};
  Header    resHeaders[config.max_nheaders];
  res.headers = (Header *)resHeaders;

  if (read_req(&req, (char *)conn->recv.rec[0].iov_base) < 0)
    return -1;

  int route_index = match_route(serv, &req);
  if (route_index == -1) {
    send_empty_res(STATUSNOTFOUND);
    return -1;
  }

  conn->send.len = 0;
  bool uses_body = serv->routes[route_index].uses_body;
  bool has_body = conn->recv.len > 0;
  if (uses_body && has_body) {
    req.body.iovlen = 0;
    for (size_t i = 0; i < 2; i++)
      if (conn->recv.iov[i].iov_len > 0)
        req.body.iovlen++;

    req.body.iov = NULL;
    for (size_t i = 0; i < 2; i++) {
      if (conn->recv.iov[i].iov_len > 0) {
        req.body.iov = &conn->recv.iov[i];
        break;
      }
    }

    req.body.len = conn->recv.len;
  }
  serv->routes[route_index].handler(&req, &res);
  res.len = conn->send.len;
  if (sendmsg_res(&req, &res) < 0)
    return -1;

  int index = HK_get_header(&req, "connection");
  if (index != -1 && strcasecmp(req.headers[index].value, "close") == 0)
    return -1;

  return 0;
}

static inline int handle_frecv(Server *serv, Conn *conn, int res) {
  if (!conn || conn->fd == -1)
    return -1;

  IOV *iov, *rec;

  void  *mem;
  char  *bptr, *headEnd, *headstart;
  int    route_index;
  size_t head_size;
  bool   uses_body;
  long   body_size;

  bptr = (char *)conn->recv.iov[0].iov_base;
  headEnd = (char *)memmem(bptr, res, "\r\n\r\n", 4);
  if (!headEnd) {
    send_empty_res(STATUSREQUESTHEADERFIELDSTOOLARGE);
    return -1;
  }

  headstart = (char *)memmem(bptr, res, "\r\n", 2);
  if (!headstart) {
    send_empty_res(STATUSBADREQUEST);
    return -1;
  }

  route_index = find_route(serv, bptr);
  if (route_index == -1) {
    send_empty_res(STATUSNOTFOUND);
    return -1;
  }

  head_size = (headEnd + 4) - bptr;
  if (have_header(headstart, (headEnd + 4), "Expect", "100-Continue"))
    send_empty_res(STATUSCONTINUE);

  uses_body = serv->routes[route_index].uses_body;
  body_size = get_content_length(headstart + 2, headEnd + 4);
  if (body_size == -1) {
    if (((headEnd + 4) - bptr) == res || !uses_body) {
      body_size = 0;
    } else if (uses_body) {
      send_empty_res(STATUSLENGTHREQUIRED);
      return -1;
    }
  }
  conn->recv.len = body_size;

  if ((size_t)body_size > serv->config.max_req_body_size) {
    send_empty_res(STATUSCONTENTTOOLARGE);
    return -1;
  }

  if (!uses_body || !body_size)
    return run_handler(serv, conn);

  iov = &conn->recv.iov[0];
  iov->iov_base = bptr + head_size;
  iov->iov_len = (res - head_size);

  if (res == (int)(head_size + body_size))
    return run_handler(serv, conn);

  conn->recv.len = body_size - iov->iov_len;
  size_t nblocks = round_to_blocks(conn->recv.len);
  bool   once = (!config.pool_only && ((size_t)body_size > (size_t)((pool.nblocks * MP_BLOCK) / 10)));
  rec = &conn->recv.rec[1];
  if (once) {
    void *mem = new_mem(conn->recv.len);
    if (!mem)
      return -1;

    rec->iov_base = mem;
    rec->iov_len = ALIGN_TO_PAGESIZE(conn->recv.len);
  } else {
    int bindex = MP_use_blks(nblocks);
    if (bindex < 0) {
      if (config.pool_only || (mem = new_mem(conn->recv.len)) == NULL)
        return -1;

      rec->iov_base = mem;
      rec->iov_len = ALIGN_TO_PAGESIZE(conn->recv.len) * pagesize;
    } else {
      rec->iov_base = GET_POOL_BY_INDEX(bindex);
      rec->iov_len = nblocks * MP_BLOCK;
    }
  }

  iov = &conn->recv.iov[1];
  iov->iov_base = rec->iov_base;
  iov->iov_len = conn->recv.len;
  return urecv(conn, iov);
}

static inline int handle_recv(Server *serv, Conn *conn, int res) {
  if (!conn || conn->fd == -1)
    return -1;

  conn->recv.len -= res;
  if (conn->recv.len == 0) {
    conn->recv.iov[1].iov_base = conn->recv.rec[1].iov_base;
    conn->recv.len = conn->recv.iov[0].iov_len + conn->recv.iov[1].iov_len;
    return run_handler(serv, conn);
  }

  conn->recv.iov[1].iov_base += res;
  return urecv(conn, &conn->recv.iov[1]);
}

static inline void handle_timeout(Conntimeout *timeout) {
  Conn *conn = timeout->conn;
  conn->cqe_count--;

  uint64_t now = get_time();
  if (pool.timeout != 0 && (now - timeout->last_used) >= pool.timeout)
    return MP_clear(conn);

  if (conn->flags & CF_CANCEL || utimeout(conn) < 0)
    return MP_clear(conn);
}

static inline int validate_config(ServConfig *config) {
  if (config->max_concurrent_clients == 0  // Required
      || config->max_nheaders == 0         // Required
      || config->max_nparams == 0          // Required
      || config->max_headers_size < KB     // Has to be at least 1KBs
      || config->mem_pool_size < (KB * KB) // Has to be at least 1MB
  )
    return -1;

  struct sysinfo info = {0};
  if (sysinfo(&info) < 0)
    return -1;

  size_t free_mem, iov_len, pool_size, nblocks, conns, needed_mem;

  free_mem = info.freeram * info.mem_unit;
  iov_len = (config->max_writes_per_handler + 1) * 2;
  pool_size = ALIGN_TO_PAGESIZE(config->mem_pool_size);
  nblocks = pool_size / MP_BLOCK;
  conns = config->max_concurrent_clients;
  needed_mem = 0;
  needed_mem += conns * sizeof(Conn);
  needed_mem += pool_size;
  needed_mem += conns * (sizeof(IOV) * iov_len);
  needed_mem += BITMAP_ELEMENTS(nblocks) * sizeof(uint64_t);
  needed_mem += BITMAP_ELEMENTS(conns) * sizeof(uint64_t);

  if (needed_mem >= free_mem)
    return -1;

  return 0;
}

static inline int serv_init(Server *serv) {
  int listenfd;
  pagesize = sysconf(_SC_PAGESIZE);

  if (!serv->routes || !serv->port || validate_config(&serv->config) < 0)
    return -1;

  config = serv->config;
  serv->nroutes = get_nroutes(serv->routes);

  pool.timeout = serv->timeout;
  if (MP_init(BYTES_TO_PAGES(config.mem_pool_size)) < 0         // Initialize the pool
      || io_uring_queue_init(IOURING_QUEUE_LIMIT, &ring, 0) < 0 // Initialize io_uring
      || (listenfd = tcp_listen(serv->port)) < 0                // Create the sever socket
  )
    return -1;

  return umaccept(listenfd);
}

static inline int serv_listen(Server *serv) {
  if (serv_init(serv) < 0)
    return -1;

  while (true) {
    struct io_uring_cqe *cqe;
    if (io_uring_wait_cqe(&ring, &cqe) < 0)
      continue;

    if (!cqe->user_data) {
      io_uring_cqe_seen(&ring, cqe);
      continue;
    }

    int   res = cqe->res;
    Conn *conn = NULL;
    if (cqe->user_data > 100) {
      uint8_t *op = (uint8_t *)cqe->user_data;
      if (*op == TIMEOUT) {
        handle_timeout((Conntimeout *)cqe->user_data);
        io_uring_cqe_seen(&ring, cqe);
        continue;
      }
      conn = (Conn *)cqe->user_data;
      current_conn = conn;
    }

    if (res > 0) {
      if (cqe->user_data == ACCEPT) {
        new_conn(res);
      } else if (conn && conn->fd != -1) {
        conn->cqe_count--;

        if (conn->flags & CF_CANCEL) {
          if (conn->cqe_count == 0)
            MP_clear(conn);
          io_uring_cqe_seen(&ring, cqe);
          continue;
        }

        conn->timeout.last_used = get_time();
        switch (conn->op) {
        case FRECV:
          if (handle_frecv(serv, conn, res) < 0)
            MP_clear(conn);
          break;
        case RECV:
          if (handle_recv(serv, conn, res) < 0)
            MP_clear(conn);
          break;
        case SENDMSG:
        case SENDMSGZC:
          bool zc = (conn->op == SENDMSGZC);
          if (handle_sendmsg(conn, res, zc) < 0)
            MP_clear(conn);
          break;
        }
      }
    } else if (res == 0) {
      if (conn && conn->fd != -1) {
        conn->cqe_count--;
        switch (conn->op) {
        case SENDMSGZC:
          if (cqe->flags & IORING_CQE_F_NOTIF) {
            conn->send.zc_notifs--;
            if (conn->send.zc_notifs == 0 && handle_sendmsg_complete(conn) < 0)
              MP_clear(conn);
          }
          break;
        case FRECV:
          MP_clear(conn);
          break;
        }
      }
    } else {
      if (conn && conn->fd != -1) {
        conn->cqe_count--;
        if (res != -EAGAIN && res != -EWOULDBLOCK && res != -EINTR)
          MP_clear(conn);
        else if (ufrecv(conn) < 0)
          MP_clear(conn);
      }
    }

    io_uring_cqe_seen(&ring, cqe);
  }

  io_uring_queue_exit(&ring);
  MP_exit(&pool);
  return 0;
}

/*** Helper ***/
static inline int _HK_write(void *data, size_t size) {
  size_t nblocks;
  int    iov_index, rec_index;
  IOV   *iov, *rec;
  bool   once;

  iov_index = current_conn->send.iovlen - 1;
  rec_index = current_conn->send.reclen - 1;
  iov = &current_conn->send.iov[iov_index];
  rec = &current_conn->send.rec[rec_index];

  if ((rec->iov_len - iov->iov_len) > size) {
    memcpy(iov->iov_base + iov->iov_len, data, size);
    iov->iov_len += size;
    current_conn->send.len += size;

    return 0;
  }

  if (current_conn->send.iovlen >= config.max_writes_per_handler)
    return -1;

  once = (!config.pool_only && size >= ((pool.npages * pagesize) / 10));
  nblocks = round_to_blocks(size);
  iov_index = MP_expand(current_conn, nblocks, once);
  iov = &current_conn->send.iov[iov_index];
  memcpy(iov->iov_base, data, size);
  iov->iov_len = size;
  current_conn->send.len += size;

  return 0;
}

static inline int _HK_write_body(Request *req, size_t offset, size_t size) {
  if (size > req->body.len || offset > req->body.len)
    return -1;

  if (current_conn->send.iovlen >= config.max_writes_per_handler)
    return -1;

  IOV   *siov, *riov;
  size_t off = offset;
  size_t bytes_left = size;
  for (size_t i = 0; i < req->body.iovlen; i++) {
    if (off > req->body.iov[i].iov_len) {
      off -= req->body.iov[i].iov_len;
      continue;
    }

    riov = &req->body.iov[i];
    siov = &current_conn->send.iov[current_conn->send.iovlen++];

    riov->iov_len -= off;

    siov->iov_base = riov->iov_base + off;
    off = 0;

    if (bytes_left > riov->iov_len) {
      bytes_left -= riov->iov_len;
      siov->iov_len = riov->iov_len;
      continue;
    }

    siov->iov_len = bytes_left;
    break;
  }

  current_conn->send.len += size;
  return 0;
}

#endif
