#define PAGESIZE (4096)

struct shim_hardware_event {
  char * name;
  int fd;
  int index;
  struct perf_event_attr perf_attr;
  struct perf_event_mmap_page *buf;
};

struct shim_software_event {
  uint64_t *source;
  char *name;  
}

struct moma_shim_struct{
  int cpuid;
  volatile int flag;  
  int nr_hw_events;
  int nr_sw_events;
  int target_pid;
  int nr_loops;
  struct shim_hardware_event *hw_events;
  int (*probe_hw_events)(uint64_t *buf, shim * myshim);  
  struct shim_software_event  *sw_events;
  int (*probe_sw_events)(uint64_t *buf, shim * myshim);  
  uint64_t nr_samples;
  uint64_t nr_bad_samples;
  uint64_t *begin_counters;
  uint64_t *end_counters;
};

typedef struct moma_shim_struct shim;

//global ppid map, exposed by /dev/ppid_map
extern char * pid_signal_map;
extern shim * shims;

#define DEBUG
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
