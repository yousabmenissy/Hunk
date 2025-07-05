#include "serv.h"
#include <sched.h>
#include <sys/prctl.h>
#include <sys/sysinfo.h>
#include <sys/wait.h>

int HK_listen(Server *serv) {
  if (!serv->config.multi_core)
    return serv_listen(serv);

  int nprocs = get_nprocs();
  for (int i = 0; i < nprocs; i++) {
    pid_t pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    } else if (pid == 0) {
      cpu_set_t cpuset;
      CPU_ZERO(&cpuset);
      CPU_SET(i, &cpuset);
      if (sched_setaffinity(0, sizeof(cpu_set_t), &cpuset) < 0)
        return -1;
      prctl(PR_SET_PDEATHSIG, SIGTERM);

      return serv_listen(serv);
    } else {
      fprintf(stderr, "process %d running\n", pid);
    }
  }
  wait(NULL);
  return 0;
}
