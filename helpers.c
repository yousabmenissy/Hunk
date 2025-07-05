#include "serv.h"

/*
 * Write the response content
 */
int HK_write(void *data, size_t size) {
  return _HK_write(data, size);
}

/*
 * Similar to HK_write but uses the request content as the data source
 */
int HK_write_body(Request *req, size_t offset, size_t size) {
  return _HK_write_body(req, offset, size);
}

int HK_set_header(ResWriter *res, Header header) {
  if ((!header.key || header.key[0] == '\0') || (!header.value || header.value[0] == '\0'))
    return -1;
  res->headers[res->nheaders++] = header;
  return 0;
}

/*
 * Return the index of the header if it exists or -1 if it does not exist
 */
int HK_get_header(const Request *req, const char *key) {
  return _get_pair((Pair *)req->headers, req->headers_count, key);
}

/*
 * Return the index of the URL parameter if it exists or -1 if it does not exist
 */
int HK_get_param(const Request *req, const char *key) {
  return _get_pair((Pair *)req->params, req->params_count, key);
}

/*
 * Returns a zero-initialized Server with default configs
 */
Server HK_new_serv() {
  Server serv = {0};
  _serv_default_config(&serv.config);
  return serv;
}
