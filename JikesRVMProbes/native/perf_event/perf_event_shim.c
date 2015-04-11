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
JNIEXPORT int JNICALL Java_moma_MomaThread_initShimThread(JNIEnv * env, jobject obj, jint cpuid, jobjectArray event_strings, jint targetcpu);
JNIEXPORT void JNICALL Java_moma_MomaThread_shimCounting(JNIEnv * env, jobject obj);

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
Java_moma_MomaThread_initShimThread(JNIEnv * env, jobject obj, jint cpuid, jobjectArray event_strings, jint targetcpu)
{   
  debug_print("jshim init at cpu %d, targetcpu %d\n", cpuid, targetcpu);
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

//Building online event per cycle histogram
//(Event2 * 100 / Event1)
JNIEXPORT void JNICALL
Java_moma_MomaThread_shimEventHistogram(JNIEnv * env, jobject obj, jint rate)
{
  int cpuid = get_cpuid();
  jshim *myjshim = jshims + cpuid;
  shim *myshim = (shim *)myjshim;
  
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
    }
  }

  //report the histogram
  printf("#rate %d, samples %lld, badsamples %lld, out range samples %lld, taken samples %lld\n", rate, (long long)(myjshim->nr_samples), (long long)(myjshim->nr_bad_samples), (long long)(out_range_samples), (long long)(taken_samples));
  for (int i=0; i<1000; i++){
    if (hist[i] != 0)
      printf("IPC %d:%d %.3f\n", i, hist[i], hist[i]/(double)(taken_samples));
  }
  free(hist);

  shim_read_counters(myjshim->end, myshim);
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
