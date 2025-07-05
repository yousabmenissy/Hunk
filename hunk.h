#define _GNU_SOURCE
#ifndef HTTP_H
#define HTTP_H

#include <bits/types/struct_iovec.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEF_MAX_REQ_SIZE    (1048576) // Default maximum length for request content
#define DEF_MAX_HEAD_SIZE   (8192)    // Default maximum size of the request and response headers
#define DEF_MAX_NHEADERS    (500)     // Default maximum number of http request headers
#define DEF_MAX_URL_PARAMS  (500)     // Default maximum number of url Parameter
#define DEF_MAX_WRITE_CALLS (7)       // Default maximum number of HK_write calls
#define DEF_MAX_CONNS       (500)     // Default maximum number of concurrent clients
#define DEF_POOL_SIZE       (DEF_MAX_CONNS * DEF_MAX_HEAD_SIZE)

typedef enum Method {
  CATCHALL,
  GET,
  HEAD,
  POST,
  PUT,
  DELETE,
  CONNECT,
  OPTIONS,
  TRACE,
  PATCH,
} Method;

typedef enum Status {
  /*** 1xx Informational responses ***/
  STATUSCONTINUE = 100,
  STATUSSWITCHINGPROTOCOLS = 101,
  STATUSPROCESSING = 102,
  STATUSEARLYHINTS = 103,

  /*** 2xx Successful responses ***/
  STATUSOK = 200,
  STATUSCREATED = 201,
  STATUSACCEPTED = 202,
  STATUSNONAUTHORITATIVE = 203,
  STATUSNOCONTENT = 204,
  STATUSRESETCONTENT = 205,
  STATUSPARTIALCONTENT = 206,
  STATUSMULTISTATUS = 207,
  STATUSALREADYREPORTED = 208,
  STATUSIMUSED = 226,

  /*** 3xx Redirection responses ***/
  STATUSMULTIPLECHOICES = 300,
  STATUSMOVEDPERMANENTLY = 301,
  STATUSFOUND = 302,
  STATUSSEEOTHER = 303,
  STATUSNOTMODIFIED = 304,
  STATUSUSEPROXY = 305,
  STATUSSWITCHPROXY = 306,
  STATUSTEMPORARYREDIRECT = 307,
  STATUSPERMANENTREDIRECT = 308,

  /*** 4xx Client error responses ***/
  STATUSBADREQUEST = 400,
  STATUSUNAUTHORIZED = 401,
  STATUSPAYMENTREQUIRED = 402,
  STATUSFORBIDDEN = 403,
  STATUSNOTFOUND = 404,
  STATUSMETHODNOTALLOWED = 405,
  STATUSNOTACCEPTABLE = 406,
  STATUSPROXYAUTHENTICATIONREQUIRED = 407,
  STATUSREQUESTTIMEOUT = 408,
  STATUSCONFLICT = 409,
  STATUSGONE = 410,
  STATUSLENGTHREQUIRED = 411,
  STATUSPRECONDITIONFAILED = 412,
  STATUSCONTENTTOOLARGE = 413,
  STATUSURITOOLONG = 414,
  STATUSUNSUPPORTEDMEDIATYPE = 415,
  STATUSRANGENOTSATISFIABLE = 416,
  STATUSEXPECTATIONFAILED = 417,
  STATUSMISDIRECTEDREQUEST = 421,
  STATUSUNPROCESSABLECONTENT = 422,
  STATUSLOCKED = 423,
  STATUSFAILEDDEPENDENCY = 424,
  STATUSTOOEARLY = 425,
  STATUSUPGRADEREQUIRED = 426,
  STATUSPRECONDITIONREQUIRED = 428,
  STATUSTOOMANYREQUESTS = 429,
  STATUSREQUESTHEADERFIELDSTOOLARGE = 431,
  STATUSUNAVAILABLEFORLEGALREASONS = 451,

  /*** 5xx Server error responses ***/
  STATUSInternalServerError = 500,
  STATUSNOTIMPLEMENTED = 501,
  STATUSBADGATEWAY = 502,
  STATUSSERVICEUNAVAILABLE = 503,
  STATUSGATEWAYTIMEOUT = 504,
  STATUSHTTPVERSIONNOTSUPPORTED = 505,
  STATUSVARIANTALSONEGOTIATES = 506,
  STATUSINSUFFICIENTSTORAGE = 507,
  STATUSLOOPDETECTED = 508,
  STATUSNOTEXTENDED = 510,
  STATUSNETWORKAUTHENTICATIONREQUIRED = 511,
} Status;

typedef struct Pair {
  char *key;
  char *value;

  size_t key_len;
  size_t value_len;
} Pair;

typedef Pair Header;
typedef Pair Param;

typedef struct Request {
  // HTTP request method
  Method method;

  // Target HTTP port
  uint16_t port;

  // HTTP path
  char *path;

  // All path parameters
  Param *params;

  // The number of URL paramaters
  size_t params_count;

  // Array of Header structs
  Header *headers;

  // The number of headers
  size_t headers_count;

  // The request content
  struct {
    struct iovec *iov;

    size_t iovlen;
    size_t len;
  } body;

} Request;

typedef struct ResWriter {
  Status  status;
  size_t  len;
  size_t  nheaders;
  Header *headers;
} ResWriter;

typedef void (*Handler)(Request *req, ResWriter *resWriter);
typedef struct Route {
  // The route HTTP method
  Method method;

  // The route path
  char *path;

  // The handler function used for matching requests
  Handler handler;

  bool uses_body;
} Route;

typedef struct ServConfig {
  /*
   * Maximum size for the request content in bytes
   * If 0 the server will reject requests that contain content
   *
   * Default is 1MB or 1048576 bytes
   */
  uint64_t max_req_body_size;

  /*
   * The size of the memory pool in page aligned bytes
   * Should be at least 1MB in size or 1048576 bytes
   *
   * This value will be rounded up by the pagesize defined by the system
   *
   * Default is 8KB * 500 or 4096000 bytes
   */
  uint64_t mem_pool_size;

  /*
   * Maximum size of the request and response headers in bytes
   * Should be at least 8KB in size. which is the default value
   *
   * Default is 8KB or 8192 bytes
   */
  uint32_t max_headers_size;

  /*
   * Maximum number of HK_write calls that can be made in a handler.

   * This number Should be as small as possible since it consumes memory.
   * It consumes 32 bytes * (max_writes_per_handler + 1) * max_concurrent_clients,
   * for example with default configs this will consume 32 * 8 * 500 or 128000 bytes
   *
   * Default is 7
   */
  uint16_t max_writes_per_handler;
  /*
   * Maximum number of url parameters in a request path
   * Should be larger than 0
   *
   * Default is 500
   */
  uint16_t max_nparams;

  /*
   * Maximum number of HTTP headers in the request and response
   * Should be larger than 0
   *
   * Default is 500
   */
  uint16_t max_nheaders;

  /*
   * Maximum number of clients the server will handle concurrently
   * Should be larger than 0
   *
   * Hunk will create a connection pool of this size, which will consume memory per client.
   * for example by default it will consume 500 * 256 or 128000 bytes
   *
   * connections accepted when the pool is full will be closed immediately
   *
   * Default is 500
   */
  uint32_t max_concurrent_clients;

  /*
   * Start a seperate process for each cpu core and let the kernel distribute the load between them
   *
   * When this is enabled hunk will create seperate version of the same server
   * where each one has it's own memory and it's own config
   * which means the memory usage defined by configs will be multiplied by the cpu cores number
   *
   * Default is false
   */
  bool multi_core;

  /*
   * Do not allocate memory per client. use the pool only
   *
   * Default is false
   */
  bool pool_only;
} ServConfig;

typedef struct Server {
  // Server port
  uint16_t port;

  // Socket timeout in milli-second
  size_t timeout;

  // Used internally for the number of routes
  size_t nroutes;

  // The address of the first route in the routes array
  Route *routes;

  ServConfig config;
} Server;

int HK_listen(Server *serv);
int HK_get_header(const Request *req, const char *key);
int HK_get_param(const Request *req, const char *key);
int HK_write(void *data, size_t size);
int HK_write_body(Request *req, size_t offset, size_t size);
int HK_set_header(ResWriter *res, Header header);

Server HK_new_serv();

#endif
