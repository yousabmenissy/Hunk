# Hunk

A fast and concurrent Linux-only HTTP server library written in C.

## Table of Contents

- [Installation](#installation)
- [Usage](#usage)
- [License](#license)

## Installation

First clone the repo and run `make`

```sh
git clone https://github.com/yousabmenissy/hunk.git
cd hunk
make
```

This will compile the library into a file named libhunk.a

You can now use it by copying libhunk.a and hunk.h into you code base.

Remember to include the header in your main file

```c
#include "hunk.h"

int main() {
    // TODO
}
```

and use the '-l' and '-L' flags in your build command

```sh
gcc main.c -o main -L. -lhunk
```

## Usage

### Hello world

```c
#include "hunk.h"

void hello_handler(Request *req, ResWriter *res) { 
    HK_write("hello world", sizeof("hello world")-1);
 }

Route routes[] = {
  {GET, "/hello", hello_handler, true},
  {0, 0, 0, 0},
};

int main() {
  Server serv = HK_new_serv();
  serv.port = 4500;
  serv.routes = routes;
  
  return HK_listen(&serv);
}
```

We define and initialize the server

```c
  Server serv = HK_new_serv();
```

then we set the port and routes fields in the Server struct

```c
  serv.port = 4500;
  serv.routes = routes;
```

The routes has to be an array of Route structs. The Server.routes field should be a pointer to the first route in the array.
We defined the routes as a simple static array where the last element is null to indicate the end.

```c
Route routes[] = {
  {GET, "/hello", hello_handler, false},
  {0, 0, 0, 0},
};
```

The Route struct is defined like this

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

The handler function that will run for matching requests is defined like this

```c
void hello_handler(Request *req, ResWriter *res) { 
    HK_write("hello world", sizeof("hello world")-1);
 }
```

The handler write the response content with HK_write(void *data, size_t size).

Finally we run the server with HK_listen(Server *serv)

```c
  return HK_listen(&serv);
```

### Handlers

Handlers are functions which are meant to *handle* the incoming the request. the Handler type is defined like this

```c
typedef void (*Handler)(Request *req, ResWriter *resWriter);
```

The Request argument is a struct pointer filled out by the server to contain all the request information. like method, path, port, headers, URL parameters, content and so on.

The ResWriter argument on the other hand is used by the handler to set the response information. mainly the status code and the headers.

For example, this handler will look for a header called "Quote" in the request and include it in the response. also the handelr set the status code to "204 No Content" by default and change it to "201 Created" if the header is present

```c
void quote_handler(Request *req, ResWriter *res) {
  res->status = STATUSNOCONTENT;

  int index = HK_get_header(req, "Quote");
  if (index != -1) { // Quote found
    res->status = STATUSCREATED;
    HK_set_header(res, req->headers[index]);
  }
}
```

since the header key and value are just regular strings, (char *). we can use it in the response content if we want.

```c
void quote_handler(Request *req, ResWriter *res) {
  res->status = STATUSNOCONTENT;

  int index = HK_get_header(req, "Quote");
  if (index != -1) { // Header found
    res->status = STATUSCREATED;

    Header *header = &req->headers[index];
    HK_write(header->value, header->value_len);
  }
}
```

Now the reponse content will be the value of the "Quote" header in the request, or an empty 202 response. Notice that the Header struct also has the length of the key and value, so you don't need to call strlen.

The Request struct also contain the URL params from the request URL. It works just like the headers where it's an array of key and value pairs.

```c
void id_handler(Request *req, ResWriter *res) {
  res->status = STATUSNOCONTENT;

  int index = HK_get_param(req, "id");
  if (index != -1) { // Param found
    res->status = STATUSCREATED;

    Param *param = &req->params[index];
    HK_write("the id is ", sizeof("the id is ") - 1);
    HK_write(param->value, param->value_len);
  }
}
```

We use the HK_get_param function to find the URL param we want. then we write the response "the id is " followed by the id value.

The data in the 2 HK_write calls above are simply concatinated into a single buffer. So there is no heavy IO syscalls taking place here, you can make multiple HK_write calls without hurting performance.

The Request parameter provide the request content in the field Request.body.

```c
void echo_handler(Request *req, ResWriter *res) {
  HK_write("The request body is ", sizeof("The request body is ") - 1);
  for (size_t i = 0; i < req->body.iovlen; i++) {
      struct iovec *iov = &req->body.iov[i];
      HK_write(iov->iov_base, iov->iov_len);
  }
}
```

The echo_handler return a response containing "The request body is " followed by the request content.

The data is provided in an array of iovec structs, (struct iovec *). That's why we use a for loop to write the data in each iov object. However this is ugly and inefficient so we should use another function for this purpose, HK_write_body(Request \*req, size_t offset, size_t size).

```c
void echo_handler(Request *req, ResWriter *res) {
  HK_write("The request body is ", sizeof("The request body is ") - 1);
  HK_write_body(req, 0, req->body.len);
}
```

We replaced the for loop with the HK_write_body. Which acts just like HK_write but use the request content as input, in addition to being faster by using the request content in place. No copying or additional memory used.

Always prefer HK_write_body when possible.

Bear in mind you can just leave the handler empty if you want. The status code is "200 OK" by default.

```c
void empty_handler(Request *req, ResWriter *res) {}
```

The response from empty handlers will look like this

```http
HTTP/1.1 200 OK\r\n\r\n
```

Everything is optional inside the handler. No boilerPlate.

### Routes

Routing in Hunk is intentionally made very simple. All we need to do is define an array of Route structs and pass it to the server.

```c
Route routes[] = {
  {GET, "/hello", hello_handler, false},
  {GET, "/quote", quote_handler, false},
  {POST, "/echo", echo_handler, true},
  {CATCHALL, "**", all_handler, false}, // Match all requests
  {0, 0, 0, 0}, // Null terminator
};

int main() {
  Server serv = HK_new_serv();
  serv.port = 4500;
  serv.routes = routes;
  
  return HK_listen(&serv);
}
```

We define 3 routes that match requests with the specific path and method we provided, followed by a 'catchall' route which will match requests with any path or method.

The CATCHALL constant is a special value that will match all methods. Similarly for the path, "**" is a special value that will match any paths.

Since the routes are evaluated in order, the catchall route will only be reached if none of the other routes match.

the boolean field in the Route struct is a flag meant to indicate whether the handler will need the request content. If true, Hunk will read the entire request content and make it available to the handler.

In the example above only the echo_handler used the request body, so we set the uses_body flag to true.

## License

Copyright (c) 2025-present Yousab Menissy

Licensed under MIT License. See the [LICENSE](LICENSE) file for details.
