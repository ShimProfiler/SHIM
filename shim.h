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

#define PAGESIZE (4096)
#define MAX_COUNTERS (20)
#define HW_COUNTERS_BASE (1)

struct shim_hardware_event {
  int index;
  int fd;
  char * name;
  struct perf_event_attr perf_attr;
  struct perf_event_mmap_page *buf;
};

struct shim_software_event {
  uint64_t *source;
  char *name;
};

typedef struct shim_worker_struct shim;

struct shim_worker_struct{
  int cpuid;
  volatile int flag;  
  int nr_hw_events;
  int nr_sw_events;
  int target_pid;
  int nr_loops;
  struct shim_hardware_event *hw_events;
  struct shim_software_event  *sw_events;
  int (*probe_sw_events)(uint64_t *buf, shim * myshim);  
  uint64_t nr_samples;
  uint64_t nr_bad_samples;
  uint64_t *begin_counters;
  uint64_t *end_counters;
};



//global ppid map, exposed by /dev/ppid_map
extern char *pid_signal_map;
extern shim *shims;

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

static shim __inline__ *get_myshim()
{
  int cpuid = get_cpuid();
  shim *my = shims + cpuid;
  return my;
}

void bind_processor(int cpu);
char *shim_init(int nr_cpu);
shim *shim_thread_init(int nr_hw_events, char **hw_event_names, int nr_sw_events, char **sw_event_names, uint64_t **sw_event_ptrs, void *sw_events_handler);
void shim_create_hw_event(char *name, int id, shim *myshim);
int shim_read_counters(uint64_t *buf, shim *myshim);
int shim_trustable_sample(uint64_t *start, uint64_t *end, shim *who);
