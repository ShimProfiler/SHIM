#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <err.h>
#include <errno.h>
#include <sched.h>
#include <jni.h>
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

#define LOGGING_BUFSIZE (64*1024*1024)
#define HISTOGRAM_BUFSIZE (100*1024*1024)
#define COUNTING_BUFSIZE (16)
#define HISTOGRAM_BUFNR ((HISTOGRAM_BUFSIZE)/4)

#define SOFT_PID_INDEX (0)
#define SOFT_TAG_INDEX (1)
#define SOFT_NR_INDEX (1)
#define CACHE_LINE_SIZE (64)

#define NR_CPU_THREADS (8)

#define HISTOGRAM_BUF_SIZE (0x8000000)
#define PHASE_HISTOGRAM (1)
#define PHASE_LOGGING (2)
#define PHASE_COUNTING (3)

#define SAMPLING_PHASE_SOFT (1<<0)
#define SAMPLING_PHASE_FP (1<<1)
#define COUNTING_PHASE_SOFTONLY (1<<0)
#define COUNTING_PHASE_HARDONLY (1<<1)

//structures we need
struct moma_event_attr {
  int fd;
  int index;
  struct perf_event_attr perf_attr;
  struct perf_event_mmap_page *buf;
  char * name;
};

struct moma_buf_struct{
  unsigned long long *buf;
  unsigned long long *cur;
  unsigned long long *end;
};

struct moma_shim_struct{
  volatile int flag;  
  struct moma_event_attr *moma_hard_attrs;
  unsigned long long **moma_soft_attrs;
  struct moma_buf_struct *moma_buf;
  int nr_hard_attrs;
  int nr_soft_attrs;
  int nr_attrs;
  int nr_read_attrs;
  int readmask;
  FILE * fd;
  int nrdumpline;
  int rate;  
  int cpu;
  int targetcpu;
  int nr_badsample;
  unsigned long long counter_begin[10];
  unsigned long long counter_end[10];
};

typedef struct moma_shim_struct shim;


//global variables
//target process
int targetpid;
//per-thread profiler thread
shim* shims;
// for same process, *(unsigned long long *(ttid_to_tr[ttid - pid] + offset))
extern char *ttid_to_tr[];
int cmidoffset;
int fpoffset;
int statusoffset;
char * pid_signal_buf;
#define READ_HW_COUNTERS (0x1)

static unsigned long long __inline__ rdtsc (void)
{
  unsigned int tickl, tickh;
  __asm__ __volatile__("rdtscp":"=a"(tickl),"=d"(tickh)::"%ecx");
  return ((unsigned long long)tickh << 32)|tickl;
}

static void __inline__ relax_cpu()
{
  __asm__ volatile("rep; nop\n\t"::: "memory");
}

static int __inline__ read_hwcounter(unsigned long long *buf, shim *who)
{
  int i =0;
  buf[i] = rdtsc();
  for (i=0; i<who->nr_hard_attrs; i++){
    buf[i+1] = __builtin_ia32_rdpmc(who->moma_hard_attrs[i].index);
  }
  return i;
}

static int __inline__ read_counters(unsigned long long *buf, shim *myshim, volatile unsigned long long *pidsource)
{
  int index = 0;
  int i = 0;
  //control sampling frequency
  for (i=0;i<myshim->rate;i++)
    relax_cpu();    
  //start timestamp
  buf[index++] = rdtsc();
  //hardware counters
  for (i=0; i<myshim->nr_hard_attrs; i++){
    rdtsc();
    buf[index++] = __builtin_ia32_rdpmc(myshim->moma_hard_attrs[i].index);
  }
  //end timestamp
  buf[index++] = rdtsc();

  //software tags
  int ypval = 0;
  int cmidval = 0;
  int status = -1;
  unsigned long long pidsignal = 0;
  pidsignal = *pidsource;      
  int tid = pidsignal & (0xffffffff);
  int pid = pidsignal >> 32;
  unsigned int offset = tid - pid;    
  if (pid == targetpid && offset < 512){
    char *cr = ttid_to_tr[offset];
    if (cr != NULL){ 
      //      printf("%x\n", cr + cmidoffset);
      volatile unsigned int * cmid_ptr = (unsigned int *)(cr + cmidoffset);
      volatile unsigned int * fp_ptr = *(unsigned int **)(cr + fpoffset);
      volatile unsigned int *status_ptr = (unsigned int *)(cr + statusoffset);
      //      printf("pid %d, tid %d, targetpid %d, offset %d, cr %p, cmid_ptr %p\n", pid, tid, targetpid, offset, cr, cmid_ptr);

      ypval = *cmid_ptr;
      status = *status_ptr;
      if (fp_ptr != NULL)
	cmidval = *(fp_ptr-1);
    }
  }     
  buf[index++] = pidsignal;  
  buf[index++] = ypval;
  buf[index++] = cmidval;
  buf[index++] = status;
  
  return index;
}

int cpuid()
{
  cpu_set_t cpuset;
  int j;
  CPU_ZERO(&cpuset);
  pthread_getaffinity_np(pthread_self(), sizeof(cpuset), &cpuset);
  //  printf("There are %d CPUs in the set\n", CPU_COUNT(&cpuset));
  assert(CPU_COUNT(&cpuset) == 1);
  for (j = 0; j < CPU_SETSIZE; j++)
        if (CPU_ISSET(j, &cpuset))
	  return j;
  return -1;
}

static int __inline__ trustable_sample(unsigned long long *end, unsigned long long *start, shim *who)
{  
  int cycle_begin_index = 0;
  int cycle_end_index = who->nr_hard_attrs + 1;
  int cycle_begin_diff = end[cycle_begin_index] - start[cycle_begin_index];
  int cycle_end_diff = end[cycle_end_index] - start[cycle_end_index];
  int cpc = (cycle_end_diff * 100 ) / cycle_begin_diff;
  if (cpc < 99 || cpc > 101)
    return 0;
  return 1;
}


void init_moma_buf(struct moma_buf_struct *moma_buf, int size)
{
  if (moma_buf->buf != NULL){
    memset(moma_buf->buf, 0, size);
    return;
  }
  
  moma_buf->buf = (unsigned long long *)malloc(size);    
  if (moma_buf->buf == NULL)
    errx(1, "error allocating struct BUFSIZE");    
  memset(moma_buf->buf, 0, size);
  moma_buf->cur = moma_buf->buf;
  moma_buf->end = moma_buf->buf + size/sizeof(unsigned long long);
}



void dump_as_string(shim *my){
  char outbuf[4096];
  int i,n;
  char *outcur;
  struct moma_buf_struct *buf = my->moma_buf;
  int nr_total_attrs = my->nr_read_attrs;
  FILE *dumpfd = my->fd;
  unsigned long long val;
  unsigned int templine = 1;
  //current cursor for processed data
  unsigned long long * pcur = buf->buf;

  while(buf->cur - pcur >= 0){
    outcur = outbuf;   
    n = sprintf(outcur, "{\"id\":%d", templine++);
    outcur += n;
    
    int counterindex = 0;
    n = sprintf(outcur, ",\"stpb\":%lld", pcur[counterindex++]);
    outcur += n;      
    for (i=0;i<my->nr_hard_attrs;i++){
      n = sprintf(outcur, ",\"hard%d\":%lld", i, pcur[counterindex++]);
      outcur += n;      
    }
    n = sprintf(outcur, ",\"stpe\":%lld", pcur[counterindex++]);
    outcur += n;      
    val = pcur[counterindex++];
    int ttid = val & 0xffffffff;
    int pid = val >> 32;
    n = sprintf(outcur, ",\"pid\":%d,\"ttid\":%d", pid, ttid);
    outcur += n;      
    
    val = pcur[counterindex++];
    int fpcmid = pcur[counterindex++];
    int stat = pcur[counterindex++];
    int yp = val & 0x1ff;
    int cmid = val >> 9;
    n = sprintf(outcur, ",\"cmid\":%d,\"yp\":%d,\"fpcmid\":%d,\"status\":%d", cmid, yp, fpcmid, stat);
    outcur += n;      

    n = sprintf(outcur, "}");
    outcur += n + 1;
    fprintf(dumpfd, "%s\n", outbuf);
    pcur += nr_total_attrs;
  }
}

static void dump_header_string(shim *my, int iteration)
{
  FILE * dumpfd = my->fd;
  time_t curtime;
  struct tm *loctime;
  int i;
  char tempbuf[32]; 
  curtime = time(NULL);
  loctime = localtime(&curtime);
  fprintf(dumpfd, "{\n");
  i = sprintf(tempbuf, "%s", asctime(loctime));
  tempbuf[i-1] = 0;
  fprintf(dumpfd, "\"date\":\"%s\",\n", tempbuf);
  fprintf(dumpfd, "\"pid\":\"%d\",\n", targetpid);
  for (i=0;i<my->nr_hard_attrs;i++){
    fprintf(dumpfd, "\"hard%d\":\"%s\",\n", i,my->moma_hard_attrs[i].name);
  }
  fprintf(dumpfd, "\"iteration\":\"%d\",\n", iteration);
}

static void dump_ipc(shim *my, int iteration)
{
  //current cursor for processed data
  struct moma_buf_struct *buf = my->moma_buf;
  FILE *dumpfd = my->fd;
  float * start = (float *)(buf->buf);
  float * end = (float *)(buf->end);
  float * begin = start;

  printf("dump_ipc\n");
  fprintf(dumpfd, "iteration:%d\n", iteration);
  while(start <= end){
    int cmid = (start - begin)/2;
    if (start[1] > 1000)
      fprintf(dumpfd, "%d:%f:%f\n", cmid, start[1], start[0]);
    start += 2;
  }
  fprintf(dumpfd, "badsamples %d\n", my->nr_badsample);
  printf("end dump_ipc\n");
  fflush(dumpfd);

}

JNIEXPORT void JNICALL
Java_moma_MomaThread_setFrequency(JNIEnv * env, jobject obj, jstring jfreq)
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
Java_moma_MomaThread_maxFrequency(JNIEnv * env, jobject obj)
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
Java_moma_MomaThread_minFrequency(JNIEnv * env, jobject obj)
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


JNIEXPORT void JNICALL
Java_moma_MomaThread_resetCounting(JNIEnv * env, jobject obj, int phase)
{
  return;
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_reportCounting(JNIEnv * env, jobject obj, int cpu)
{
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_reportShimStat(JNIEnv * env, jobject obj, int cpu)
{
  shim *myshim = shims + cpu;
  int i;

  //  for (i=0;i<myshim->nr_hard_attrs+1;i++){
  //    printf("%d:%lld, %lld\n", i, myshim->counter_begin[i], myshim->counter_end[i]);
  //  }
  printf("============================ Tabulate Statistics ============================\n");
  printf("SHIM.RDTSC\tSHIM.LOOP");
  for (i=0;i<myshim->nr_hard_attrs;i++){
    printf("\tSHIM.%s",myshim->moma_hard_attrs[i].name);
  }
  printf("\n");
  printf("%lld\t%lld", myshim->counter_end[0] - myshim->counter_begin[0], myshim->counter_begin[9]);
  for (i=0;i<myshim->nr_hard_attrs;i++){
    printf("\t%lld",myshim->counter_end[i+1] - myshim->counter_begin[i+1]);
  }
  printf("\n");
  printf("---------------------------- End --------------------------------------------\n");
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_resetHistogram(JNIEnv * env, jobject obj, int cpu)
{
  shim *myshim = shims + cpu;  
  myshim->nr_badsample = 0;
  memset(myshim->moma_buf->buf, 0, 0x100000);
  memset((char *)0x50000000, 0, 0x100000);
}

int compare_histogram(const void *p1, const void *p2, void *v_array)
{
  unsigned int index1 = *(unsigned int *)p1;
  unsigned int index2 = *(unsigned int *)p2;
  
  unsigned int *vals = v_array;
  if (vals[index1] < vals[index2])
    return -1;
  if (vals[index1] == vals[index2])
    return 0;
  if (vals[index1] > vals[index2])
    return 1;      
}

static void histogram_top_10(unsigned int *val_buf, unsigned int *top10)
{
  unsigned int *tempbuf = (unsigned int*)(shims[1].moma_buf->buf);
  int i;
  int nmemb =HISTOGRAM_BUFNR;
  for (i=0;i<nmemb;i++){
    tempbuf[i] = i;
  }
  qsort_r(tempbuf, nmemb, sizeof(unsigned int), compare_histogram, val_buf);
  
  for (i=0;i<10;i++){
    top10[i] = tempbuf[nmemb-(1+i)];
  }
}

static double histogram_entropy(unsigned int *buf, unsigned long total)
{
  int i;
  double entropy = 0.0;
  for (i=0;i<HISTOGRAM_BUFNR;i++){
    if (buf[i] == 0)
      continue;
    double p = (double)(buf[i])/total;
    entropy += p * log2(1/p);
  }
  return entropy;
}

//write raw histogram to file
//format, index,value,percentage

void dump_histogram_to_fd(unsigned int *buf, FILE *fd, unsigned long long total)
{
  int i;
  fprintf(fd,"#index,value,percentage\n");
  for (i=0;i<HISTOGRAM_BUFNR;i++){
    unsigned int val = buf[i];
    if (val == 0)
      continue;
    fprintf(fd,"%d,%d,%.3f\n", i, val, val / (double)total);
  }
}

//cpu:1 -> count on remote 
//cpu:2 -> count on local
//cpu:3 -> count on both
JNIEXPORT void JNICALL
Java_moma_MomaThread_reportHistogram(JNIEnv * env, jobject obj, int cpu)
{
  shim *shim0 = shims + 0;
  shim *shim3 = shims + 3;
  unsigned int * buf0 = (unsigned int *)shim0->moma_buf->buf;
  unsigned int * buf3 = (unsigned int *)shim3->moma_buf->buf;
  unsigned int shim0top10[10];
  unsigned int shim3top10[10];
  histogram_top_10(buf0, shim0top10);
  histogram_top_10(buf3, shim3top10);
  int pointShim0 = 0;
  int pointShim3 = 0;
  
  int i,j;
  unsigned long long t1=0;
  unsigned long long t2=0;
  unsigned long long t1top10 = 0;
  unsigned long long t2top10 = 0;
  unsigned int max1 = 0;
  unsigned int max2 = 0;
  unsigned int maxindex1 = 0;
  unsigned int maxindex2 = 0;
  double entropyShim0 = 0;
  double entropyShim3 = 0;

  

  for (i=0;i<HISTOGRAM_BUFNR;i++){
    unsigned int val1 = buf0[i];
    unsigned int val2 = buf3[i];
    if (val1 > max1){
      max1 = val1;
      maxindex1 = i;
    }
    if (val2 > max2){
      max2 = val2;
      maxindex2 = i;
    }
    t1+= val1;
    t2+= val2;
  }    

  if (t1 != 0){
    dump_histogram_to_fd(buf0, shim0->fd, t1);
    entropyShim0 = histogram_entropy(buf0, t1);
  }

  if (t2 != 0){
    dump_histogram_to_fd(buf3, shim3->fd, t2);
    entropyShim3 = histogram_entropy(buf3, t2);
  }

  for (i=0;i<10;i++){
    unsigned int val1 = shim0top10[i];
    unsigned int val2 = shim3top10[i];
    t1top10 += buf0[val1];
    t2top10 += buf3[val2];
    int flag = 0;
    for (j=0;j<10;j++){
      if (val1 == shim3top10[j]){
	flag = 1;
	break;
      }
    }
    if (flag == 0)
      pointShim0++;
  }
  for (i=0;i<10;i++){
    unsigned int val1 = shim3top10[i];
    int flag = 0;
    for (j=0;j<10;j++){
      if (val1 == shim0top10[j]){
	flag = 1;
	break;
      }
    }
    if (flag == 0)
      pointShim3++;
  }

  //calculate entropy

  for (i=0;i<10;i++){
    printf("shim0:%d,%d,%d\n", i, shim0top10[i], buf0[shim0top10[i]]);
    printf("shim3:%d,%d,%d\n", i, shim3top10[i], buf3[shim3top10[i]]);
  }
  
  printf("============================ Tabulate Statistics ============================\n");
  switch(cpu){
  case 1:
    //REMOTE 
    printf("TOTAL\tTOP10CMID\tMISSLINE\tENTROPY\n"
	   "%lld\t%lld\t%d\t%f\n", 
	   t1, t1top10, shim0->nr_badsample, entropyShim0);
    break;
  case 2:
    printf("TOTAL\tTOP10CMID\tMISSLINE\tENTROPY\n"
	   "%lld\t%lld\t%d\t%f\n", 
	   t2, t2top10, shim3->nr_badsample, entropyShim3);
    break;
  case 3:
    printf("REMOTECORETOTAL\tSAMECORETOTAL\tREMOTECORETOP10\tSAMECORETOP10\tREMOTECORETOP10CMID\tSAMECORETOP10CMID\tREMOTEMISSLINE\tSAMEMISSLINE\tREMOTEENTROPY\tSAMEENTROPY\n"
	   "%lld\t%lld\t%d\t%d\t%lld\t%lld\t%d\t%d\t%f\t%f\n", 
	   t1, t2, pointShim0, pointShim3, t1top10, t2top10,shim0->rate,shim3->rate,entropyShim0, entropyShim3);
    break;
  }

  printf("---------------------------- End --------------------------------------------\n");
  /* fprintf(dumpfd, "total: %lld, %lld\n", t1, t2);  */
  /* for (i=0;i<0x100000/4;i++){ */
  /*   if (buf1[i] || buf2[i]) */
  /*     fprintf(dumpfd, "%d:%d,%d\n", i, buf1[i], buf2[i]); */
  /* } */
  /* fclose(dumpfd); */
}

JNIEXPORT jint JNICALL
Java_moma_MomaThread_initShim(JNIEnv * env, jobject obj, jint offset, jint stat)
{

/* 1. Find the major number assigned to the driver
 *
 *	grep mmap_alloc /proc/devices'
 *
 * 2. Create the special file (assuming major number 254)
 *
 *	mknod /dev/mmap_alloc c 250 0
*/
	int fd;
        int *kadr;
	int len = 1 * getpagesize();
	fpoffset = offset;
	statusoffset = stat;
	if ((fd=open("/dev/mmap_alloc", O_RDWR|O_SYNC)) < 0) {
		perror("open");
		exit(-1);
	}
	kadr = mmap(0, len, PROT_READ|PROT_WRITE, MAP_SHARED| MAP_LOCKED,
	    fd, 0);
	if (kadr == MAP_FAILED)	{
		perror("mmap");
		exit(-1);
	}
	pid_signal_buf = (char *)kadr;

	//step two
	shims = (shim *)malloc(NR_CPU_THREADS * sizeof(shim));	
	if (shims == NULL){
	  perror("malloc shims");
	  exit(-1);
	}	
	memset(shims, 0, NR_CPU_THREADS * sizeof(shim));
	targetpid = getpid();	       

	int ret = pfm_initialize();
	if (ret != PFM_SUCCESS) {
	  errx(1, "error in pfm_initialize: %s", pfm_strerror(ret));
	}	
	
	return (int)pid_signal_buf;
}

//pick up one shim based on which CPU this thread sitting on, then init the handler.
JNIEXPORT void JNICALL
Java_moma_MomaThread_updateRate(JNIEnv * env, jobject obj, jint newrate, jint targetcpu, jint offset)
{
  int cpu = cpuid();
  shim *myshim = shims + cpu;  
  myshim->rate = newrate;
  myshim->targetcpu = targetcpu;
  myshim->moma_soft_attrs[SOFT_PID_INDEX] = (unsigned long long *)(pid_signal_buf + targetcpu * CACHE_LINE_SIZE);
  cmidoffset = (int)offset;
}


//pick up one shim based on which CPU this thread sitting on, then init the handler.
JNIEXPORT jint JNICALL
Java_moma_MomaThread_initEvents(JNIEnv * env, jobject obj, jint nrHardwareEvents, jstring jfileName, jint jflag)
{
  int cpu = cpuid();
  shim *myshim = shims + cpu;  
  int numEvents = nrHardwareEvents;
  struct moma_event_attr * moma_hard_attrs;
  struct moma_buf_struct * moma_buf;
  unsigned long long ** moma_soft_attrs;
  FILE * dumpfd;
  //  int flag = jflag;

  //start with hardware attrs
  //  printf("shim%d,  init %d events\n", cpu, pthread_self(), numEvents);

  //software signal
  myshim->nr_soft_attrs = SOFT_NR_INDEX;
  moma_soft_attrs = (unsigned long long **)malloc(sizeof(long long *) * myshim->nr_soft_attrs);
  if (moma_soft_attrs == NULL)
    errx(1, "error allocating moma_soft_attrs");
  moma_soft_attrs[SOFT_PID_INDEX] = NULL;

  //hardware signal
  if (numEvents > 0){
    moma_hard_attrs = (struct moma_event_attr *)malloc(numEvents * sizeof(struct moma_event_attr)); 
    if (!moma_hard_attrs) {
      errx(1, "error allocating perf_event_fds");
    }
    memset(moma_hard_attrs,0,numEvents * sizeof(struct moma_event_attr));
    for(int i=0; i < numEvents; i++) {    
      moma_hard_attrs[i].perf_attr.size = sizeof(struct perf_event_attr);
    }
  }
    
  myshim->nr_hard_attrs = numEvents;
  //  printf("%s:shim%d create %d moma_event_attr, each size in %d bytes,including %d bytes from perf_event_attr\n",
  //	 __FUNCTION__, cpu, numEvents, sizeof(struct moma_event_attr), sizeof(struct perf_event_attr));

  myshim->nr_attrs = myshim->nr_soft_attrs + myshim->nr_hard_attrs;

  //then thread local buf for kepping data
  moma_buf = (struct moma_buf_struct *)calloc(1,sizeof(struct moma_buf_struct));
  if (moma_buf == NULL)
    errx(1, "error allocating struct moma_buf_struct");
  
  const char * filename = (*env)->GetStringUTFChars(env, jfileName, 0);
  //open dump file "/tmp/dumpMoma"
  dumpfd = fopen(filename, "w+");
  if (dumpfd == NULL)
    printf("can not create %s\n", filename);
  (*env)->ReleaseStringUTFChars(env,jfileName, filename);

  myshim->moma_hard_attrs = moma_hard_attrs;
  myshim->moma_soft_attrs = moma_soft_attrs;
  myshim->moma_buf = moma_buf;
  myshim->flag = 0;
  myshim->fd = dumpfd;
  myshim->nrdumpline = 0;
  myshim->rate = -1;
  myshim->cpu = cpu;

  return (jint)(&(myshim->flag));
}

JNIEXPORT jint JNICALL
Java_moma_MomaThread_createHardwareEvent(JNIEnv * env, jobject obj, jint jeventid, jstring jeventName)
{
  int i;
  int id = jeventid;
  const char * eventName = (*env)->GetStringUTFChars(env, jeventName, 0);  
  int cpu = cpuid();
  shim *myshim = shims + cpu;  

  struct moma_event_attr *moma_attrs = myshim->moma_hard_attrs;
  struct perf_event_attr *pe = &(moma_attrs[id].perf_attr);
  int ret = pfm_get_perf_event_encoding(eventName, PFM_PLM3, pe, NULL, NULL);
  if (ret != PFM_SUCCESS) {
    errx(1, "error creating event %d '%s': %s\n", id, eventName, pfm_strerror(ret));
  }
  pe->sample_type = PERF_SAMPLE_READ;    
  moma_attrs[id].fd = perf_event_open(pe, 0, -1, -1, 0);
  if (moma_attrs[id].fd == -1) {
    err(1, "error in perf_event_open for event %d '%s'", id, eventName);
  }
  //mmap the fd to get the raw index
  moma_attrs[id].buf = (struct perf_event_mmap_page *)mmap(NULL, sysconf(_SC_PAGESIZE), PROT_READ, MAP_SHARED, moma_attrs[id].fd, 0);
  if (moma_attrs[id].buf == MAP_FAILED) {
    err(1,"mmap on perf fd");
  }

  moma_attrs[id].name = (char *)malloc(strlen(eventName) + 1);
  if (moma_attrs[id].name == NULL){
    err(1, "mmap small buffer for moma_attrs[id].name");
  }
  strncpy(moma_attrs[id].name, eventName, strlen(eventName) + 1);

  if (id == myshim->nr_hard_attrs - 1){
    for (i=0;i<myshim->nr_hard_attrs;i++){
      printf("Creat moma event name:%s, fd:%d, index:%x, buf address %p\n",
	     moma_attrs[i].name,
	     moma_attrs[i].fd,
	     (int)(moma_attrs[i].buf->index - 1),
	     moma_attrs[i].buf);
      moma_attrs[i].index = (int)(moma_attrs[i].buf->index - 1);
    }
  }

  (*env)->ReleaseStringUTFChars(env,jeventName, eventName);
  moma_attrs[id].index = moma_attrs[id].buf->index - 1;
  return (jint)(moma_attrs[id].index);
}


void counting_hardonly()
{
  int index = 0;
  int i;
  int cpu = cpuid();
  shim *myshim = shims + cpu; 
  int nr_total_attrs;
  unsigned long long tempbuf[10];
  int * nr_loop = (int *)(myshim->counter_begin + 9);
  struct timespec deadline;
  deadline.tv_sec = 0;
  deadline.tv_nsec = 0;
  int wflag = 0;
  if (myshim->rate > 100000)
    wflag = 1;
  printf("shim%d hardonly pin cpu %d, at rate %d, wflag %d \n", cpu, myshim->targetcpu, myshim->rate, wflag);
  read_hwcounter(myshim->counter_begin, myshim);
  while(myshim->flag != 0xdead){
    nr_loop[0] += 1;
    tempbuf[index++] = rdtsc();
    for (i=0; i<myshim->nr_hard_attrs; i++){
      rdtsc();
      tempbuf[index++] = __builtin_ia32_rdpmc(myshim->moma_hard_attrs[i].index);
    }
    tempbuf[index++] = rdtsc();

    if (wflag){
      clock_nanosleep(CLOCK_REALTIME, 0, &deadline, NULL);
      continue;
    }
    for (i=0;i<myshim->rate;i++)
      relax_cpu();
  }
  read_hwcounter(myshim->counter_end, myshim); 
}

void counting_softonly()
{

  int cpu = cpuid();
  int i;

  shim *myshim = shims + cpu;
  unsigned long long * pidsignalsource = (myshim->moma_soft_attrs[0]);
  int *histogram_buf = (int *)(myshim->moma_buf->buf);

  int * nr_loop = (int *)(myshim->counter_begin + 9);
  struct timespec deadline;
  deadline.tv_sec = 0;
  deadline.tv_nsec = 0;
  int wflag = 0;
  if (myshim->rate > 100000)
    wflag = 1;
  printf("shim%d softonly pin cpu %d, at rate %d, wflag %d \n", cpu, myshim->targetcpu, myshim->rate, wflag);
  read_hwcounter(myshim->counter_begin, myshim);
  while(myshim->flag != 0xdead){
    //pid|tid signal of targeted cpu
    nr_loop[0] += 1;
    unsigned long long who  = *pidsignalsource;
    int tid = who & (0xffffffff);
    int pid = who >> 32;
    unsigned int offset = tid - pid;    
    if (pid == targetpid && offset < 512){
      char *cr = ttid_to_tr[offset];
      if (cr != NULL){ 
	//      printf("%x\n", cr + cmidoffset);
	unsigned int * cmid_ptr = (unsigned int *)(cr + cmidoffset);
	int val = *cmid_ptr;
	histogram_buf[0]+=val;
      }
    }
    if (wflag){
      clock_nanosleep(CLOCK_REALTIME, 0, &deadline, NULL);
      continue;
    }
    for (i=0;i<myshim->rate;i++)
      relax_cpu();

  }
  read_hwcounter(myshim->counter_end, myshim);
}


JNIEXPORT void JNICALL
Java_moma_MomaThread_counting(JNIEnv *env, jobject obj, jint jphase)
{
  if (jphase & COUNTING_PHASE_SOFTONLY){    
    counting_softonly();
  }else if (jphase & COUNTING_PHASE_HARDONLY){
    counting_hardonly();
  } else {
    printf("Err: unknown counting phase\n");
  }
}

#define CYCLE_BEGIN_INDEX (0)
#define INS_INDEX (1)
#define CYCLE_END_INDEX (2)

JNIEXPORT void JNICALL
Java_moma_MomaThread_histogram(JNIEnv *env, jobject obj, jint jphase)
{
  //let's build two histograms, one is for IPC and another is for Frequency counter
  int cpu = cpuid();
  shim *myshim = shims + cpu;
  unsigned long long * pidsignalsource = (myshim->moma_soft_attrs[0]);
  unsigned int *histogram_freq = (unsigned int *)(myshim->moma_buf->buf);
  unsigned int *histogram_cycle = (unsigned int *)(shims[1].moma_buf->buf);
  unsigned int *histogram_ins = (unsigned int *)(shims[2].moma_buf->buf);
  int i;
  read_hwcounter(myshim->counter_begin, myshim);

  int * nr_loop = (int *)(myshim->counter_begin + 9);
  unsigned long long  sample[3][2];
  int old_index = 0;
  int new_index = 1;
  
  sample[old_index][CYCLE_BEGIN_INDEX] = rdtsc();
  rdtsc();
  sample[old_index][INS_INDEX] = __builtin_ia32_rdpmc(myshim->moma_hard_attrs[0].index);
  sample[old_index][CYCLE_END_INDEX] = rdtsc();

  while(myshim->flag != 0xdead){
    //pid|tid signal of targeted cpu
    unsigned long long *p_old = sample[old_index];
    unsigned long long *p_new = sample[new_index];

    nr_loop[0] += 1;

    //loop
    for (i=0;i<myshim->rate;i++)
      relax_cpu();

    //hardware
    p_new[CYCLE_BEGIN_INDEX] = rdtsc();
    rdtsc();
    p_new[INS_INDEX] = __builtin_ia32_rdpmc(myshim->moma_hard_attrs[0].index);
    p_new[CYCLE_END_INDEX] = rdtsc();

    //software
    int yp = -1;
    unsigned long long who  = *pidsignalsource;
    int tid = who & (0xffffffff);
    int pid = who >> 32;
    unsigned int offset = tid - pid;    
    if (pid == targetpid && offset < 512){
      char *cr = ttid_to_tr[offset];
      if (cr != NULL){ 
	//      printf("%x\n", cr + cmidoffset);
	unsigned int * cmid_ptr = (unsigned int *)(cr + cmidoffset);
	yp = *cmid_ptr;
      }
    }

    //should we trust this sample?
    int cycle_begin_diff = p_new[CYCLE_BEGIN_INDEX] - p_old[CYCLE_BEGIN_INDEX];
    int cycle_end_diff = p_new[CYCLE_BEGIN_INDEX] - p_old[CYCLE_BEGIN_INDEX];
    int ins_diff = p_new[INS_INDEX] - p_old[INS_INDEX];

    int cpc = (cycle_end_diff * 100 ) / cycle_begin_diff;

    old_index = new_index;
    new_index = (new_index + 1) & 0x1;	

    if (yp == -1 ||
	yp >= HISTOGRAM_BUFNR ||
	cpc < 97 ||
	cpc > 103){
      myshim->nr_badsample++;
      continue;
    }

    //got one sample
    histogram_freq[yp] += 1;
    histogram_cycle[yp] += cycle_begin_diff;
    histogram_ins[yp] += ins_diff;    
  }
  read_hwcounter(myshim->counter_end, myshim);
}


JNIEXPORT void JNICALL
Java_moma_MomaThread_ipc(JNIEnv *env, jobject obj, jint iteration)
{
  //let's build two histograms, one is for IPC and another is for Frequency counter
  int cpu = cpuid();
  shim *myshim = shims + cpu;

  printf("shim%d: start building IPC histogram\n", cpu);

  //alloc the buffer when we first time use this
  init_moma_buf(myshim->moma_buf, HISTOGRAM_BUFSIZE);
  struct moma_buf_struct *moma_buf = myshim->moma_buf;
  float * cmid_map = (float *) myshim->moma_buf->cur;
  int max_cmid_buf = HISTOGRAM_BUFSIZE / 8;

  unsigned long long * pidsource = myshim->moma_soft_attrs[0];
  int cycle_signal_offset = 1;
  int instruction_signal_offset = 2;
  int pid_signal_offset = myshim->nr_hard_attrs + 2;
  int yp_signal_offset = myshim->nr_hard_attrs + 3;
  int cmid_signal_offset = myshim->nr_hard_attrs + 4;
  int status_signal_offset = myshim->nr_hard_attrs + 5;

  
  int begin_index = 0;
  int end_index = 1;
  unsigned long long tempbuf[2][20];

  read_hwcounter(myshim->counter_begin, myshim);

  int * nr_loop = (int *)(myshim->counter_begin + 9);

  read_counters(tempbuf[end_index], myshim, pidsource);
  printf("start to jump in the loop\n");
  while(myshim->flag != 0xdead){
    end_index = begin_index;
    begin_index = (begin_index + 1) & 0x1;	
    //pid|tid signal of targeted cpu
    unsigned long long *p_end = tempbuf[end_index];
    unsigned long long *p_begin = tempbuf[begin_index];
    //    printf("begin index %d, end index %d\n", begin_index, end_index);

    nr_loop[0] += 1;

    read_counters(p_end, myshim, pidsource);

    //trustable?
    if (!trustable_sample(p_end, p_begin, myshim)){
      myshim->nr_badsample++;
      continue;
    }

    int last_cmid = p_end[yp_signal_offset] >> 9;
    int compare_cmid = p_begin[yp_signal_offset] >> 9;
    //       printf("trustable sample cmid %d\n", last_cmid);

    if (p_end[status_signal_offset] != 1 ||
	last_cmid == 0 || 
	last_cmid >= max_cmid_buf) { 
      myshim->nr_badsample++;
      continue;
    }

    //got one sample， now aggregate it to the buffer.
    float * cmid_ipc = cmid_map + last_cmid * 2;
    int nr_cycle = p_end[cycle_signal_offset] - p_begin[cycle_signal_offset];
    if (nr_cycle == 0)
      continue;
    float nr_ins = p_end[instruction_signal_offset] - p_begin[instruction_signal_offset];
    float ipc = nr_ins / (float)(nr_cycle);
    //       printf("cmid %d has ipc %f\n", last_cmid, ipc);
    //        printf("cmid_ipc %p, buf %p, end %p\n", cmid_ipc, myshim->moma_buf->cur, myshim->moma_buf->end);
    cmid_ipc[1] += 1;
    if (cmid_ipc[0] != 0)
      cmid_ipc[0] = (cmid_ipc[0] + ipc) / 2;
    else
      cmid_ipc[0] = ipc;
    //    printf("aggregate sample ipc %f\n", ipc);
  }

  dump_ipc(myshim, iteration);
  read_hwcounter(myshim->counter_end, myshim);
  myshim->flag = 0;
}

JNIEXPORT void JNICALL
Java_moma_MomaThread_sampling(JNIEnv *env, jobject obj, jint jphase, jint iteration)
{  
  int i;
  int cpu = cpuid();
  int phase = jphase;
  shim *myshim = shims + cpu; 
  myshim->readmask = 1; 
  
  //alloc the buffer when we first time use this
  init_moma_buf(myshim->moma_buf, LOGGING_BUFSIZE);
  struct moma_buf_struct *buf = myshim->moma_buf;
 
  int nr_total_attrs;
  unsigned long long *pidsource = myshim->moma_soft_attrs[0];
  unsigned long long temprdtsc;
  //number of events plus one timestamp
  printf("shim%d pin cpu %d, at rate %d, phase %d, iteration %d\n", cpu, myshim->targetcpu, myshim->rate, phase, iteration);  
  myshim->nr_read_attrs = myshim->nr_attrs + 5;
  nr_total_attrs = myshim->nr_read_attrs;
  FILE * dumpfd = myshim->fd;
  dump_header_string(myshim, iteration);
  //mark global begin counters
  read_hwcounter(myshim->counter_begin, myshim);

  //read it first time
  buf->cur = buf->buf;
  unsigned long long *tempbuf = buf->cur;
  int index = 0;
  while(1){
    read_counters(buf->cur, myshim, pidsource);    
    //increase cursor
    buf->cur += nr_total_attrs;

    if (buf->cur + nr_total_attrs > buf->end || myshim->flag == 0xdead){
      printf("dump data\n");
      printf("reset: buf %p, cur %p, end %p, nr_attrs %d, flag: %x\n", buf->buf, buf->cur, buf->end, nr_total_attrs, myshim->flag);
      //time to process the buffer
      dump_as_string(myshim);
      buf->cur = buf->buf;
      printf("reset: buf %p, cur %p, end %p, nr_attrs %d, flag: %x\n", buf->buf, buf->cur, buf->end, nr_total_attrs, myshim->flag);
      //      if (myshim->flag == 0xdead){
	myshim->flag = 0;
	fprintf(dumpfd,"}\n");
	fflush(dumpfd);
	//	break;
	//      }    
	break;
    }
  }
  //mark global end counters.
  read_hwcounter(myshim->counter_end, myshim); 
}

