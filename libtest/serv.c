#include "hunk.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void test_handler(Request *req, ResWriter *res) {
  res->status = STATUSNOCONTENT;
}

void big_handler(Request *req, ResWriter *res) {
#define BUFFSIZE 1024 * 1024 * 1
  char buff[BUFFSIZE];
  for (size_t c = '1'; c < '2'; c++) {
    memset(buff, c, BUFFSIZE);
    HK_write(buff, BUFFSIZE);
  }
}

void echo_handler(Request *req, ResWriter *res) {
  HK_write("The request body is ", sizeof("The request body is ") - 1);
  for (size_t i = 0; i < req->body.iovlen; i++)
  {
  for (size_t i = 0; i < req->body.iovlen; i++) {
      // IOV *iov = &req->body.iov[i];
  }

  HK_write_body(req, 0, req->body.len);
}
}

void all_handler(Request *req, ResWriter *res) {
  res->status = STATUSNOTFOUND;
  HK_write("this page doesn't exist", sizeof("this page doesn't exist") - 1);
}

void quote_handler(Request *req, ResWriter *res) {
  res->status = STATUSNOTFOUND;

  int index = HK_get_header(req, "Quote");
  if (index != -1) { // Quote found
    res->status = STATUSCREATED;
    char  *quote = req->headers[index].value;
    size_t quote_len = strlen(req->headers[index].value);
    HK_write(quote, quote_len);
  }
}

void id_handler(Request *req, ResWriter *res) {
  res->status = STATUSNOCONTENT;

  int index = HK_get_param(req, "id");
  if (index != -1) { // Quote found
    res->status = STATUSCREATED;
    char buff[256];
    long id_val = atol(req->params[index].value);
    int  res_len = snprintf(buff, 256, "the id is %ld", id_val);
    HK_write(buff, res_len);
  }
}

void hello_handler(Request *req, ResWriter *res) {
  char body[] = "hello world";
  HK_write(body, sizeof(body) - 1);
}

Route routes[] = {
  {GET, "/test", test_handler, false},  {GET, "/big", big_handler, false},
  {POST, "/echo", echo_handler, true},  {GET, "/quote", quote_handler, false},
  {GET, "/id", id_handler, false},      {GET, "/hello", hello_handler, false},
  {CATCHALL, "**", all_handler, false}, {0, 0, 0, 0},
};
int main() {
  Server serv = HK_new_serv();
  serv.port = 4500;
  serv.routes = routes;
  serv.timeout = 1000; // 1s timeout
  serv.config.multi_core = true;
  serv.config.pool_only = true;
  serv.config.max_concurrent_clients = 500;
  serv.config.mem_pool_size = (4096 * 8) * serv.config.max_concurrent_clients;

  printf("Server listening on port %d...\n", serv.port);
  return HK_listen(&serv);
}