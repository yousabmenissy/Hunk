#define _GNU_SOURCE
#ifndef UTILS_H
#define UTILS_H

#include "types.h"
#include "uring.h"
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

static inline void *new_mem(size_t len) {
  void *mem = mmap(NULL, len, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (mem == MAP_FAILED)
    return NULL;

  return mem;
}

/*
 * Create and initialize a server socket
 */
static inline int tcp_listen(uint16_t port) {
  int enable;
  int listenfd;
  if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    return -1;

  int flags = fcntl(listenfd, F_GETFL, 0);
  fcntl(listenfd, F_SETFL, flags | O_NONBLOCK);
  struct sockaddr_in servaddr;
  memset(&servaddr, 0, sizeof(servaddr));
  servaddr.sin_family = AF_INET;
  servaddr.sin_addr.s_addr = INADDR_ANY;
  servaddr.sin_port = htons(port);

  enable = 1;
  if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEPORT, &enable, sizeof(enable)) < 0)
    return -1;

  if (bind(listenfd, (struct sockaddr *)&servaddr, sizeof(servaddr)) < 0)
    return -1;

  if (listen(listenfd, SOMAXCONN) < 0)
    return -1;

  return listenfd;
}

/*
 * Calculate the difference between two pointers
 */
static inline uintptr_t ptr_diff(uintptr_t p1, uintptr_t p2) {
  return (uintptr_t)((p1 > p2) ? (p1 - p2) : (p2 - p1));
}

/*
 * Count the number of routes
 */
static inline size_t get_nroutes(Route *routes) {
  Route *route;
  for (size_t i = 0;; i++) {
    route = &routes[i];
    if (!route->path && !route->handler && !route->method)
      return i;
  }
}

/*
 * Read the value of the Content-Length header
 * return -1 if the header does not exist
 */
static inline long get_content_length(char *src, char *end) {
  char *line = src;
  char *key, *colon, *value;
  if (memcmp(line, "\r\n", 2) == 0)
    line += 2;
  while (line && line < end) {
    if (memcmp(line, "\r\n", 2) == 0)
      break;

    char *line_end = memchr(line, '\r', end - line);
    if (!line_end)
      return -1;
    key = line;

    colon = memchr(key, ':', line_end - key);
    if (!colon)
      return -1;

    *colon = '\0';
    if (strcasecmp(key, "Content-Length") != 0) {
      line = line_end + 2;
      *colon = ':';
      continue;
    }
    *colon = ':';

    value = (colon + 1);
    while (*value == ' ')
      value++;

    *line_end = '\0';
    long res = atol(value);
    *line_end = '\r';

    return res;
  }
  return -1;
}

/*
 * Check wether the request has a header
 */
static inline bool have_header(char *src, char *end, const char *header_key, const char *header_value) {
  char *line = src;
  char *key, *colon, *value;
  if (memcmp(line, "\r\n", 2) == 0)
    line += 2;
  while (line && line < end) {
    if (memcmp(line, "\r\n", 2) == 0)
      break;

    key = line;
    while (*key == ' ')
      key++;
    while (*header_key == ' ')
      header_key++;

    char *line_end = memchr(line, '\r', end - line);
    if (!line_end)
      return false;

    colon = memchr(key, ':', line_end - key);
    if (!colon)
      return -1;

    *colon = '\0';
    if (strcasecmp(key, header_key) != 0) {
      line = line_end + 2;
      *colon = ':';
      continue;
    }
    *colon = ':';

    value = (colon + 1);
    while (*value == ' ')
      value++;
    while (*header_value == ' ')
      header_value++;

    *line_end = '\0';
    bool res = (strcasecmp(value, header_value) == 0);
    *line_end = '\r';

    return res;
  }

  return false;
}

/*
 * Round up the input into a multiple of 64 blocks
 */
static inline size_t round_to_blocks(size_t size) {
  size_t nblocks = BYTES_TO_BLOCKS(size);
  nblocks = CEIL_BLOCKS(nblocks);
  return nblocks;
}

/*
 * Count the digits in unsigned 64-bit int
 */
static inline int countd(uint64_t num) {
  if (num < 10ULL)
    return 1;
  if (num < 100ULL)
    return 2;
  if (num < 1000ULL)
    return 3;
  if (num < 10000ULL)
    return 4;
  if (num < 100000ULL)
    return 5;
  if (num < 1000000ULL)
    return 6;
  if (num < 10000000ULL)
    return 7;
  if (num < 100000000ULL)
    return 8;
  if (num < 1000000000ULL)
    return 9;
  if (num < 10000000000ULL)
    return 10;
  if (num < 100000000000ULL)
    return 11;
  if (num < 1000000000000ULL)
    return 12;
  if (num < 10000000000000ULL)
    return 13;
  if (num < 100000000000000ULL)
    return 14;
  if (num < 1000000000000000ULL)
    return 15;
  if (num < 10000000000000000ULL)
    return 16;
  if (num < 100000000000000000ULL)
    return 17;
  if (num < 1000000000000000000ULL)
    return 18;
  if (num < 10000000000000000000ULL)
    return 19;
  return 20;
}

/*
 * Get the time elabsed in ms
 */
static inline uint64_t get_time() {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)(ts.tv_sec) * 1000 + (ts.tv_nsec / 1000000);
}

/*
 * Return the status code string as string literal
 */
static inline char *status_str(Status status) {
  switch (status) {
  case STATUSCONTINUE:
    return "Continue";
  case STATUSSWITCHINGPROTOCOLS:
    return "Switching Protocols";
  case STATUSPROCESSING:
    return "Processing";
  case STATUSEARLYHINTS:
    return "Early Hints";
  case STATUSOK:
    return "OK";
  case STATUSCREATED:
    return "Created";
  case STATUSACCEPTED:
    return "Accepted";
  case STATUSNONAUTHORITATIVE:
    return "Non-Authoritative Information";
  case STATUSNOCONTENT:
    return "No Content";
  case STATUSRESETCONTENT:
    return "Reset Content";
  case STATUSPARTIALCONTENT:
    return "Partial Content";
  case STATUSMULTISTATUS:
    return "Multi-Status";
  case STATUSALREADYREPORTED:
    return "Already Reported";
  case STATUSIMUSED:
    return "Im Used";
  case STATUSMULTIPLECHOICES:
    return "Multiple Choices";
  case STATUSMOVEDPERMANENTLY:
    return "Moved Permanently";
  case STATUSFOUND:
    return "Found";
  case STATUSSEEOTHER:
    return "See Other";
  case STATUSNOTMODIFIED:
    return "Not Modified";
  case STATUSUSEPROXY:
    return "Use Proxy";
  case STATUSTEMPORARYREDIRECT:
    return "Temporary Redirect";
  case STATUSPERMANENTREDIRECT:
    return "Permanent Redirect";
  case STATUSBADREQUEST:
    return "Bad Request";
  case STATUSUNAUTHORIZED:
    return "Unauthorized";
  case STATUSPAYMENTREQUIRED:
    return "Payment Required";
  case STATUSFORBIDDEN:
    return "Forbidden";
  case STATUSNOTFOUND:
    return "Not Found";
  case STATUSMETHODNOTALLOWED:
    return "Method Not Allowed";
  case STATUSNOTACCEPTABLE:
    return "Not Acceptable";
  case STATUSPROXYAUTHENTICATIONREQUIRED:
    return "Proxy Authentication Required";
  case STATUSREQUESTTIMEOUT:
    return "Request Timeout";
  case STATUSCONFLICT:
    return "Conflict";
  case STATUSGONE:
    return "Gone";
  case STATUSLENGTHREQUIRED:
    return "Length Required";
  case STATUSPRECONDITIONFAILED:
    return "Precondition Failed";
  case STATUSCONTENTTOOLARGE:
    return "Content Too Large";
  case STATUSURITOOLONG:
    return "URI Too Long";
  case STATUSUNSUPPORTEDMEDIATYPE:
    return "Unsupported Media Type";
  case STATUSRANGENOTSATISFIABLE:
    return "Range Not Satisfiable";
  case STATUSEXPECTATIONFAILED:
    return "Expectation Failed";
  case STATUSMISDIRECTEDREQUEST:
    return "Misdirected Request";
  case STATUSUNPROCESSABLECONTENT:
    return "Unprocessable Content";
  case STATUSLOCKED:
    return "Locked";
  case STATUSFAILEDDEPENDENCY:
    return "Failed Dependency";
  case STATUSTOOEARLY:
    return "Too Early";
  case STATUSUPGRADEREQUIRED:
    return "Upgrade Required";
  case STATUSPRECONDITIONREQUIRED:
    return "Precondition Required";
  case STATUSTOOMANYREQUESTS:
    return "Too Many Requests";
  case STATUSREQUESTHEADERFIELDSTOOLARGE:
    return "Request Header Fields Too Large";
  case STATUSUNAVAILABLEFORLEGALREASONS:
    return "Unavailable For Legal Reasons";
  case STATUSInternalServerError:
    return "Internal Server Error";
  case STATUSNOTIMPLEMENTED:
    return "Not Implemented";
  case STATUSBADGATEWAY:
    return "Bad Gateway";
  case STATUSSERVICEUNAVAILABLE:
    return "Service Unavailable";
  case STATUSGATEWAYTIMEOUT:
    return "Gateway Timeout";
  case STATUSHTTPVERSIONNOTSUPPORTED:
    return "HTTP Version Not Supported";
  case STATUSVARIANTALSONEGOTIATES:
    return "Variant Also Negotiates";
  case STATUSINSUFFICIENTSTORAGE:
    return "Insufficient Storage";
  case STATUSLOOPDETECTED:
    return "Loop Detected";
  case STATUSNOTEXTENDED:
    return "Not Extended";
  case STATUSNETWORKAUTHENTICATIONREQUIRED:
    return "Network Authentication Required";
  default:
    return NULL;
  }
}

/*
 * Retrun the length of the HTTP status
 */
static inline size_t status_len(Status status) {
  switch (status) {
  case STATUSCONTINUE:
    return STRLEN("Continue");
  case STATUSSWITCHINGPROTOCOLS:
    return STRLEN("Switching Protocols");
  case STATUSPROCESSING:
    return STRLEN("Processing");
  case STATUSEARLYHINTS:
    return STRLEN("Early Hints");
  case STATUSOK:
    return STRLEN("OK");
  case STATUSCREATED:
    return STRLEN("Created");
  case STATUSACCEPTED:
    return STRLEN("Accepted");
  case STATUSNONAUTHORITATIVE:
    return STRLEN("Non-Authoritative Information");
  case STATUSNOCONTENT:
    return STRLEN("No Content");
  case STATUSRESETCONTENT:
    return STRLEN("Reset Content");
  case STATUSPARTIALCONTENT:
    return STRLEN("Partial Content");
  case STATUSMULTISTATUS:
    return STRLEN("Multi-Status");
  case STATUSALREADYREPORTED:
    return STRLEN("Already Reported");
  case STATUSIMUSED:
    return STRLEN("Im Used");
  case STATUSMULTIPLECHOICES:
    return STRLEN("Multiple Choices");
  case STATUSMOVEDPERMANENTLY:
    return STRLEN("Moved Permanently");
  case STATUSFOUND:
    return STRLEN("Found");
  case STATUSSEEOTHER:
    return STRLEN("See Other");
  case STATUSNOTMODIFIED:
    return STRLEN("Not Modified");
  case STATUSUSEPROXY:
    return STRLEN("Use Proxy");
  case STATUSTEMPORARYREDIRECT:
    return STRLEN("Temporary Redirect");
  case STATUSPERMANENTREDIRECT:
    return STRLEN("Permanent Redirect");
  case STATUSBADREQUEST:
    return STRLEN("Bad Request");
  case STATUSUNAUTHORIZED:
    return STRLEN("Unauthorized");
  case STATUSPAYMENTREQUIRED:
    return STRLEN("Payment Required");
  case STATUSFORBIDDEN:
    return STRLEN("Forbidden");
  case STATUSNOTFOUND:
    return STRLEN("Not Found");
  case STATUSMETHODNOTALLOWED:
    return STRLEN("Method Not Allowed");
  case STATUSNOTACCEPTABLE:
    return STRLEN("Not Acceptable");
  case STATUSPROXYAUTHENTICATIONREQUIRED:
    return STRLEN("Proxy Authentication Required");
  case STATUSREQUESTTIMEOUT:
    return STRLEN("Request Timeout");
  case STATUSCONFLICT:
    return STRLEN("Conflict");
  case STATUSGONE:
    return STRLEN("Gone");
  case STATUSLENGTHREQUIRED:
    return STRLEN("Length Required");
  case STATUSPRECONDITIONFAILED:
    return STRLEN("Precondition Failed");
  case STATUSCONTENTTOOLARGE:
    return STRLEN("Content Too Large");
  case STATUSURITOOLONG:
    return STRLEN("URI Too Long");
  case STATUSUNSUPPORTEDMEDIATYPE:
    return STRLEN("Unsupported Media Type");
  case STATUSRANGENOTSATISFIABLE:
    return STRLEN("Range Not Satisfiable");
  case STATUSEXPECTATIONFAILED:
    return STRLEN("Expectation Failed");
  case STATUSMISDIRECTEDREQUEST:
    return STRLEN("Misdirected Request");
  case STATUSUNPROCESSABLECONTENT:
    return STRLEN("Unprocessable Content");
  case STATUSLOCKED:
    return STRLEN("Locked");
  case STATUSFAILEDDEPENDENCY:
    return STRLEN("Failed Dependency");
  case STATUSTOOEARLY:
    return STRLEN("Too Early");
  case STATUSUPGRADEREQUIRED:
    return STRLEN("Upgrade Required");
  case STATUSPRECONDITIONREQUIRED:
    return STRLEN("Precondition Required");
  case STATUSTOOMANYREQUESTS:
    return STRLEN("Too Many Requests");
  case STATUSREQUESTHEADERFIELDSTOOLARGE:
    return STRLEN("Request Header Fields Too Large");
  case STATUSUNAVAILABLEFORLEGALREASONS:
    return STRLEN("Unavailable For Legal Reasons");
  case STATUSInternalServerError:
    return STRLEN("Internal Server Error");
  case STATUSNOTIMPLEMENTED:
    return STRLEN("Not Implemented");
  case STATUSBADGATEWAY:
    return STRLEN("Bad Gateway");
  case STATUSSERVICEUNAVAILABLE:
    return STRLEN("Service Unavailable");
  case STATUSGATEWAYTIMEOUT:
    return STRLEN("Gateway Timeout");
  case STATUSHTTPVERSIONNOTSUPPORTED:
    return STRLEN("HTTP Version Not Supported");
  case STATUSVARIANTALSONEGOTIATES:
    return STRLEN("Variant Also Negotiates");
  case STATUSINSUFFICIENTSTORAGE:
    return STRLEN("Insufficient Storage");
  case STATUSLOOPDETECTED:
    return STRLEN("Loop Detected");
  case STATUSNOTEXTENDED:
    return STRLEN("Not Extended");
  case STATUSNETWORKAUTHENTICATIONREQUIRED:
    return STRLEN("Network Authentication Required");
  default:
    return 0;
  }
}

/*
 * Return true if routes match
 */
static inline bool route_equ(Route *route, Request *req) {
  return (route->method == CATCHALL // The special method
          || route->method == req->method)
         && (strcmp(route->path, "**") == 0 // The special path
             || strcmp(route->path, req->path) == 0);
}

/*
 * Return the matching route index on success. -1 on failure
 */
static inline int match_route(Server *serv, Request *req) {
  for (size_t i = 0; i < serv->nroutes; i++) {
    Route route = serv->routes[i];
    if (!req->path || !route.path)
      continue;
    if (route_equ(&route, req))
      return i;
    else if (req->method == HEAD && route.method == GET && strcmp(route.path, req->path) == 0)
      return i;
  }
  return -1;
}

/*
 * Return true if the response should contain the Content-Length header
 */
static inline bool should_have_content_length(Request *req, ResWriter *res) {
  return (req->method != CONNECT              // CONNECT request
          && res->status != STATUSNOCONTENT   // 204 response
          && res->status != STATUSNOTMODIFIED // 304 response
          && res->status >= 200);             // 1xx response
}

/*
 * Format http response from the ResWriter.
 * Return response size on success, 0 on failure.
 */
static inline size_t fmt_res(Request *req, ResWriter *res, char *buffer, size_t buffer_size) {
  if (!res || !buffer || buffer_size == 0)
    return 0;

  if (!res->status)
    res->status = STATUSOK;

  size_t   res_len, total = 0;
  char    *dst = buffer;
  size_t   maxlen = buffer_size;
  bool     have_content_length = should_have_content_length(req, res);
  uint64_t body_length = res->len;

  res_len = snprintf(dst, maxlen, "HTTP/1.1 %d %s", (int)res->status, status_str(res->status));
  total += res_len;
  dst += res_len;
  maxlen -= res_len;

  for (size_t i = 0; i < res->nheaders; i++) {
    Header *header = &res->headers[i];
    res_len = snprintf(dst, maxlen, "\r\n%s: %s", header->key, header->value);
    total += res_len;
    dst += res_len;
    maxlen -= res_len;
  }

  if (have_content_length) {
    res_len = snprintf(dst, maxlen, "\r\nContent-Length: %lu", body_length);
    total += res_len, dst += res_len, maxlen -= res_len;
  }

  memcpy(dst, "\r\n\r\n", 4);
  total += 4, dst += 4, maxlen -= 4;

  return total;
}

/*
 * Send an empty HTTP response containing only the status
 */
static inline int send_empty_res(Status status) {
  Conn *conn = current_conn;

  if (!conn || conn->fd <= 0)
    return -1;

  struct io_uring_sqe *sqe = io_uring_get_sqe(&ring);
  if (!sqe)
    return -1;

  size_t res_len = STRLEN("HTTP/1.1 ")  // HTTP version
                   + 4                  // status code + space
                   + status_len(status) // status string
                   + 4;                 // \r\n\r\n

  IOV *rec = &conn->send.rec[0];
  memset(rec->iov_base, 0, res_len);
  if (snprintf((char *)rec->iov_base, res_len + 1, "HTTP/1.1 %d %s\r\n\r\n", (int)status, status_str(status))
      != (int)res_len)
    return -1;

  io_uring_prep_send(sqe, conn->fd, rec->iov_base, res_len, MSG_NOSIGNAL);
  return io_uring_submit(&ring);
}

/*
 * Read the request method from src
 */
static inline int read_method(Method *method, void *src) {
  if (memcmp(src, "GET", STRLEN("GET")) == 0)
    *method = GET;
  else if (memcmp(src, "POST", STRLEN("POST")) == 0)
    *method = POST;
  else if (memcmp(src, "HEAD", STRLEN("HEAD")) == 0)
    *method = HEAD;
  else if (memcmp(src, "PUT", STRLEN("PUT")) == 0)
    *method = PUT;
  else if (memcmp(src, "DELETE", STRLEN("DELETE")) == 0)
    *method = DELETE;
  else if (memcmp(src, "CONNECT", STRLEN("CONNECT")) == 0)
    *method = CONNECT;
  else if (memcmp(src, "OPTIONS", STRLEN("OPTIONS")) == 0)
    *method = OPTIONS;
  else if (memcmp(src, "TRACE", STRLEN("TRACE")) == 0)
    *method = TRACE;
  else if (memcmp(src, "PATCH", STRLEN("PATCH")) == 0)
    *method = PATCH;
  else
    return -1;

  return 0;
}

/*
 * Format the HTTP response and send it
 */
static inline int sendmsg_res(Request *req, ResWriter *res) {
  IOV *iov, *rec;
  rec = &current_conn->send.rec[0];
  iov = &current_conn->send.iov[0];
  memcpy(iov, rec, sizeof(IOV));

  size_t res_size = fmt_res(req, res, iov->iov_base, rec->iov_len);
  if (res_size > rec->iov_len)
    return -1;

  iov->iov_len = res_size;
  current_conn->send.len += res_size;

  return usendmsg(current_conn, 0, current_conn->send.iovlen);
}

static inline int parse_headers(char *start, char *end, Header *dstbuff, size_t maxHeaders) {
  if (!start || !end || !dstbuff || maxHeaders == 0)
    return -1;

  size_t  i, key_len, val_len;
  char   *head_start, *head_end, *key, *colon, *val;
  Header *header = {0};

  head_start = start;
  for (i = 0; head_start && head_start < end && i < maxHeaders; i++) {
    while (*head_start == '\r' || *head_start == '\n')
      head_start++;

    if (!head_start || *head_start == '\0' || head_start == end)
      break;

    while (*head_start == ' ')
      head_start++;

    key = head_start;
    colon = strchr(key, ':');
    if (!colon)
      return -1;

    key_len = colon - key;
    *colon = '\0';

    val = (colon + 1);
    while (*val == ' ')
      val++;

    head_end = strchr(val, '\r');
    if (!head_end)
      return -1;

    val_len = head_end - val;
    *head_end = '\0';

    header = &dstbuff[i];
    header->key = key;
    header->value = val;
    header->key_len = key_len;
    header->value_len = val_len;

    head_start = head_end + 1;
  }

  return i;
}

static inline int parse_params(char *start, char *end, Param *dstbuff, size_t maxParams) {
  if (!start || !end || !dstbuff || maxParams == 0)
    return -1;

  size_t i, key_len, val_len;
  char  *param_start, *param_end, *key, *equ, *val;
  Param *param = {0};

  param_start = start;
  while (*param_start == '?' || *param_start == '&')
    param_start++;

  for (i = 0; param_start && param_start < end && i < maxParams; i++) {
    if (!param_start || *param_start == '\0' || param_start == end)
      break;

    while (*param_start == ' ')
      param_start++;

    key = param_start;
    equ = strchr(key, '=');
    if (!equ)
      return -1;

    key_len = equ - key;
    *equ = '\0';

    val = equ + 1;
    while (*val == ' ')
      val++;

    param_end = strchr(val, '&');
    if (!param_end) {
      val_len = end - val;
      param_start = NULL; // The end
    } else {
      val_len = param_end - val;
      param_start = param_end + 1;
      *param_end = '\0';
    }

    param = &dstbuff[i];
    param->key = key;
    param->value = val;
    param->key_len = key_len;
    param->value_len = val_len;
  }

  return i;
}

/*
 * Fill the request struct from the raw request
 */
static inline int read_req(Request *req, char *req_buffer) {
  if (!req_buffer)
    return -1;
  /*** Method ***/
  char *sep = strchr(req_buffer, ' ');
  if (!sep)
    return -1;

  *sep = '\0';
  if (read_method(&req->method, req_buffer) < 0)
    return -1;

  /*** Path ***/
  char *fullpath = sep + 1;
  sep = strchr(fullpath, ' ');
  if (!sep)
    return -1;

  *sep = '\0';
  req->path = fullpath;

  /*** Params ***/
  char *paramStart = strchr(fullpath, '?');
  if (paramStart) {
    *paramStart++ = '\0';
    int params_count = parse_params(paramStart, sep, req->params, config.max_nparams);
    if (params_count > 0)
      req->params_count = params_count;
    else
      req->params_count = 0;
  } else {
    req->params = NULL;
    req->params_count = 0;
  }

  /*** Headers ***/
  char *headStart = strstr(sep + 1, "\r\n");
  char *headEnd = strstr(sep + 1, "\r\n\r\n");
  if (!headStart)
    return -1;

  int headers_count = parse_headers(headStart, headEnd, req->headers, config.max_nheaders);
  if (headers_count <= 0)
    return -1;
  req->headers_count = headers_count;

  /*** Host ***/
  int index = HK_get_header(req, "Host");
  if (index < 0)
    return -1;
  char *host = req->headers[index].value;
  // req->hostname = host;

  /*** Port ***/
  char *colon = strchr(host, ':');
  if (colon) {
    *colon++ = '\0';
    req->port = atoi(colon);
  } else {
    req->port = DEF_HTTP_PORT;
  }

  return 0;
}

static inline int _get_pair(Pair *pairs, size_t npairs, const char *key) {
  if (!pairs || npairs == 0)
    return -1;

  for (size_t i = 0; i < npairs; i++)
    if (pairs[i].key && pairs[i].key[0] != '\0' && strcasecmp(pairs[i].key, key) == 0)
      return i;

  return -1;
}

static inline void _serv_default_config(ServConfig *config) {
  config->max_req_body_size = DEF_MAX_REQ_SIZE;
  config->max_headers_size = DEF_MAX_HEAD_SIZE;
  config->max_writes_per_handler = DEF_MAX_WRITE_CALLS;
  config->max_nparams = DEF_MAX_URL_PARAMS;
  config->max_nheaders = DEF_MAX_NHEADERS;
  config->max_concurrent_clients = DEF_MAX_CONNS;
  config->mem_pool_size = DEF_POOL_SIZE;
  config->multi_core = false;
  config->pool_only = false;
}

#endif
