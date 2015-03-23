#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <sched.h>
#include <sys/sysinfo.h>
#include <perfmon/pfmlib_perf_event.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <math.h>
#include "shim.h"

//global ppid map, exposed by /dev/ppid_map
char * pid_signal_map;
//all shim threads, indexed with cpuid()
shim * shims;


int cpuid()
{
  cpu_set_t cpuset;
  int j;
  CPU_ZERO(&cpuset);
  pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  assert(CPU_COUNT(&cpuset) == 1);
  for (j = 0; j < CPU_SETSIZE; j++)
        if (CPU_ISSET(j, &cpuset))
	  return j;
  return -1;
}

char *shim_init(nr_cpu)
{
  int fd;
  char *kadr;

  int ret = pfm_initialize();
  if (ret != PFM_SUCCESS) {
    perror("error in pfm_initialize: %s", pfm_strerror(ret));
  }

  if ((fd=open("/dev/ppid_map", O_RDWR|O_SYNC)) < 0) {
    perror("Can't open /dev/ppid_map");
    exit(-1);
  }

  kadr = mmap(0, PAGESIZE, PROT_READ|PROT_WRITE, MAP_SHARED| MAP_LOCKED, fd, 0);
  if (kadr == MAP_FAILED)	{
    perror("mmap");
    exit(-1);
  }

  pid_signal_map = (char *)kadr;

  shims = (shim *)calloc(nr_cpu,sizieof(shim));

  debug_print("init: fd %d, ppid %p, shim %p\n", fd, kadr, shims);

  return (char *)pid_signal_map;
}

static char *copy_name(char *name)
{
  char *dst = (char *)malloc(strlen(name) + 1);
  assert(event->name != NULL);
  strncpy(dst, name, strlen(name) + 1);
  return dst;
}

void create_hw_event(char *name, int id, shim *myshim)
{
  struct shim_hardware_event * event = myshim->hw_events + id;
  struct perf_event_attr *pe = &(event->perf_attr);
  int ret = pfm_get_perf_event_encoding(name, PFM_PLM3, pe, NULL, NULL);
  if (ret != PFM_SUCCESS) {
    errx(1, "error creating event %d '%s': %s\n", id, name, pfm_strerror(ret));
  }
  pe->sample_type = PERF_SAMPLE_READ;    
  event->fd = perf_event_open(pe, 0, -1, -1, 0);
  if (event->fd == -1) {
    err(1, "error in perf_event_open for event %d '%s'", id, name);
  }
  //mmap the fd to get the raw index
  event->buf = (struct perf_event_mmap_page *)mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, event->fd, 0);
  if (event->buf == MAP_FAILED) {
    err(1,"mmap on perf fd");
  }

  event->name = copy_name(name);

  event->index = event->buf->index - 1;
  debug_print("SHIM%d:creat %d hardware event name:%s, fd:%d, index:%x\n",
	      myshim->cpuid;
	      id,
	      name,
	      event->fd,
	      event->index);
}

shim *shim_thread_init(int nr_hw_events, char **hw_event_names, int nr_software_events, char **sw_event_names, uint64_t *sw_event_ptrs, void *sw_events_handler)
{
  int i;
  int cpuid = cpuid();
  debug_print("shim thread %d init\n", cpuid);
  shim *my = shims + cpuid;
  my->cpuid = cpuid;
  my->flag = 0;
  my->nr_hw_events = nr_hw_events;
  //hardware events first
  my->hw_events = (struct moma_hardware_event *)calloc(nr_hw_events, sizeof(struct moma_hardware_event));
  assert(my->hw_events != NULL);
  for (i=0; i<nr_hw_events; i++){
    create_hw_event(hw_event_names[i], i, my);
  }

  my->sw_events = (struct moma_software_event *)calloc(nr_sw_events, sizeof(struct moma_software_event));
  assert(my->sw_events != NULL);
  for (i=0; i<nr_sw_event; i++){
    my->sw_events[i].source = sw_event_ptrs[i];
    my->sw_event[i].name = copy_name(sw_event_names[i]);
    debug_print("SHIM%d: create %d software event name:%s\n",
		my->cpuid;
		my->sw_event[i].name);
  }
  my->probe_sw_events = sw_events_handler;   
  return my;
}





int read_counters(uint64_t *buf, shim *myshim)
{
  int index = 0;
  int i = 0;
  //control sampling frequency
  for (i=0;i<myshim->nr_loops;i++)
    relax_cpu();    
  //start timestamp
  buf[index++] = rdtsc();
  //hardware counters
  for (i=0; i<myshim->nr_hw_events; i++){
    rdtsc();
    buf[index++] = __builtin_ia32_rdpmc(myshim->hw_events[i].index);
  }
  //end timestamp
  buf[index++] = rdtsc();
  index += myshim->probe_sw_events(buf + index, myshim); 
  return index;
}

int trustable_sample(uint64_t *start, uint64_t *end, shim *who)
{  
  int cycle_begin_index = 0;
  int cycle_end_index = who->nr_hard_attrs + 1;
  uint64_t cycle_begin_diff = end[cycle_begin_index] - start[cycle_begin_index];
  uint64_t cycle_end_diff = end[cycle_end_index] - start[cycle_end_index];
  int cpc = (cycle_end_diff * 100 ) / cycle_begin_diff;
  if (cpc < 99 || cpc > 101)
    return 0;
  return 1;
}

