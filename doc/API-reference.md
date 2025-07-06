# Usage

## Table of Contents

- [Constants](#constants)
  - [DEF_MAX_REQ_SIZE](#def_max_req_size)
  - [DEF_MAX_HEAD_SIZE](#def_max_head_size)
  - [DEF_MAX_NHEADERS](#def_max_nheaders)
  - [DEF_MAX_URL_PARAMS](#def_max_url_params)
  - [DEF_MAX_WRITE_CALLS](#def_max_write_calls)
  - [DEF_MAX_CONNS](#def_max_conns)
  - [DEF_POOL_SIZE](#def_pool_size)
- [Types](#types)
  - [Mthod](#method)
  - [Status](#status)
  - [Header, Param, Pair](#header-param-pair)
  - [Request](#request)
  - [ResWriter](#reswriter)
  - [Handler](#handler)
  - [Route](#route)
  - [ServConfig](#servconfig)
    - [max_req_body_size](#servconfigmax_req_body_size)
    - [mem_pool_size](#servconfigmem_pool_size)
    - [max_headers_size](#servconfigmax_headers_size)
    - [max_writes_per_handler](#servconfigmax_writes_per_handler)
    - [max_nheaders](#servconfigmax_nheaders)
    - [max_nparams](#servconfigmax_nparams)
    - [max_concurrent_clients](#servconfigmax_concurrent_clients)
    - [multi_core](#servconfigmulti_core)
    - [pool_only](#servconfigpool_only)
  - [Server](#server)
- [Functions](#functions)
  - [HK_listen](#hk_listen)
  - [HK_get_header](#hk_get_header)
  - [HK_get_param](#hk_get_param)
  - [HK_write](#hk_write)
  - [HK_write_body](#hk_write_body)
  - [HK_set_header](#hk_set_header)
  
## Constants

### DEF_MAX_REQ_SIZE

```c
#define DEF_MAX_REQ_SIZE (1048576) // 1MB
```

The default value for [ServConfig.max_req_body_size](#servconfigmax_req_body_size).

### DEF_MAX_HEAD_SIZE

```c
#define DEF_MAX_HEAD_SIZE (8192) // 8KBs
```

The default value for [ServConfig.max_headers_size](#servconfigmax_headers_size).

### DEF_MAX_NHEADERS

```c
#define DEF_MAX_NHEADERS (500)
```

The default value for [ServConfig.max_nheaders](#servconfigmax_nheaders).

### DEF_MAX_URL_PARAMS

```c
#define DEF_MAX_URL_PARAMS (500)
```

The default value for [ServConfig.max_nparams](#servconfigmax_nparams).

### DEF_MAX_WRITE_CALLS

```c
#define DEF_MAX_WRITE_CALLS (7)
```

The default value for [ServConfig.max_writes_per_handler](#servconfigmax_writes_per_handler).

### DEF_MAX_CONNS

```c
#define DEF_MAX_CONNS (500)
```

The default value for [ServConfig.max_concurrent_clients](#servconfigmax_concurrent_clients).

### DEF_POOL_SIZE

```c
#define DEF_POOL_SIZE (DEF_MAX_CONNS * DEF_MAX_HEAD_SIZE)
```

The default value for [ServConfig.mem_pool_size](#servconfigmem_pool_size).

## Types

### Method

```c
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
```

enum type for the request HTTP method. CATCHALL is a special method which means "all methods". It's meant to be used in the routes difinition to indicate a route that matches all methods. Like this

```c
Route routes[] = {
    {CATCHALL, "/all",all_handler, false}, // Match all methods
  }; 
```

### Status

```c
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
}
```

Status is an enum for the response status code. Used mainly in the handler to set the response status code. They use the STATUS prefix to destinguish them.

### Header, Param, Pair

Header and Param are aliases for the same type, Pair.

```c
typedef struct Pair {
  char *key;
  char *value;

  size_t key_len;
  size_t value_len;
} Pair;

typedef Pair Header;
typedef Pair Param;
```

The Pair type contain a key and value pair, alongside the length of the key and value.

### Request

Request is a struct used in the handler to contain all the information of the incoming HTTP request.

```c
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
```

This struct will be filled out by Hunk and pass it by reference to the handler.

### ResWriter

ResWriter is a struct used in the handlers to set the reponse status and headers.

```c
typedef struct ResWriter {
  // The response status code
  Status  status;

  // Used internally to contain the total content length
  size_t  len;

  // Used internally to contain the headers count
  size_t  nheaders;

  // An empty array of headers meant to be filled out by the handler
  Header *headers;
} ResWriter;
```

We can set the status code like this

```c
void created_handler(Request *req, ResWriter *res) {
  res->status = STATUSCREATED; // Status 201
}
```

And we set headers with the function [HK_set_header(ResWriter *res, Header header)](#hk_set_header)

```c
void quote_handler(Request *req, ResWriter *res) {
  HK_set_header(res, (Header){"name", "Lincoln (2012)"});
  HK_set_header(res, (Header){"Quote", "Thunder forth, God of war!"});
}
```

In the example above we only need to set the key and value fiels in the header. No need to initialize the key_len and value_len fields in the Header struct since the ResWriter struct does not use them.

You can set them to 0 if you want to be safe

```c
void quote_handler(Request *req, ResWriter *res) {
  HK_set_header(res, (Header){"name", "Lincoln (2012)", 0, 0});
  HK_set_header(res, (Header){"Quote", "Thunder forth, God of war!", 0, 0});
}
```

### Handler

Handler is a the function difinition for the handlers.

```c
typedef void (*Handler)(Request *req, ResWriter *resWriter);
```

Handlers is where the magic happens.

each route should be associated with a handler function. They are meant to *handle* the incoming request and perform any necessary actions for each route.

For example if the /hello route should return "hello " followed by the value of the Name header or return "hello world". The program will look like this

```c
void hello_handler(Request *req, ResWriter *res) {
  int index = HK_get_header(req, "Name");
  if (index == -1)
    return (void)HK_write("hello world\n", sizeof("hello world\n") - 1);

  HK_write("hello ", sizeof("hello ") - 1);
  Header *header = &req->headers[index];
  HK_write(header->value, header->value_len);
  HK_write("\n", 1);
}

Route routes[] = {
  {GET, "/hello", hello_handler, false},
  {0, 0, 0, 0},
};

int main() {
  Server serv = HK_new_serv();
  serv.port = 4500;
  serv.routes = routes;

  return HK_listen(&serv);
}
```

In the handler above, we check for the header with the key "Name", if it's not present, index is -1,  we write "hello world!" and exit. otherwise we write "hello " followed by whatever was in the Name header.

Here's another example, let's say we want the route to use the URL parameter "id" that be a number. If it's present we will convert to int, square it, and write it back in a formatted reponse string. otherwise return an empty 404 reponse.

```c
void id_handler(Request *req, ResWriter *res) {
  int index = HK_get_param(req, "id");
  if (index == -1) {
    res->status = STATUSNOTFOUND; // 404 reponse
    return;
  }

  Param *param = &req->params[index];
  long   idnum = atol(param->value);
  idnum *= idnum;
  char buff[64];
  int  res_len = snprintf(buff, 64, "%s squared is %ld\n", param->value, idnum);
  HK_write(buff, res_len);
}
```

### Route

Route is a struct for each url path

```c
typedef struct Route {
  // The route HTTP method
  Method method;

  // The route path
  char *path;

  // The handler function used for matching requests
  Handler handler;

  // Flag for wether the handler needs the request content
  bool uses_body;
} Route;
```

It's meant to represent a route in the difinition of the server routes

```c
  Route routes[] = {
  {GET, "/hello", hello_handler, false},
  {POST, "/echo", echo_handler, true},
  {GET, "/quote", quote_handler, false},
  {CATCHALL, "**",all_handler, false}, // Match all requests
  {0, 0, 0, 0},
};
```

Routes are matched in the order they appear in the array. In the example above the route with the path "/hello" is evaluated first. If the route method and path match the method and path of the request, then hello_handler will be used to handle the request. If it's not matched the next route is evaluated for a match.

The Method parameter in the Route can be a specif method like, GET or POST, or it can it teh special method CATCHALL which matches all methods. Similarly the path can be a specif path, like "/hello" or  "/echo", or the special path "**" which matches all paths.

This route will match all requests

```c
  (Route){CATCHALL, "**",all_handler, false}; // Match all requests
```

which is why we made the last route in the example above.

The Route.uses_body field is a booleant flag which indicate whether the handler need to use the request body. If the flag is false, the request content will not be read and the handler won't be able to access it.

The sole reason this flag exist is to improve performance. Since The request content is only read if the handler needs it.

### ServConfig

ServConfig is a struct containing configuration options and limits for the server. All fields have a default value.

```c
typedef struct ServConfig {
  uint64_t max_req_body_size; // default is 1MB

  uint64_t mem_pool_size; // default is 4000KBs or 4096000 bytes

  uint32_t max_headers_size; // default is 8KB

  uint16_t max_writes_per_handler; // default is 7

  uint32_t max_concurrent_clients; // default is 500

  uint16_t max_nparams; // default is 500

  uint16_t max_nheaders; // default is 500

  bool multi_core; // default is false

  bool pool_only; // default is false
} ServConfig;
```

If used carefully These configurations can greatly improve performance.

#### ServConfig.max_req_body_size

The maximum size of the request content, default is 1MB or 1048576 bytes.

#### ServConfig.mem_pool_size

The amount of memory the memory pool will use. Should be at least 1MB. Default is 8KB * 500 or 4096000 bytes.

#### ServConfig.max_headers_size

The maximum size of the HTTP headers in bytes, for the request and the response.  Can't be 0. Default is 8KB.

#### ServConfig.max_writes_per_handler

The maximum number of times a handler can call HK_write everytime it's run. This number Should be as small as possible since it consumes memory. The memory used by this can be calculated like this

```c
  (sizeof(struct iovec) * 2) * (max_writes_per_handler + 1) * max_concurrent_clients
```

for example with default configs this will consume 32 \* 8 \* 500 or 128000 bytes.

If you don't need to call HK_write more than once, for the love of god, set this to 1.

Default is 7.

#### ServConfig.max_nheaders

The Maximum number of HTTP headers for both request and response. Can't be 0. Default is 500.

#### ServConfig.max_nparams

The Maximum number of URL parameter in the request path. Can't be 0. Default is 500.

#### ServConfig.max_concurrent_clients

The Maximum number of clients the server will handle concurrently. Can't be 0.

Hunk will create a connection pool of this size, which will consume memory per client. The memory used will be max_concurrent_clients * 256.

connections accpeted when the pool is full will be closed immeadiately. Default is 500

#### ServConfig.multi_core

This option let Hunk utilies multi-core processors and kernel load balancing to double or triple performance.

If ServConfig.multi_core = true, Hunk will create a seperate process with a listening socket for each cpu core, and let the linux kernel balance the load between them.

Since each process act as a seperate program with it's own memory, the memory use we set in the configs will be multiplied by the number of the cpu core in the machine you run it on.

For example if you configure it like this

```c
Server serv = HK_new_serv();
serv.mem_pool_size = 1024 * 1024 * 100; // 100MBs
serv.multi_core = true; 
```

the server will use 100MB * cpu_cores. If the CPU has 4 cores, it will use 400MBs. If it has 8 CPU cores, the server will use 800MBs. In addition to the memory needed by the connection pool and other required space.

#### ServConfig.pool_only

This option allow you to constrain the server memory use and improve performance by leveraging Hunk memory pool. effectively treating the pool as the sole source of memory.

If ServConfig.pool_only = true, the server will not allocate memory per client but instead only use the memory provided in the memory pool.

This option is used in conjunction with [ServConfig.mem_pool_size](#servconfigmem_pool_size). For example if we configure it like this

```c
Server serv = HK_new_serv();
serv.mem_pool_size = 1024 * 1024 * 100; // 100MBs
serv.pool_only = true;                  // zero-allocations
```

The server memory use will remain unchanging no matter the load or how long it runs for. This option is ideal when you've already dedicated an amount of memory for the server and you want to make sure the server never breaks over it.

Also, it achieves zero-allocations. Which significantly improves performance.

### Server

Server is a struct meant to contain the port, routes, and the configs of the server.

```c
typedef struct Server {
  // Server port
  uint16_t port;

  // Socket timeout in milli-second
  size_t timeout;

  // The address of the first route in the routes array
  Route *routes;

  // Used internally for the number of routes
  size_t nroutes;

  // Server configuration
  ServConfig config;
} Server;
```

We can create a new Server with default configs with HK_new_serv()

```c
int main() {
  Server serv = HK_new_serv();
}
```

and we run the server with [HK_listen(Server *serv)](#hk_listen)

```c
Route routes[] = {
  {GET, "/hello", hello_handler, false},
  {0, 0, 0, 0},
};

int main() {
  Server serv = HK_new_serv();
  serv.port = 4500;
  serv.routes = routes;

  return HK_listen(&serv);
}
```

The port and routes fields can't be left empty and the function will fail if port is 0 or routes is null.

## Functions

### HK_listen

```c
int HK_listen(Server *serv);
```

Used to start the server. Return 0 on success, -1 on failure.

```c
int main() {
  Server serv = HK_new_serv();
  serv.port = 4500;
  serv.routes = routes;

  return HK_listen(&serv);
}
```

### HK_get_header

```c
int HK_get_header(const Request *req, const char *key);
```

Used inside the handler to look for a header in the request headers. Return the header index on success, -1 on failure.

```c
void quote_handler(Request *req, ResWriter *res) {
  int index = HK_get_header(req, "Quote");
  if (index != -1)
    HK_write(req->headers[index].value, req->headers[index].value_len);
}
```

### HK_get_param

```c
int HK_get_param(const Request *req, const char *key);
```

Used inside the handler to look for a URL param in the request path. Return the param index on success, -1 on failure.

```c
void id_handler(Request *req, ResWriter *res) {
  int index = HK_get_param(req, "id");
  if (index != -1)
    HK_write(req->params[index].value, req->params[index].value_len);
}
```

### HK_write

```c
int HK_write(void *data, size_t size);
```

Used inside the handler to write data in the response content. It can be called more than once in the handler but it is controlled by the [ServConfig.max_writes_per_handler](#servconfigmax_writes_per_handler) option.

If multiple calls are made the data will be concatinated in the final response.

Return the number of bytes written on success, -1 on failure.

```c
void hello_handler(Request *req, ResWriter *res) {
  HK_write("hello world\n", sizeof("hello world\n") - 1);
}
```

```c
void hello_handler(Request *req, ResWriter *res) {
  HK_write("hello ", sizeof("hello ") - 1);
  HK_write("world", sizeof("world") - 1);
  HK_write("\n", 1);
}
```

### HK_write_body

```c
int HK_write_body(Request *req, size_t offset, size_t size);
```

Similar to [HK_write](#hk_write) but uses the request body as the data source. You can control the offset and size of body content you will send in the response.

Useful in handlers which write back some, or all, of the request content. Since the request content is already read in memory before the handler is called, this function uses the body in place without copying or using extra memory.

### HK_set_header

```c
int HK_set_header(ResWriter *res, Header header);
```

Used in the handler to set a header in the response.

```c
void lincoln_handler(Request *req, ResWriter *res) {
  HK_set_header(res, (Header){"name", "Lincoln (2012)"});
  HK_set_header(res, (Header){"Quote", "Thunder forth, God of war!"});
}
```
