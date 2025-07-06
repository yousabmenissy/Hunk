#include "types.h"

struct io_uring ring = {0};

ServConfig config = {0};

int      pipe_in = 0, pipe_out = 0, pipe_sz = 0, nullfd = 0;
size_t   pagesize = 0;
MPool    pool = {0};
Conn    *current_conn = NULL;
Request *current_req = NULL;
