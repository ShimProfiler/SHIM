#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <perfmon/pfmlib_perf_event.h>

int DEBUG = 1;

#define debug_printf(fmt, ...) \
  do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)

int create_counter(const char *name, struct perf_event_attr *attr, int cpu, int task)
{
  //  int ret = pfm_get_perf_event_encoding(name, PFM_PLM3, attr, NULL, NULL);
  pfm_perf_encode_arg_t raw;
  memset(&raw, 0, sizeof(pfm_perf_encode_arg_t));
  memset(attr, 0, sizeof(struct perf_event_attr));
  raw.attr = attr;
  raw.size = sizeof(pfm_perf_encode_arg_t);
  int ret = pfm_get_os_event_encoding(name, PFM_PLM3, PFM_OS_PERF_EVENT, &raw);
  if (ret != PFM_SUCCESS) {
    errx(1, "error when translating name to attr '%s': %s\n", name, pfm_strerror(ret));
  }

  attr->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
  attr->disabled = 1;
  attr->inherit = 1;

  int fd = perf_event_open(attr, task, cpu, -1, 0);
  if (fd == -1) {
    err(1, "error in perf_event_open for event %s on task %d on cpu %d", name, task, cpu);
  }

  debug_printf("create_counter: %s, on cpu %d, on task %d as fd %d\n", name, cpu, task, fd);

  return fd;
}

int main(int argc, char* argv[]) {
  int nr_cpus = 1;
  int nr_events = 1;
  char *events;

  struct perf_event_attr *attrs;
  char **names;
  int *fds;

  int percpu = 0;
  int pincpu = 0;

  char *event_list = NULL;
  int len_list = 100;


  if (argc <= 2)
    printf( "perf_event_launcher [COMMAND] EVENT1,EVENT2,...,EVENTN program ...\n"
	    "Default COMMAND is creating counters for the program process\n"
	    "commands are:\n"
	    "  percpu: create counters for each CPU for the program process\n"
	    "  pincpu: create counters on each CPU\n");


  //process COMMAND
  if (strcmp(argv[1],"percpu") == 0){
    events = argv[2];
    percpu = 1;
  }
  else if (strcmp(argv[1],"pincpu") == 0){
    percpu = 1;
    pincpu = 1;
    events = argv[2];
  }
  else {
    events = argv[1];
  }


  nr_cpus = get_nprocs();

  for (char *c = events; *c!='\0'; c++)
    if (*c == ',')
      nr_events++;

  debug_printf("Setup %d events on %d cpus: %s\n ", nr_events,nr_cpus,events);

  int ret = pfm_initialize();
  if (ret != PFM_SUCCESS)
    errx(1, "Cannot initialize library: %s", pfm_strerror(ret));

  /*allocate attrs,etc. based on nr_events*/
  attrs = (struct perf_event_attr *)calloc(nr_events,sizeof(struct perf_event_attr));
  if (attrs == NULL)
    errx(1, "Cannot allocate space for attrs");

  names = (char **)malloc(sizeof(char*) * nr_events);
  if (names == NULL)
    errx(1, "Cannot allocate space for event_name");

  fds = (int *)malloc(sizeof(int) * nr_events * nr_cpus);
  if (fds == NULL)
    errx(1, "Cannot allocate space for fds");

  for(int i=0;i<nr_events*nr_cpus;i++)
    fds[i] = -1;


  /*names point to event names*/
  char *s = events;
  names[0] = events;
  for (int i=0; *s!='\0'; s++){
    len_list++;
    if (*s == ','){
      *s = '\0';
      debug_printf("Event %d:%s\n",i,names[i]);
      names[++i] = s+1;
    }
  }
  debug_printf("Event %d:%s\n",nr_events-1,names[nr_events-1]);

  for (int i=0; i < nr_events; i++){
    int ret;
    char buf[100];
    if (percpu){
      for (int cpu=0; cpu < nr_cpus; cpu++){
	int taskflag = 0;
	if (pincpu)
	  taskflag = -1;
	ret = create_counter(names[i], attrs+i, cpu, taskflag);
	len_list += sprintf(buf,"%d,",ret);
	fds[i*nr_cpus + cpu] = ret;
      }
    }
    else {
      ret = create_counter(names[i], &attrs[i], -1, 0);
      len_list += sprintf(buf,"%d,",ret);
      fds[i*nr_cpus] = ret;
    }
  }


  /*Set PERF_EVENT_NAMES as EVENT1,fd0,fd1,...fdn;EVENT2,fd0,fd1,....*/

  event_list = (char *)malloc(len_list);
  if (event_list == NULL)
    errx(1, "Cannot allocate %d space for event_list\n",len_list);

  char *ptr = event_list;
  for (int i=0; i < nr_events; i++){
    int n = sprintf(ptr, "%s,", names[i]);
    ptr += n;
    for (int j=0; j < nr_cpus; j++){
      int index = i *nr_cpus + j;
      if (fds[index] != -1){
	int n = sprintf(ptr, "%d,", fds[index]);
	ptr += n;
      }
    }
    ptr[-1] = ';';
  }

  ptr[-1] = '\0';

  debug_printf("Event_list:%s\n",event_list);
  setenv("PERF_EVENT_NAMES", event_list, 1);

  if (percpu)
    execvp(argv[3], argv + 3);
  else
    execvp(argv[2], argv + 2);
  return 0;
}
