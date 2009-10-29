#include <unistd.h>
#include <signal.h>
#include <stdio.h>

#include <assert.h>

void handler () {
  printf ("*BING*\n");
}

int main () {
  signal (SIGALRM, handler);
  assert (alarm (4) == 0);
  sleep (1);
  assert (alarm (0) < 4);
  assert (alarm (2) == 0);
  sigpause (0);
  printf ("OK\n");
  return 0;
}
