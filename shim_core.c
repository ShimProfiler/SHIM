#include "shim.h"

#define PAGESIZE (4096)
static void shim_create_hw_event(char *name, int id, shim *myshim);

//help functions
static char *copy_name(char *name)
{
  char *dst = (char *)malloc(strlen(name) + 1);
  strncpy(dst, name, strlen(name) + 1);
  return dst;
}

static void bind_processor(int cpu)
{
  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(cpu, &cpuset);
  pthread_setaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
}

char *ppid_init()
{
  char *kadr;
  int fd;

  if ((fd=open("/dev/ppid_map", O_RDWR|O_SYNC)) < 0) {
    err(1,"Can't open /dev/ppid_map");
    exit(-1);
  }

  kadr = mmap(0, PAGESIZE, PROT_READ|PROT_WRITE, MAP_SHARED| MAP_LOCKED, fd, 0);
  if (kadr == MAP_FAILED) {
    perror("mmap");
    exit(-1);
  }
  return kadr;  
}

void shim_init()
{
  //from libpfm library man page, http://perfmon2.sourceforge.net/manv4/pfm_initialize.html
  //This is the first function that a program must call otherwise the library will not function at all. 
  //This function probes the underlying hardware looking for valid PMUs event tables to activate. 
  //Multiple distinct PMU tables may be activated at the same time.The function must be called only once.
  int ret = pfm_initialize();
  if (ret != PFM_SUCCESS) {
    err(1,"pfm_initialize() is failed!");
    exit(-1);
  }
}

void shim_thread_init(shim *my, int cpuid, int nr_hw_events, const char **hw_event_names)
{
  int i;
  debug_print("init shim thread at cpu %d\n", cpuid);
  bind_processor(cpuid);  
  my->cpuid = cpuid;
  my->nr_hw_events = nr_hw_events;
  my->hw_events = (struct shim_hardware_event *)calloc(nr_hw_events, sizeof(struct shim_hardware_event));
  //  assert(my->hw_events != NULL);
  for (i=0; i<nr_hw_events; i++){
    shim_create_hw_event(hw_event_names[i], i, my);
  }
  for (i=0;i <nr_hw_events; i++){
    struct shim_hardware_event *e = my->hw_events + i;
    debug_print("updateindex event %s, fd %d, index %x\n", e->name, e->fd, e->buf->index - 1);
    e->index = e->buf->index - 1;

  }
  my->probe_other_events = NULL;
  my->probe_tags = NULL;
}

static void shim_create_hw_event(char *name, int id, shim *myshim)
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
  debug_print("SHIM %d:creat %d hardware event name:%s, fd:%d, index:%x\n",
	      myshim->cpuid,
	      id,
	      name,
	      event->fd,
	      event->index);
}


int shim_read_counters(uint64_t *buf, shim *myshim)
{
  //[0] and [1] are start timestamp and end timestamp
  int index = 2;
  int i = 0;
  //start timestamp
  buf[0] = rdtsc();
  //hardware counters
  for (i=0; i<myshim->nr_hw_events; i++){
    rdtsc();
    buf[index++] = __builtin_ia32_rdpmc(myshim->hw_events[i].index);
  }
  //call back probe_other_events is happened between getting two timestamps
  if (myshim->probe_other_events != NULL)
    index += myshim->probe_other_events(buf + index, myshim); 
  //end timestamp
  buf[1] = rdtsc();
  //call back probe_other_tags is happend after reading two timestamps
  if (myshim->probe_tags != NULL)
    index += myshim->probe_tags(buf + index, myshim); 
  return index;
}

//test whether the error is in [lowpass, highpass], for example [99,101] means acceptiong +-%1 error
int shim_trustable_sample(uint64_t *start, uint64_t *end, int lowpass, int highpass)
{  
  int cycle_begin_index = 0;
  int cycle_end_index = 1;
  uint64_t cycle_begin_diff = end[cycle_begin_index] - start[cycle_begin_index];
  uint64_t cycle_end_diff = end[cycle_end_index] - start[cycle_end_index];
  int cpc = (cycle_end_diff * 100 ) / cycle_begin_diff;
  if (cpc < lowpass || cpc > highpass){
    //    debug_print("cpc is %d\n", cpc);
    return 0;
  }
  return 1;
}
