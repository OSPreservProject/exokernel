#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <signal.h>

struct Apps {
  char name[80];
  char cmd[300];
} apps[] = {
            {"Pax -r & rm", "pax -rf lcc-3.6.pax ; rm -rf lcc-3.6"},
            {"Pax -r & rm", "pax -rf lcc-3.6.pax ; rm -rf lcc-3.6"},
            {"cp & rm", "cp -r d1 d2 ; rm -rf d2"},
            {"cp & rm", "cp -r d1 d2 ; rm -rf d2"},
	    {"Diff", "diff lcc-3.6.pax lcc-3.6.pax2 > temp; rm -f temp"},
	    {"Tsp", "tsp > temp; rm -f temp"},
	    {"Sor", "sor > temp; rm -f temp"},
};

int depth;
int already_run = 0;
int to_run;
int total;
struct timeval s;
int next_tag = 0;

char history[1024];

int explicit_wait = 0;

void handle_exit ();

void handler () {
  handle_exit ();
}

int timestamp () {
  struct timeval e;

  gettimeofday (&e, NULL);
  return (((e.tv_usec-s.tv_usec) + ((e.tv_sec-s.tv_sec)*1000000))/1000);
}

#define MAX_TRACE 1024
struct {
  int cmd;
  int pid;
  int start;
  int done;
} trace[MAX_TRACE];
int next_trace = 0;

void proc_start (int pid, int cmd) {
  assert (next_trace < MAX_TRACE);
  trace[next_trace].cmd = cmd;
  trace[next_trace].pid = pid;
  trace[next_trace].start = timestamp ();
  trace[next_trace].done = 0;
  next_trace++;
}

void proc_terminate (int pid) {
  int i;
  float start, end;

  for (i = 0; i < next_trace; i++) {
    if (trace[i].pid == pid) {
      trace[i].done = timestamp ();
      start = (float )trace[i].start/1000;
      end =   (float )trace[i].done/1000;
      printf("%-16s%20f%20f%20f\n", apps[trace[i].cmd].name, start,
	     end, end - start);
      return;
    }
  }
  assert (0);
}

void proc_alldone () {
  int i;
  float start, end;
return;

  printf ("%s\t\tStart\t\t\tEnd\t\t\tElapsed\n\n", "Process");
  for (i = 0; i < next_trace; i++) {
    start = ((float )trace[i].start)/1000;
    end = ((float )trace[i].done)/1000;
    printf ("%s\t\t%fs\t\t%fs\t\t%fs\n", apps[trace[i].cmd].name, start,
	    end, end - start);
  }
}

#define MAX_DIRS 32
int dirs[MAX_DIRS] = {0};

int find_free_dir () {
  int i;

  if (depth > MAX_DIRS) {
    assert (0);
  }

  for (i = 0; i < depth; i++) {
    if (dirs[i] == 0) {
      dirs[i] = 1;
      return i;
    }
  }
  assert (0);
  return 0;			/* for compiler */
}

void mark_dir (int dir, int ret) {
  dirs[dir] = ret;
}

void free_dir (int pid) {
  int i;

  for (i = 0; i < depth; i++) {
    if (dirs[i] == pid) {
      dirs[i] = 0;
      return;
    }
  }
  assert (0);
}

void execute_command (int cmd) {
  int ret;
  char newdir[20];
  int dir;
  int tag = next_tag++;

  dir = find_free_dir ();
  to_run--;

  ret = fork ();
  if (ret == -1) {
    fprintf (stderr, "couldn't fork\n");
    exit (-1);
  }
  if (ret > 0) {
    mark_dir (dir, ret);
    proc_start (tag, cmd);
    return;
  }

  sprintf (newdir, "test%d", dir);

  /*  printf ("Starting %s in %s (outstanding = %d)\n", apps[cmd].name, newdir, outstanding); */
  if (chdir (newdir) < 0) {
    fprintf (stderr, "couldn't chdir to %s\n", newdir);
    exit (-1);
  }
  ret = system (apps[cmd].cmd);
  if (ret < 0) {
    fprintf (stderr, "system failed on %s\n", apps[cmd].cmd);
    exit (-1);
  }
  
  exit (tag);
}

int num_cmds () {
  return (sizeof (apps)/sizeof (apps[0]));
}

int outstanding () {
  assert (total - (already_run + to_run) <= depth);
  assert (total - (already_run + to_run) >= 0);
  return (total - (already_run + to_run));
}

void handle_exit () {
  int pid;
  int status;
  int tag;

  pid = wait (&status);
  if (pid < 0) {
    if (errno == ECHILD) {
      return;			/* already got this guy */
    }
    assert (0);			/* some unknown error */
  }
  /* someone really died so update accounting info appropriately */
  if (!WIFEXITED(status)) {
    fprintf (stderr, "process didn't exit\n");
    exit (1);
  }
  tag = WEXITSTATUS(status);
  proc_terminate (tag);
  free_dir (pid);
  already_run++;
}

void wait_for_proc () {
  handle_exit ();
}

int main (int argc, char *argv[]) {
  int seed;

  if (argc < 4) {
    printf ("usage %s seed depth total\n", argv[0]);
    return -1;
  }

  seed = atoi (argv[1]);
  srandom (seed);
  depth = atoi (argv[2]);
  if (depth < 1 || depth > 20)
    exit (1);

  total = to_run = atoi(argv[3]);

  printf("%-16s%20s%20s%20s\n\n", "Process", "Start", "End", "Elapsed");

  /*  signal (SIGCHLD, handler); */

  /* now start to randomly wait, start new procs, or just sleep */
  printf ("\n*** STARTING DEPTH %d ***\n\n", depth);
  gettimeofday (&s, NULL);
  while (to_run) {
    while (outstanding () == depth) {
      wait_for_proc ();
    }
    execute_command (random () % num_cmds ());
  }
  while (outstanding())
    wait_for_proc ();
  printf ("\n*** DONE WITH DEPTH %d ***\n\n", depth); 

  return 0;
}

