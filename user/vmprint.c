#include "kernel/types.h"
#include "kernel/sysinfo.h"
#include "user/user.h"

int main(int argc, char *argv[]) {
  if (vmprint() < 0) {
    fprintf(2, "vmprint failed\n");
    exit(1);
  }
  exit(0);
}