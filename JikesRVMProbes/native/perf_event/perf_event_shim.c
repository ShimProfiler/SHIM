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
#include <fcntl.h>
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
#define YP_OFFSET (1)
#define CMID_OFFSET (2)
#define PIDSIGNAL_OFFSET (3)

#define ACCESS_ONCE(x) (*(volatile __typeof__(x) *)&(x))

extern char *ttid_to_tr[];

//software signal
struct {
  int targetpid;
  int fpOffset;
  int execStatOffset;
  int cmidOffset;
  char *pid_signal_buf;
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
  int flag;
};

typedef struct jikesShimThread jshim;

jshim *jshims;

static int probe_soft_signals(uint64_t *buf, shim *myshim);
JNIEXPORT void JNICALL Java_moma_MomaThread_initShimProfiler(JNIEnv * env, jobject obj, jint nr_shim, jint fpOffset, jint execStatOffset, jint cmidOffset);
JNIEXPORT int JNICALL Java_moma_MomaThread_initShimThread(JNIEnv * env, jobject obj, jint cpuid, jobjectArray event_strings, jint targetcpu, jstring dumpfilename);
JNIEXPORT void JNICALL Java_moma_MomaThread_shimCounting(JNIEnv * env, jobject obj);

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
  fprintf(dumpfd, "\"date\":\"%s\",\"pid\": %d\n}}\n", tempbuf, sf_signals.targetpid);
}

static void dump_global_counters(jshim *myjshim)
{
  shim *myshim = (shim *)myjshim;

  if (myjshim->dumpfd != NULL){
    FILE *fd = myjshim->dumpfd;
    //dump jason format copy to dumpfd
    //"SHIM.RDTSC":val, "SHIM.LOOP":val,
    fprintf(fd, "\"SHIM.RDTSC\":%lld, \"SHIM.LOOP\":%lld,\n",
	    myjshim->end[0] - myjshim->begin[0], myjshim->nr_samples);
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

static int probe_soft_signals(uint64_t *buf, shim *myshim)
{ 
  //software tags
  uint64_t pidsignal = 0;
  int ypval = 0;
  int cmidval = 0;
  int status = -1;
  int index = 0;
  jshim *myjshim = (jshim *)myshim;
  
  myjshim->nr_samples += 1;
  pidsignal = *(myjshim->pidsource);
  int tid = pidsignal & (0xffffffff);
  int pid = pidsignal >> 32;
  unsigned int offset = tid - pid;    
  if (pid == sf_signals.targetpid && offset < 512){
    char *cr = ttid_to_tr[offset];
    if (cr != NULL){ 
      ypval = *((unsigned int *)(cr + sf_signals.cmidOffset));
      status = *((unsigned int *)(cr + sf_signals.execStatOffset));
      unsigned int * fp_ptr = *(unsigned int **)(cr + sf_signals.fpOffset);
      if (fp_ptr != NULL)
	cmidval = *(fp_ptr-1);
    }
  }
  buf[index++] = status;
  buf[index++] = ypval;
  buf[index++] = cmidval;
  buf[index++] = pidsignal;  
  return index;
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_initShimProfiler(JNIEnv * env, jobject obj, jint nr_shim, jint fpOffset, jint execStatOffset, jint cmidOffset)
{ 
  int i;
  shim_init();
  debug_print("create %d jshim threads\n", nr_shim);
  jshims = (jshim *)calloc(nr_shim, sizeof(jshim));
  if (jshims == NULL){
    err(1,"Can't create jshims");
    exit(-1);
  }
  sf_signals.pid_signal_buf = ppid_init();
  sf_signals.fpOffset = fpOffset;
  sf_signals.execStatOffset = execStatOffset;
  sf_signals.cmidOffset = cmidOffset;  
  sf_signals.targetpid = getpid();
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
  shim_thread_init(myshim, (int)cpuid, nr_events, event_names, probe_soft_signals);

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
    shim_read_counters(myjshim->end, myshim);
  }
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
  
  myjshim->nr_samples = 0;
  myjshim->nr_bad_samples = 0;

  unsigned int  out_range_samples  = 0;
  uint64_t taken_samples = 0;
  uint64_t nr_samples = 0;
 
  struct cmid_tag *hist = calloc(maxCMID, sizeof(struct cmid_tag));
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
    unsigned int  nr_instructions = vals[now_index][INDEX_HW_COUNTERS+1]  - vals[last_index][INDEX_HW_COUNTERS+1];
    unsigned int  nr_cycles = vals[now_index][INDEX_HW_COUNTERS]  - vals[last_index][INDEX_HW_COUNTERS];
    double ipc = (double)nr_instructions / nr_cycles;
    int cmid = vals[now_index][soft_index_base + CMID_OFFSET];
    int interesting_sample = shim_trustable_sample(vals[last_index], vals[now_index]) &&
      vals[now_index][soft_index_base + EXEC_STAT_OFFSET] == 1 &&      
      ipc <= 10 &&
      cmid > 0 &&
      cmid <= maxCMID;


    if (step <= 0){
      step = rate;
      taken_samples++;
      last_index ^= 1;
      now_index ^=1;
      if (interesting_sample){
	hist[cmid].ipc += ipc;
	hist[cmid].nr_sample += 1;
	nr_samples += 1;
      } else
	myjshim->nr_bad_samples++;
    }
  }
  shim_read_counters(myjshim->end, myshim);
  uint64_t profiling_cycles = myjshim->end[0] - myjshim->begin[0];

  //report the histogram
  fprintf(dumpfd, "{\"cmidHistogram\":{\"rate\":%d, \"samples\":%lld, \"untrustableSamples\":%lld, \"outrangeSamples\":%lld, \"takenSamples\":%lld, \"interestingSamples\":%lld\n", rate, (long long)(myjshim->nr_samples), (long long)(myjshim->nr_bad_samples), (long long)(out_range_samples), (long long)(taken_samples), (long long)nr_samples);
  fprintf(dumpfd, "\"hist\":{\n");
  for (int i=0; i<maxCMID; i++){
    if (hist[i].ipc != 0 && hist[i].nr_sample/(double)(nr_samples) >= 0.001)
      fprintf(dumpfd, "[\"cmid\":%d, \"ipc\":%.3f, \"samples\":%lld, \"percentage\":%.3f],\n", i, hist[i].ipc/hist[i].nr_sample, hist[i].nr_sample, hist[i].nr_sample/(double)(nr_samples));
  }
  fprintf(dumpfd, "}}}");

  free(hist);
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_shimEventHistogram(JNIEnv * env, jobject obj, jint rate)
{
  int cpuid = get_cpuid();
  jshim *myjshim = jshims + cpuid;
  shim *myshim = (shim *)myjshim;
  FILE *dumpfd = myjshim->dumpfd;
  
  myjshim->nr_samples = 0;
  myjshim->nr_bad_samples = 0;

  unsigned int  out_range_samples  = 0;
  uint64_t taken_samples = 0;
 
  unsigned int *hist = calloc(1000, sizeof(unsigned int));
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
    unsigned int  nr_instructions = vals[now_index][INDEX_HW_COUNTERS+1]  - vals[last_index][INDEX_HW_COUNTERS+1];
    unsigned int  nr_cycles = vals[now_index][INDEX_HW_COUNTERS]  - vals[last_index][INDEX_HW_COUNTERS];
    int ipc = (nr_instructions * 100)/nr_cycles;
    int interesting_sample = shim_trustable_sample(vals[last_index], vals[now_index]) &&
      vals[now_index][soft_index_base + EXEC_STAT_OFFSET] == 1 && ipc <= 1000;

    if (step <= 0){
      step = rate;
      taken_samples++;
      last_index ^= 1;
      now_index ^=1;
      if (interesting_sample)
	hist[ipc]++;
      else
	myjshim->nr_bad_samples++;
    }
  }
  shim_read_counters(myjshim->end, myshim);
  uint64_t profiling_cycles = myjshim->end[0] - myjshim->begin[0];

  //report the histogram
  fprintf(dumpfd, "{\"eventHistogram\":{\"rate\":%d, \"samples\":%lld, \"untrustableSamples\":%lld, \"outrangeSamples\":%lld, \"takenSamples\":%lld,\n", rate, (long long)(myjshim->nr_samples), (long long)(myjshim->nr_bad_samples), (long long)(out_range_samples), (long long)(taken_samples));
  fprintf(dumpfd, "\"hist\":{\n");
  for (int i=0; i<1000; i++){
    if (hist[i] != 0)
      fprintf(dumpfd, "[\"ipc\":%d, \"samples\":%d, \"percentage\":%.3f],\n", i, hist[i], hist[i]/(double)(taken_samples));
  }
  fprintf(dumpfd, "}}}");

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
    int nr_write = write(fd, freq, strlen(freq));
    printf("setFrequency, cpu:%d, cur freq:%s, new freq:%s\n",
	   i, cur_freq, freq);
  }
  (*env)->ReleaseStringUTFChars(env,jfreq, freq);
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
