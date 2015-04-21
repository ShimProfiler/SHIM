#define _GNU_SOURCE             /* See feature_test_macros(7) */
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

struct shim_hardware_event {
  int index;
  int fd;
  struct perf_event_attr perf_attr;
  struct perf_event_mmap_page *buf;
  char * name;
};

typedef struct shim_worker_struct shim;

struct shim_worker_struct{
  int cpuid;
  int nr_hw_events;
  struct shim_hardware_event *hw_events;
  int (*probe_other_events)(uint64_t *buf, shim *myshim);
  int (*probe_tags)(uint64_t *buf, shim * myshim);  
};

#define DEBUG 1

#define debug_print(fmt, ...) \
            do { if (DEBUG) fprintf(stderr, fmt, __VA_ARGS__); } while (0)


static void __inline__ relax_cpu()
{
  __asm__ volatile("rep; nop\n\t"::: "memory");
}

static uint64_t __inline__ rdtsc(void)
{
  unsigned int tickl, tickh;
  __asm__ __volatile__("rdtscp":"=a"(tickl),"=d"(tickh)::"%ecx");
  return ((uint64_t)tickh << 32)|tickl;
}

static int __inline__ get_cpuid()
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

static shim __inline__ *get_myshim(shim *shims)
{
  int cpuid = get_cpuid();
  shim *my = shims + cpuid;
  return my;
}

void shim_init();
char *ppid_init();
void shim_thread_init(shim *myshim, int cpuid, int nr_hw_events, const char **hw_event_names);
int shim_read_counters(uint64_t *buf, shim *myshim);
int shim_trustable_sample(uint64_t *start, uint64_t *end, int lowpass, int highpass);
