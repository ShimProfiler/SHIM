#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <sched.h>
#include <jni.h>
#include <sys/sysinfo.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <time.h>
#include <assert.h>
#include <pthread.h>
#include <math.h>
#include <shim.h>

#define MAX_HW_COUNTERS (10)
#define MAX_EVENTS (MAX_HW_COUNTERS + 4)
#define INDEX_HW_COUNTERS (2)
#define CACHE_LINE_SIZE (64)

#define EXEC_STAT_OFFSET (0)
#define CMID_OFFSET (1)
#define PIDSIGNAL_OFFSET (2)
#define GC_OFFSET (3)
#define MEMBAND_OFFSET (4)
//#define YP_OFFSET (1)

#define ACCESS_ONCE(x) (*(volatile __typeof__(x) *)&(x))

#define DUMP_TO_BUFFER 1
#define DUMP_BUFFER_SIZE (10*1024*1024)

extern char *ttid_to_tr[];

//software signal
struct {
  int targetpid;
  int targettid;
  int fpOffset;
  int execStatOffset;
  int cmidOffset;
  int gcOffset;
  char *pid_signal_buf;
  unsigned int *membandsource;
}sf_signals;

struct jikesShimThread {
  shim shimthread;
  uint64_t * pidsource;
  int targetcpu;
  FILE *dumpfd;
  uint64_t begin[MAX_EVENTS];
  uint64_t end[MAX_EVENTS];
  uint64_t nr_samples;
  uint64_t nr_bad_samples;
  uint64_t nr_taken_samples;
  uint64_t nr_interesting_samples;
  int flag;
};

typedef struct jikesShimThread jshim;

jshim *jshims;

static int probe_soft_signals(uint64_t *buf, shim *myshim);
JNIEXPORT void JNICALL Java_moma_MomaThread_initShimProfiler(JNIEnv * env, jobject obj, jint nr_shim, jint fpOffset, jint execStatOffset, jint cmidOffset, jint gcOffset, jint targetTid;);
JNIEXPORT int JNICALL Java_moma_MomaThread_initShimThread(JNIEnv * env, jobject obj, jint cpuid, jobjectArray event_strings, jint targetcpu, jstring dumpfilename);
JNIEXPORT void JNICALL Java_moma_MomaThread_shimCounting(JNIEnv * env, jobject obj);


/////////////counters count number of read/write transactions on memory controler///////////
struct MCFGRecord
{
    unsigned long long baseAddress;
    unsigned short PCISegmentGroupNumber;
    unsigned char startBusNumber;
    unsigned char endBusNumber;
    char reserved[4];
};

struct MCFGHeader
{
    char signature[4];
    unsigned length;
    unsigned char revision;
    unsigned char checksum;
    char OEMID[6];
    char OEMTableID[8];
    unsigned OEMRevision;
    unsigned creatorID;
    unsigned creatorRevision;
    char reserved[8];
};

void reset_sample_counters(jshim *myjshim)
{
  myjshim->nr_samples = 0;
  myjshim->nr_bad_samples = 0;
  myjshim->nr_taken_samples = 0;
  myjshim->nr_interesting_samples = 0;
}

static uint64_t read_memcontroller_addr()
{
  uint64_t addr;
  struct MCFGRecord record;
  int fd = open("/sys/firmware/acpi/tables/MCFG", O_RDONLY);
  if (fd < 0)
    perror("can't open MCFG\n");
  int32_t result = pread(fd, (void *)&record, sizeof(struct MCFGRecord), sizeof(struct MCFGHeader));
  printf("read base address at %llx\n", record.baseAddress);
  return record.baseAddress;

}

unsigned int *get_memband_counter()
{
  int handle = open("/dev/mem", O_RDONLY);
  if (handle < 0){
    perror("can't open /dev/mem\n");
  }

  uint64_t offset = read_memcontroller_addr();
  char * mmapAddr = (char*) mmap(NULL, 4096, PROT_READ, MAP_SHARED , handle, offset);
  uint64_t imcbar = *((uint64_t *)(mmapAddr + 0x48));
  imcbar &= (~(4096-1));
  debug_print("imcbar offset is at %llx\n", (unsigned long long)imcbar);
  char * counterAddr = (char*) mmap(NULL, 0x6000, PROT_READ, MAP_SHARED, handle, imcbar);
  debug_print("mmap region at %p\n", counterAddr);
  if (counterAddr == (char *)-1){
    perror("can't mmap BAR\n");
  }

  unsigned int *imc_counters = (unsigned int *)(counterAddr + 0x5044);
  return imc_counters;
}

////////////////////////////////
static void dump_header_string(jshim *myjshim)
{
  shim *myshim = (shim *)myjshim;
  FILE * dumpfd = myjshim->dumpfd;
  time_t curtime;
  struct tm *loctime;
  int i;
  char tempbuf[32]; 
  curtime = time(NULL);
  loctime = localtime(&curtime);
  i = sprintf(tempbuf, "%s", asctime(loctime));  
  tempbuf[i-1] = 0;
  fprintf(dumpfd, "{\"header\":{\n");
  for (i=0;i<myshim->nr_hw_events;i++){
    fprintf(dumpfd, "\"hard%d\":\"%s\",\n", i,myshim->hw_events[i].name);
  }
  fprintf(dumpfd, "\"date\":\"%s\",\"pid\": %d\n}}\nOBJECTEND\n", tempbuf, sf_signals.targetpid);
}

static void dump_global_counters(jshim *myjshim)
{
  shim *myshim = (shim *)myjshim;
  char cur_freq[20];

  int fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_setspeed", O_RDWR);
  int nr_read = read(fd, cur_freq, sizeof(cur_freq));
  for (int i=0; i<nr_read; i++){
    if (cur_freq[i] == '\n')
      cur_freq[i] = 0;
  }

  if (myjshim->dumpfd != NULL){
    FILE *fd = myjshim->dumpfd;
    //dump jason format copy to dumpfd
    //"SHIM.RDTSC":val, "SHIM.LOOP":val,
    fprintf(fd, "\"SHIM.RDTSC\":%lld, \"SHIM.LOOP\":%lld, \"SHIM.CPUFREQ\":%s,\n",
	    myjshim->end[0] - myjshim->begin[0], myjshim->nr_samples, cur_freq);
    for (int i=0;i<myshim->nr_hw_events;i++){
      fprintf(fd, "\"SHIM.%s\":%lld,\n",
	      myshim->hw_events[i].name,
	      myjshim->end[i+INDEX_HW_COUNTERS] - myjshim->begin[i+INDEX_HW_COUNTERS]);
    }
  }
  //report to stdout as the format Plotty can understand.
  printf("============================ Tabulate Statistics ============================\n");
  printf("SHIM.RDTSC\tSHIM.LOOP");
  for (int i=0;i<myshim->nr_hw_events;i++){
    printf("\tSHIM.%s",myshim->hw_events[i].name);
  }
  printf("\n");
  printf("%lld\t%lld", myjshim->end[0] - myjshim->begin[0], myjshim->nr_samples);
  for (int i=0;i<myshim->nr_hw_events;i++){
    printf("\t%lld",myjshim->end[i+INDEX_HW_COUNTERS] - myjshim->begin[i+INDEX_HW_COUNTERS]);
  }
  printf("\n");
  printf("---------------------------- End --------------------------------------------\n");  
}

int probe_memcontroller(uint64_t *buf, shim *myshim)
{
  buf[0] = *(sf_signals.membandsource);
  return 1;  
}

int gc_probe_sf_signals(uint64_t *buf, shim *myshim)
{ 
  //software tags
  uint64_t pidsignal = 0;
  //  int ypval = 0;
  int gcval = 0;
  //  unsigned int memval = 0;
  int cmidval = 0;
  int status = -1;
  int index = 0;
  jshim *myjshim = (jshim *)myshim;
  
  pidsignal = *(myjshim->pidsource);
  int tid = pidsignal & (0xffffffff);
  int pid = pidsignal >> 32;
  unsigned int offset = tid - pid;    
  if (pid == sf_signals.targetpid && offset < 512){
    //    printf("XXX\n");
    char *cr = ttid_to_tr[offset];
    if (cr != NULL){ 
      unsigned int * fp_ptr = *(unsigned int **)(cr + sf_signals.fpOffset);
      if (fp_ptr != NULL)
	cmidval = *(fp_ptr-1);
      status = *((unsigned int *)(cr + sf_signals.execStatOffset));
      gcval = *((unsigned int *)(cr + sf_signals.gcOffset));
      //      ypval = *((unsigned int *)(cr + sf_signals.cmidOffset));      
      //      memval = *(sf_signals.membandsource);
    }
  }
  buf[index++] = status;
  buf[index++] = cmidval;
  buf[index++] = tid;  
  buf[index++] = gcval;
  return index;
}

int probe_sf_signals(uint64_t *buf, shim *myshim)
{ 
  //software tags
  uint64_t pidsignal = 0;
  //  int ypval = 0;
  int gcval = 0;
  //  unsigned int memval = 0;
  int cmidval = 0;
  int status = -1;
  int index = 0;
  jshim *myjshim = (jshim *)myshim;
  
  pidsignal = *(myjshim->pidsource);
  int tid = pidsignal & (0xffffffff);
  int pid = pidsignal >> 32;
  unsigned int offset = tid - pid;    
  if (pid == sf_signals.targetpid && offset < 512){
    //    printf("XXX\n");
    char *cr = ttid_to_tr[offset];
    if (cr != NULL){ 
      unsigned int * fp_ptr = *(unsigned int **)(cr + sf_signals.fpOffset);
      if (fp_ptr != NULL)
	cmidval = *(fp_ptr-1);
      status = *((unsigned int *)(cr + sf_signals.execStatOffset));
      //      gcval = *((unsigned int *)(cr + sf_signals.gcOffset));
      //      ypval = *((unsigned int *)(cr + sf_signals.cmidOffset));      
      //      memval = *(sf_signals.membandsource);
    }
  }
  buf[index++] = status;
  buf[index++] = cmidval;
  buf[index++] = tid;  
  //  buf[index++] = gcval;
  return index;
}



JNIEXPORT void JNICALL
Java_moma_MomaThread_initShimProfiler(JNIEnv * env, jobject obj, jint nr_shim, jint fpOffset, jint execStatOffset, jint cmidOffset, jint gcOffset, jint targetTid)
{ 
  int i;
  shim_init();
  debug_print("create %d jshim threads, target thread id %d\n", nr_shim, targetTid);
  jshims = (jshim *)calloc(nr_shim, sizeof(jshim));
  if (jshims == NULL){
    err(1,"Can't create jshims");
    exit(-1);
  }
  sf_signals.pid_signal_buf = ppid_init();
  sf_signals.fpOffset = fpOffset;
  sf_signals.execStatOffset = execStatOffset;
  sf_signals.cmidOffset = cmidOffset;  
  sf_signals.gcOffset = gcOffset;
  sf_signals.targetpid = getpid();
  sf_signals.targettid = targetTid;
  sf_signals.membandsource = get_memband_counter();
  debug_print("fpOffset:%d, execOffset:%d, cmidOffset:%d, gcOffset:%d, targetpid:%d, targettid:%d\n",
	      fpOffset, execStatOffset, cmidOffset, gcOffset, sf_signals.targetpid, targetTid);
}


JNIEXPORT int JNICALL
Java_moma_MomaThread_initShimThread(JNIEnv * env, jobject obj, jint cpuid, jobjectArray event_strings, jint targetcpu, jstring dumpfilename)
{   
  const char * filename = (*env)->GetStringUTFChars(env, dumpfilename, 0);
  debug_print("jshim init at cpu %d, targetcpu %d, dump info to file %s \n", cpuid, targetcpu, filename);
  //hardware events
  shim *myshim = (shim *)(jshims + cpuid);
  jshim *myjshim = jshims + cpuid;

  int nr_events = (*env)->GetArrayLength(env,event_strings);
  const char *event_names[MAX_HW_COUNTERS];

  for (int i=0; i<nr_events; i++) {
    jstring string = (jstring) (*env)->GetObjectArrayElement(env, event_strings, i);
    event_names[i] = (*env)->GetStringUTFChars(env, string, 0);
  }    
  shim_thread_init(myshim, (int)cpuid, nr_events, event_names);

  for (int i=0; i<nr_events; i++){
    jstring string = (jstring) (*env)->GetObjectArrayElement(env, event_strings, i);
    (*env)->ReleaseStringUTFChars(env, string, event_names[i]);
  }

  //pidsource 
  myjshim->targetcpu = targetcpu;
  myjshim->pidsource = (unsigned long long *)(sf_signals.pid_signal_buf + targetcpu * CACHE_LINE_SIZE);
  myjshim->dumpfd = fopen(filename, "w+");
  dump_header_string(myjshim);
  
  (*env)->ReleaseStringUTFChars(env, dumpfilename, filename); 
  
  return (jint)(&(myjshim->flag));
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_shimCounting(JNIEnv * env, jobject obj)
{
  int cpuid = get_cpuid();
  jshim *myjshim = jshims + cpuid;
  shim *myshim = (shim *)myjshim;

  shim_read_counters(myjshim->begin, myshim);
  while (ACCESS_ONCE(myjshim->flag) != 0xdead){
    ;
  }
  myjshim->nr_samples++;
  shim_read_counters(myjshim->end, myshim);
  dump_global_counters(myjshim); 
}

struct cmid_tag{
  double ipc;
  uint64_t nr_sample;
};


JNIEXPORT void JNICALL
Java_moma_MomaThread_shimCMIDHistogram(JNIEnv * env, jobject obj, jint rate, jint maxCMID)
{
  int cpuid = get_cpuid();
  jshim *myjshim = jshims + cpuid;
  shim *myshim = (shim *)myjshim;
  FILE *dumpfd = myjshim->dumpfd;

  reset_sample_counters(myjshim);
  myshim->probe_other_events = NULL;
  myshim->probe_tags = probe_sf_signals;

  struct cmid_tag *app_hist = calloc(maxCMID, sizeof(struct cmid_tag));

  uint64_t vals[2][MAX_EVENTS];
  int last_index = 0;
  int now_index = 1;

  int step = rate;

  int soft_index_base = INDEX_HW_COUNTERS + myshim->nr_hw_events;
  
  shim_read_counters(myjshim->begin, myshim);

  shim_read_counters(vals[last_index], myshim);
  while (ACCESS_ONCE(myjshim->flag) != 0xdead){
    myjshim->nr_samples++;
    step--;
    shim_read_counters(vals[now_index], myshim);
          
    //one trustable sample
    unsigned int  nr_instructions_core = vals[now_index][INDEX_HW_COUNTERS+1]  - vals[last_index][INDEX_HW_COUNTERS+1];
    unsigned int  nr_instructions_self = vals[now_index][INDEX_HW_COUNTERS+2]  - vals[last_index][INDEX_HW_COUNTERS+2];
   
    unsigned int  nr_cycles = vals[now_index][INDEX_HW_COUNTERS]  - vals[last_index][INDEX_HW_COUNTERS];
    double ipc_core = (double)nr_instructions_core / nr_cycles;
    double ipc_self = (double)nr_instructions_self / nr_cycles;
    double ipc_app = ipc_core - ipc_self;

    int cmid = vals[now_index][soft_index_base + CMID_OFFSET];
    //    int gcflag = vals[now_index][soft_index_base + GC_OFFSET];
    int interesting_sample = shim_trustable_sample(vals[last_index], vals[now_index], 99, 101) &&
      vals[now_index][soft_index_base + EXEC_STAT_OFFSET] == 1 &&
      ipc_core <= 5 &&
      ipc_self <= ipc_core &&
      cmid > 0 &&
      cmid <= maxCMID;


    if (step <= 0){
      step = rate;
      myjshim->nr_taken_samples++;
      last_index ^= 1;
      now_index ^=1;
      if (interesting_sample){
	app_hist[cmid].ipc += ipc_app;
	app_hist[cmid].nr_sample++;
	myjshim->nr_interesting_samples++;
	//	printf("%f,%d\n", ipc, gcflag);
      } else
	myjshim->nr_bad_samples++;
    }
  }
  shim_read_counters(myjshim->end, myshim);
  uint64_t profiling_cycles = myjshim->end[0] - myjshim->begin[0];

  //report the histogram
  fprintf(dumpfd, "\n{\"cmidHistogram\":{\"rate\":%d, \"samples\":%lld, \"untrustableSamples\":%lld, \"takenSamples\":%lld, \"interestingSamples\":%lld,\n", rate, (long long)(myjshim->nr_samples), (long long)(myjshim->nr_bad_samples), (long long)(myjshim->nr_taken_samples), (long long)(myjshim->nr_interesting_samples));
  dump_global_counters(myjshim);
  fprintf(dumpfd, "\"hist\":[\n");
  int cmflag = 0;
  for (int i=0; i<maxCMID; i++){
    if (app_hist[i].ipc != 0 && app_hist[i].nr_sample/(double)(myjshim->nr_interesting_samples) >= 0.0001){
      char * cmstr = ",";
      if (cmflag == 0){
	cmstr = "";
	cmflag = 1;
      }
      fprintf(dumpfd, "%s{\"cmid\":%d, \"ipc\":%.3f,\"samples\":%lld, \"percentage\":%.4f}\n", cmstr, i, app_hist[i].ipc/app_hist[i].nr_sample, app_hist[i].nr_sample, app_hist[i].nr_sample/(double)(myjshim->nr_interesting_samples));
    }
  }
  fprintf(dumpfd, "]}}\nOBJECTEND\n");

  fflush(dumpfd);

  free(app_hist);
}

struct gc_tag{
  double ipc;
  double memband;
  uint64_t nr_sample;
};

//#define DUMP_GC_TIMELINE

JNIEXPORT void JNICALL
Java_moma_MomaThread_shimGCHistogram(JNIEnv * env, jobject obj, jint rate, jint maxCMID)
{
  int cpuid = get_cpuid();
  jshim *myjshim = jshims + cpuid;
  shim *myshim = (shim *)myjshim;
  FILE *dumpfd = myjshim->dumpfd;
  

#ifdef DUMP_GC_TIMELINE
  unsigned int *dumpbuf = NULL;
  int dumpbuf_index = 0;
  dumpbuf = calloc(DUMP_BUFFER_SIZE, sizeof(uint64_t));
#endif

  reset_sample_counters(myjshim);
  myshim->probe_other_events = probe_memcontroller;
  myshim->probe_tags = gc_probe_sf_signals;

  struct gc_tag *app_hist = calloc(maxCMID, sizeof(struct gc_tag));

  struct gc_tag **gc_hist = calloc(10, sizeof(struct gc_tag *));
  for (int i=0; i<10; i++){
    gc_hist[i] = calloc(500, sizeof(struct gc_tag));
  }

  uint64_t vals[2][MAX_EVENTS];
  int last_index = 0;
  int now_index = 1;

  int step = rate;

  int soft_index_base = INDEX_HW_COUNTERS + myshim->nr_hw_events + 1;
  
  shim_read_counters(myjshim->begin, myshim);

  shim_read_counters(vals[last_index], myshim);
  while (ACCESS_ONCE(myjshim->flag) != 0xdead){
    myjshim->nr_samples++;
    step--;
    shim_read_counters(vals[now_index], myshim);
          
    //one trustable sample
    unsigned int  nr_cycles = vals[now_index][INDEX_HW_COUNTERS]  - vals[last_index][INDEX_HW_COUNTERS];
    unsigned int  nr_instructions_core = vals[now_index][INDEX_HW_COUNTERS+1]  - vals[last_index][INDEX_HW_COUNTERS+1];
    unsigned int  nr_instructions_self = vals[now_index][INDEX_HW_COUNTERS+2]  - vals[last_index][INDEX_HW_COUNTERS+2];
    unsigned int nr_memrequest = vals[now_index][INDEX_HW_COUNTERS+3]  - vals[last_index][INDEX_HW_COUNTERS+3];
   
    double ipc_core = (double)nr_instructions_core / nr_cycles;
    double ipc_self = (double)nr_instructions_self / nr_cycles;
    double ipc_app = ipc_core - ipc_self;
    double mem_band = (double)nr_memrequest / nr_cycles;

    int cmid = vals[now_index][soft_index_base + CMID_OFFSET];
    int gcflag = vals[now_index][soft_index_base + GC_OFFSET];
    int tid = vals[now_index][soft_index_base + PIDSIGNAL_OFFSET];
    
    int interesting_sample = shim_trustable_sample(vals[last_index], vals[now_index], 99, 101) &&
      vals[now_index][soft_index_base + EXEC_STAT_OFFSET] == 1 &&
      tid == sf_signals.targettid &&
      ipc_core <= 5 &&
      ipc_self <= ipc_core &&
      cmid > 0 &&
      cmid <= maxCMID;


    if (step <= 0){
      step = rate;
      myjshim->nr_taken_samples++;
      last_index ^= 1;
      now_index ^=1;
      if (interesting_sample){
	//	hist[cmid].ipc += ipc_core;
	//	self_hist[cmid].ipc += ipc_self;
	app_hist[cmid].ipc += ipc_app;
	app_hist[cmid].memband += mem_band;
	app_hist[cmid].nr_sample += 1;
	int gc_hist_index = (int)(ipc_app*100);
	//	printf("%d,%d,%d\n", cmid, gc_hist_index, gcflag);
	gc_hist[gcflag][gc_hist_index].ipc += ipc_app;
	gc_hist[gcflag][gc_hist_index].memband += mem_band;
	gc_hist[gcflag][gc_hist_index].nr_sample +=1;
	myjshim->nr_interesting_samples++;

#ifdef DUMP_GC_TIMELINE
	if (dumpbuf_index + 4 < DUMP_BUFFER_SIZE){
	  dumpbuf[dumpbuf_index++] = vals[now_index][0];
	  dumpbuf[dumpbuf_index++] = (int)(ipc_app*1000);
	  dumpbuf[dumpbuf_index++] = cmid;
	  dumpbuf[dumpbuf_index++] = gcflag;
	  dumpbuf[dumpbuf_index++] = (int)(mem_band*1000);
	}
#endif
      } else
	myjshim->nr_bad_samples++;
    }
  }
  shim_read_counters(myjshim->end, myshim);
  uint64_t profiling_cycles = myjshim->end[0] - myjshim->begin[0];

  //report the histogram
  fprintf(dumpfd, "\n{\"cmidHistogram\":{\"rate\":%d, \"samples\":%lld, \"untrustableSamples\":%lld, \"takenSamples\":%lld, \"interestingSamples\":%lld,\n", rate, (long long)(myjshim->nr_samples), (long long)(myjshim->nr_bad_samples), (long long)(myjshim->nr_taken_samples), (long long)(myjshim->nr_interesting_samples));
  dump_global_counters(myjshim);
  fprintf(dumpfd, "\"hist\":[\n");
  int cmflag = 0;
  for (int i=0; i<maxCMID; i++){
    if (app_hist[i].ipc != 0 && app_hist[i].nr_sample/(double)(myjshim->nr_interesting_samples) >= 0.0001){
      char * cmstr = ",";
      if (cmflag == 0){
	cmstr = "";
	cmflag = 1;
      }
      fprintf(dumpfd, "%s{\"cmid\":%d, \"ipc\":%.3f, \"memband\":%.5f, \"samples\":%lld, \"percentage\":%.4f}\n", cmstr, i, app_hist[i].ipc/app_hist[i].nr_sample, app_hist[i].memband/app_hist[i].nr_sample,app_hist[i].nr_sample, app_hist[i].nr_sample/(double)(myjshim->nr_interesting_samples));
    }
  }
  fprintf(dumpfd, "]\n");

  for (int phase=0; phase<10; phase++){
    struct gc_tag *h = gc_hist[phase];
    double sum_ipc = 0;
    double sum_memband = 0;
    unsigned int total_samples = 0;
    for (int i=0;i<500;i++){
      total_samples += h[i].nr_sample;
      sum_ipc += h[i].ipc;
      sum_memband += h[i].memband;
    }
    if (total_samples == 0)
      continue;
    fprintf(dumpfd, ",\"gcphase%d\":{\"samples\":%d, \"averageipc\":%.3f, \"averagememband\":%.5f, \"hist\":[\n", phase, total_samples, sum_ipc/total_samples, sum_memband/total_samples);

    cmflag = 0;
    for (int i=0; i<500; i++){
      if (h[i].ipc != 0){
	char * cmstr = ",";
	if (cmflag == 0){
	  cmstr = "";
	  cmflag = 1;
	}
	fprintf(dumpfd, "%s{\"gcflag\":%d, \"ipc\":%.3f, \"memband\":%.5f, \"samples\":%lld, \"percentage\":%.4f}\n", cmstr, phase, (float)i/100, h[i].memband/h[i].nr_sample, h[i].nr_sample, h[i].nr_sample/(double)(total_samples));
      }
    }
    fprintf(dumpfd, "]}\n");
  }
  fprintf(dumpfd, "}}\nOBJECTEND\n");

#ifdef DUMP_GC_TIMELINE
  fprintf(dumpfd, "#id, timestamp, ipc, cmid, gcflag, memband\n");
  for (int i=0; i<DUMP_BUFFER_SIZE; i+=5){
    if (dumpbuf[i] != 0)
      fprintf(dumpfd, "%d, %d, %.3f, %d, %d, %d\n", i/5, dumpbuf[i], (float)dumpbuf[i+1]/1000, dumpbuf[i+2], dumpbuf[i+3], dumpbuf[i+4]);
  }
  free(dumpbuf);
#endif
    
  fflush(dumpfd);


  free(app_hist);
  for (int i=0; i<10; i++){
    free(gc_hist[i]);
  }
  free(gc_hist);
}


static inline int cpc_val(uint64_t *start, uint64_t *end)
{  
  int cycle_begin_index = 0;
  int cycle_end_index = 1;
  uint64_t cycle_begin_diff = end[cycle_begin_index] - start[cycle_begin_index];
  uint64_t cycle_end_diff = end[cycle_end_index] - start[cycle_end_index];
  int cpc = (cycle_end_diff * 100 ) / cycle_begin_diff;
  return cpc;
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_shimFidelityHistogram(JNIEnv * env, jobject obj, jint rate, jint lowpass, jint highpass)
{
  int cpuid = get_cpuid();
  jshim *myjshim = jshims + cpuid;
  shim *myshim = (shim *)myjshim;
  FILE *dumpfd = myjshim->dumpfd;

  int max_prefilter_ipc = 0;
  int max_prefilter_cpc = 0;
  
  reset_sample_counters(myjshim);
  myshim->probe_other_events = NULL;
  myshim->probe_tags = probe_sf_signals;
 
  unsigned int *hist = calloc(1000, sizeof(unsigned int));
  unsigned int *hist_self = calloc(1000, sizeof(unsigned int));
  unsigned int *hist_app = calloc(1000, sizeof(unsigned int));

  uint64_t vals[2][MAX_EVENTS];
  int last_index = 0;
  int now_index = 1;

  int step = rate;

  int soft_index_base = INDEX_HW_COUNTERS + myshim->nr_hw_events;
  
  shim_read_counters(myjshim->begin, myshim);

  shim_read_counters(vals[last_index], myshim);
  while (ACCESS_ONCE(myjshim->flag) != 0xdead){
    myjshim->nr_samples++;
    step--;
    shim_read_counters(vals[now_index], myshim);
          
    //one trustable sample
    unsigned int  nr_instructions_core = vals[now_index][INDEX_HW_COUNTERS+1]  - vals[last_index][INDEX_HW_COUNTERS+1];
    unsigned int  nr_instructions_self = vals[now_index][INDEX_HW_COUNTERS+2]  - vals[last_index][INDEX_HW_COUNTERS+2];
    unsigned int  nr_cycles = vals[now_index][INDEX_HW_COUNTERS]  - vals[last_index][INDEX_HW_COUNTERS];
    int ipc = (nr_instructions_core * 100)/nr_cycles;
    int cpc = cpc_val(vals[last_index], vals[now_index]);
    
    if (ipc > max_prefilter_ipc)
      max_prefilter_ipc = ipc;
    if (cpc > max_prefilter_cpc)
      max_prefilter_cpc = cpc;

    int ipc_self = (nr_instructions_self * 100)/nr_cycles;
    int ipc_app = ((nr_instructions_core - nr_instructions_self)*100)/nr_cycles;
    
    int interesting_sample = cpc >= lowpass && cpc<= highpass;
    If (step <= 0){
      step = rate;
      myjshim->nr_taken_samples++;
      last_index ^= 1;
      now_index ^=1;
      if (interesting_sample){
	//	printf("%d,%d,%d\n", ipc, ipc_app, ipc_self);
	hist[ipc]++;
	hist_app[ipc_app]++;
	hist_self[ipc_self]++;
	myjshim->nr_interesting_samples++;
      } else
	myjshim->nr_bad_samples++;
    }
  }
  shim_read_counters(myjshim->end, myshim);
  uint64_t profiling_cycles = myjshim->end[0] - myjshim->begin[0];

  //report the histogram
  fprintf(dumpfd, "{\"eventHistogram\":{\"rate\":%d, \"samples\":%lld, \"untrustableSamples\":%lld, \"takenSamples\":%lld, \"interestingSamples\":%lld,\n", rate, (long long)(myjshim->nr_samples), (long long)(myjshim->nr_bad_samples), (long long)(myjshim->nr_taken_samples), (long long)(myjshim->nr_interesting_samples));
  dump_global_counters(myjshim); 
  fprintf(dumpfd, "\"hist\":[\n");
  int cmflag = 0;
  for (int i=0; i<1000; i++){
    if (hist[i] != 0 || hist_app[i] != 0 || hist_self[i] != 0){
      char * cmstr = ",";
      if (cmflag == 0){
	cmstr = "";
	cmflag = 1;
      }
      fprintf(dumpfd, "%s{\"ipc\":%d, \"samples\":%d, \"percentage\":%.3f, \"samples_app\":%d, \"percentage_app\":%.3f, \"samples_self\":%d, \"percentage_self\":%.3f}\n", cmstr, i, hist[i], hist[i]/(double)(myjshim->nr_interesting_samples),hist_app[i], hist_app[i]/(double)(myjshim->nr_interesting_samples),hist_self[i], hist_self[i]/(double)(myjshim->nr_interesting_samples));
    }
  }
  fprintf(dumpfd, "]}}\nOBJECTEND\n");

  printf("max prefilter ipc: %d, maxprefilter cpc: %d\n", max_prefilter_ipc, max_prefilter_cpc);

  free(hist);
}


JNIEXPORT void JNICALL
Java_moma_MomaThread_shimEventHistogram(JNIEnv * env, jobject obj, jint rate)
{
  int cpuid = get_cpuid();
  jshim *myjshim = jshims + cpuid;
  shim *myshim = (shim *)myjshim;
  FILE *dumpfd = myjshim->dumpfd;
  
  reset_sample_counters(myjshim);
  myshim->probe_other_events = NULL;
  myshim->probe_tags = probe_sf_signals;
 
  unsigned int *hist = calloc(1000, sizeof(unsigned int));
  unsigned int *hist_self = calloc(1000, sizeof(unsigned int));
  unsigned int *hist_app = calloc(1000, sizeof(unsigned int));

  uint64_t vals[2][MAX_EVENTS];
  int last_index = 0;
  int now_index = 1;

  int step = rate;

  int soft_index_base = INDEX_HW_COUNTERS + myshim->nr_hw_events;
  
  shim_read_counters(myjshim->begin, myshim);

  shim_read_counters(vals[last_index], myshim);
  while (ACCESS_ONCE(myjshim->flag) != 0xdead){
    myjshim->nr_samples++;
    step--;
    shim_read_counters(vals[now_index], myshim);
          
    //one trustable sample
    unsigned int  nr_instructions_core = vals[now_index][INDEX_HW_COUNTERS+1]  - vals[last_index][INDEX_HW_COUNTERS+1];
    unsigned int  nr_instructions_self = vals[now_index][INDEX_HW_COUNTERS+2]  - vals[last_index][INDEX_HW_COUNTERS+2];
    unsigned int  nr_cycles = vals[now_index][INDEX_HW_COUNTERS]  - vals[last_index][INDEX_HW_COUNTERS];
    int ipc = (nr_instructions_core * 100)/nr_cycles;
    int ipc_self = (nr_instructions_self * 100)/nr_cycles;
    int ipc_app = ((nr_instructions_core - nr_instructions_self)*100)/nr_cycles;
    
    int interesting_sample = shim_trustable_sample(vals[last_index], vals[now_index], 99, 101) &&
      vals[now_index][soft_index_base + EXEC_STAT_OFFSET] == 1 && ipc <= 1000 && ipc_self <= ipc_app;

    if (step <= 0){
      step = rate;
      myjshim->nr_taken_samples++;
      last_index ^= 1;
      now_index ^=1;
      if (interesting_sample){
	//	printf("%d,%d,%d\n", ipc, ipc_app, ipc_self);
	hist[ipc]++;
	hist_app[ipc_app]++;
	hist_self[ipc_self]++;
	myjshim->nr_interesting_samples++;
      } else
	myjshim->nr_bad_samples++;
    }
  }
  shim_read_counters(myjshim->end, myshim);
  uint64_t profiling_cycles = myjshim->end[0] - myjshim->begin[0];

  //report the histogram
  fprintf(dumpfd, "{\"eventHistogram\":{\"rate\":%d, \"samples\":%lld, \"untrustableSamples\":%lld, \"takenSamples\":%lld, \"interestingSamples\":%lld,\n", rate, (long long)(myjshim->nr_samples), (long long)(myjshim->nr_bad_samples), (long long)(myjshim->nr_taken_samples), (long long)(myjshim->nr_interesting_samples));
  dump_global_counters(myjshim); 
  fprintf(dumpfd, "\"hist\":[\n");
  int cmflag = 0;
  for (int i=0; i<1000; i++){
    if (hist[i] != 0 || hist_app[i] != 0 || hist_self[i] != 0){
      char * cmstr = ",";
      if (cmflag == 0){
	cmstr = "";
	cmflag = 1;
      }
      fprintf(dumpfd, "%s{\"ipc\":%d, \"samples\":%d, \"percentage\":%.3f, \"samples_app\":%d, \"percentage_app\":%.3f, \"samples_self\":%d, \"percentage_self\":%.3f}\n", cmstr, i, hist[i], hist[i]/(double)(myjshim->nr_interesting_samples),hist_app[i], hist_app[i]/(double)(myjshim->nr_interesting_samples),hist_self[i], hist_self[i]/(double)(myjshim->nr_interesting_samples));
    }
  }
  fprintf(dumpfd, "]}}\nOBJECTEND\n");

  free(hist);
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_setCurFrequency(JNIEnv * env, jobject obj, jstring jfreq)
{
  const char *freq = (*env)->GetStringUTFChars(env, jfreq, 0);
  char cur_freq[20]; 
  char path[100];
  
  for (int i=0; i<8; i++){
    sprintf(path,"/sys/devices/system/cpu/cpu%d/cpufreq/scaling_setspeed", i);
    int fd = open(path, O_RDWR);
    if (fd == -1)
      continue;
    int nr_read = read(fd, cur_freq, sizeof(cur_freq));
    for (int i=0; i<nr_read; i++){
      if (cur_freq[i] == '\n')
	cur_freq[i] = 0;
    }
    int nr_write = write(fd, freq, strlen(freq));
    printf("setFrequency, cpu:%d, cur freq:%s, new freq:%s\n",
	   i, cur_freq, freq);
    close(fd);
  }
  (*env)->ReleaseStringUTFChars(env,jfreq, freq);
}


#define PREFECHER_MSR_INDEX (0x1a4)

//enable prefecher for the cpu
JNIEXPORT void JNICALL
Java_moma_MomaThread_setPrefetcher(JNIEnv *env, jobject obj, jint cpu, jlong val)
{
  char path[100];
  sprintf(path,"/dev/cpu/%d/msr",cpu);
  uint64_t old_val;
  uint64_t set_val = (uint64_t) val;
  uint64_t new_val;
  int msr_fd = open(path, O_RDWR);
  if (msr_fd == -1){
    err(1,"Can't open msr\n");
    exit(-1);
  }

  lseek(msr_fd, PREFECHER_MSR_INDEX, SEEK_SET);
  read(msr_fd,&old_val,sizeof(old_val));
  write(msr_fd,&set_val, sizeof(set_val));
  read(msr_fd,&new_val,sizeof(new_val));  
  close(msr_fd);
  debug_print("set CPU%d prefecher from %llx to %llx\n", cpu, old_val, new_val);
}

JNIEXPORT jstring JNICALL
Java_moma_MomaThread_getMaxFrequency(JNIEnv * env, jobject obj)
{
  char max_freq[20];
  int fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_max_freq", O_RDONLY);
  if (fd == -1)
    return NULL;
  int nr_read = read(fd, max_freq, sizeof(max_freq));
  max_freq[nr_read] = 0;
  jstring ret_freq = (*env)->NewStringUTF(env, max_freq);
  return ret_freq;
}

JNIEXPORT jstring JNICALL
Java_moma_MomaThread_getMinFrequency(JNIEnv * env, jobject obj)
{
  char min_freq[20];
  int fd = open("/sys/devices/system/cpu/cpu0/cpufreq/scaling_min_freq", O_RDONLY);
  if (fd == -1)
    return NULL;
  int nr_read = read(fd, min_freq, sizeof(min_freq));
  min_freq[nr_read] = 0;
  jstring ret_freq = (*env)->NewStringUTF(env, min_freq);
  return ret_freq;
}
