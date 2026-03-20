#include "kernel/types.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    fprintf(2, "Usage: trace mask command [args...]\n");
    exit(1);
  }
  int mask = atoi(argv[1]);
  if (trace(mask) < 0) { // trace system call = -1
    fprintf(2, "trace failed\n");
    exit(1);
  }
  exec(argv[2], &argv[2]);
  exit(0);
}