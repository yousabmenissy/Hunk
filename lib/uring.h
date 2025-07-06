#define _GNU_SOURCE
#ifndef URING_H
#define URING_H

#include "pool.h"
#include "types.h"
#include <sys/poll.h>

/*
 * Prepare and submit a multi-shot accept uring op
 */
static inline int umaccept(int listenfd) {
  if (listenfd <= 0)
    return -1;

  struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return -1;

  sqe->user_data = ACCEPT;
  io_uring_prep_multishot_accept(sqe, listenfd, NULL, NULL, SOCK_NONBLOCK);
  return io_uring_submit(&ring);
}

static inline int urecv(Conn *conn, IOV *iov) {
  if (!conn || !iov || !iov->iov_base || !iov->iov_len)
    return -1;

  struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return -1;

  conn->op = RECV;
  sqe->user_data = (__u64)conn;
  sqe->rw_flags = IORING_RECVSEND_POLL_FIRST;
  io_uring_prep_recv(sqe, conn->fd, iov->iov_base, iov->iov_len, MSG_NOSIGNAL);
  int res = io_uring_submit(&ring);
  if (res < 0)
    return -1;

  return res;
}

/*
 * Similar to urecv but for the initial recv only
 */
static inline int ufrecv(Conn *conn) {
  IOV *iov, *rec;

  iov = &conn->recv.iov[0];
  rec = &conn->recv.rec[0];
  memcpy(iov, rec, sizeof(IOV));
  memset(rec->iov_base, 0, rec->iov_len);
  int res = urecv(conn, iov);
  if (res < 0)
    return -1;

  conn->op = FRECV;
  return res;
}

/*
 * Prepare and submit a sendmsg uring op
 */
static inline int usendmsg(Conn *conn, size_t iov_index, size_t nios) {
  if (!conn || conn->fd <= 0)
    return -1;

  struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return -1;

  bool zc = (conn->send.len > ZC_RES) ? true : false;
  conn->op = (zc) ? SENDMSGZC : SENDMSG;

  sqe->user_data = (__u64)conn;
  sqe->rw_flags = IORING_RECVSEND_POLL_FIRST;
  struct msghdr *msg = &conn->send.msg;
  msg->msg_iov = &conn->send.iov[iov_index];
  msg->msg_iovlen = nios;
  if (zc) {
    io_uring_prep_sendmsg_zc(sqe, conn->fd, msg, MSG_NOSIGNAL);
    conn->send.zc_notifs++;
  } else {
    io_uring_prep_sendmsg(sqe, conn->fd, msg, MSG_NOSIGNAL);
  }
  int res = io_uring_submit(&ring);
  if (res < 0)
    return -1;

  return res;
}

static inline int utimeout(Conn *conn) {
  if (!conn || conn->fd <= 0)
    return -1;

  struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return -1;

  struct __kernel_timespec ts = {0};
  ts.tv_sec = 1;
  sqe->user_data = (__u64)&conn->timeout;
  io_uring_prep_timeout(sqe, &ts, 0, 0);
  int res = io_uring_submit(&ring);
  if (res < 0)
    return -1;

  return res;
}

static inline int ucancel(Conn *conn) {
  if (!conn || conn->fd <= 0)
    return -1;

  struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return -1;
  io_uring_prep_cancel(sqe, conn, 0);

  int res = io_uring_submit(&ring);
  if (res < 0)
    return -1;

  return res;
}

#endif