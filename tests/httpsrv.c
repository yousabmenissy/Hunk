#include "hunk.h"
// #include "types.h"
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

void quote_handler(Request *req, ResWriter *res) {
  HK_set_header(res, (Header){"name", "Lincoln (2012)"});
  HK_set_header(res, (Header){"Quote", "Thunder forth, God of war!"});
}

void all_handler(Request *req, ResWriter *res) {
  res->status = STATUSNOTFOUND;
  HK_write("this page doesn't exist", sizeof("this page doesn't exist") - 1);
}

// void quote_handler(Request *req, ResWriter *res) {
//   res->status = STATUSNOTFOUND;

//   int index = HK_get_header(req, "Quote");
//   if (index != -1) { // Header found
//     res->status = STATUSCREATED;

//     Header *header = &req->headers[index];
//     HK_write(header->value, header->value_len);
//   }
// }

void hello_handler(Request *req, ResWriter *res) {
  int index = HK_get_header(req, "Name");
  if (index == -1)
    return (void)HK_write("hello world!", sizeof("hello world!") - 1);

  HK_write("hello ", sizeof("hello ") - 1);
  Header *header = &req->headers[index];
  HK_write(header->value, header->value_len);
}

// void id_handler(Request *req, ResWriter *res) {
//   res->status = STATUSNOCONTENT;

//   int index = HK_get_param(req, "id");
//   if (index != -1) { // Param found
//     res->status = STATUSCREATED;

//     Param *param = &req->params[index];
//     HK_write("the id is ", sizeof("the id is ") - 1);
//     HK_write(param->value, param->value_len);
//   }
// }

void id_handler(Request *req, ResWriter *res) {
  int index = HK_get_param(req, "id");
  if (index == -1) {
    res->status = STATUSNOTFOUND; // 404 Status code
    return;
  }

  Param *param = &req->params[index];
  long   idnum = atol(param->value);
  idnum *= idnum;
  char buff[64];
  int  res_len = snprintf(buff, 64, "%s squared is %ld\n", param->value, idnum);
  HK_write(buff, res_len);
}

void echo_handler(Request *req, ResWriter *res) {
  HK_write("The request body is ", sizeof("The request body is ") - 1);
  for (size_t i = 0; i < req->body.iovlen; i++) {
    struct iovec *iov = &req->body.iov[i];
    HK_write(iov->iov_base, iov->iov_len);
  }
}

void echo_body_handler(Request *req, ResWriter *res) {
  HK_write("The request body is ", sizeof("The request body is ") - 1);
  HK_write_body(req, 0, req->body.len);
}

Route routes[] = {
  {GET, "/test", test_handler, true},    {GET, "/big", big_handler, false},
  {POST, "/echo", echo_handler, true},   {GET, "/quote", quote_handler, false},
  {GET, "/hello", hello_handler, false}, {GET, "/id", id_handler, false},
  {CATCHALL, "**", all_handler, false},  {0, 0, 0, 0},
};
int main() {
  Server serv = HK_new_serv();
  serv.port = 4500;
  serv.routes = routes;
  serv.timeout = 10000;
  // serv.config.multi_core = true;
  // serv.config.pool_only = true;
  // serv.config.max_concurrent_clients = 500;
  // serv.config.mem_pool_size = (4096 * 8) * serv.config.max_concurrent_clients;

  printf("Server listening on port %d...\n", serv.port);
  return HK_listen(&serv);
}