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
//RAPL
#define RAPL_ENERGY_UNIT_MASK   0x1F00
#define RAPL_ENERGY_UNIT_OFFSET 0x08
#define RAPL_POWER_UNIT         0x606
#define RAPL_PACKAGE_ENERGY     0x611
#define RAPL_CORE_ENERGY        0x639
#define RAPL_UNCORE_ENERGY      0x641
#define RAPL_DRAM_ENERGY        0x619

static int msr_fd;

static jlong readRAPL(unsigned int reg){
  jlong data;
  if (msr_fd != -1)
    if (lseek(msr_fd, reg, SEEK_SET) == reg)
      if (read(msr_fd, &data, sizeof(data)) == sizeof(data))
	return data;

  return 0;
}

JNIEXPORT jlong JNICALL
Java_probe_PerfEventReader_raplInit(JNIEnv *env, jobject obj)
{
  jlong mask;
  msr_fd = open("/dev/cpu/0/msr", O_RDONLY);

  if(msr_fd != -1)
    if (lseek(msr_fd, RAPL_POWER_UNIT, SEEK_SET) == RAPL_POWER_UNIT)
      if (read(msr_fd,&mask,sizeof(mask)) == sizeof(mask))
	return 1<< ((mask & RAPL_ENERGY_UNIT_MASK) >> RAPL_ENERGY_UNIT_OFFSET);

  return 0;
}

JNIEXPORT jlong JNICALL
Java_probe_PerfEventReader_raplPackage(JNIEnv *env, jobject obj)
{
  return readRAPL(RAPL_PACKAGE_ENERGY);
}

JNIEXPORT jlong JNICALL
Java_probe_PerfEventReader_raplCore(JNIEnv *env, jobject obj)
{
  return readRAPL(RAPL_CORE_ENERGY);
}

JNIEXPORT jlong JNICALL
Java_probe_PerfEventReader_raplUncore(JNIEnv *env, jobject obj)
{
  return readRAPL(RAPL_UNCORE_ENERGY);
}

JNIEXPORT jlong JNICALL
Java_probe_PerfEventReader_raplDram(JNIEnv *env, jobject obj)
{
  return readRAPL(RAPL_DRAM_ENERGY);
}



JNIEXPORT void JNICALL
Java_probe_PerfEventReader_init(JNIEnv *env, jobject obj)
{
  int ret = pfm_initialize();
  if (ret != PFM_SUCCESS) {
    errx(1, "error in pfm_initialize: %s", pfm_strerror(ret));
  }
}

JNIEXPORT jint JNICALL
Java_probe_PerfEventReader_create(JNIEnv * env, jobject obj, jstring jeventName)
{
  pfm_perf_encode_arg_t raw;
  struct perf_event_attr peattr;
  struct perf_event_attr *pe = &peattr;
  memset(&raw, 0, sizeof(pfm_perf_encode_arg_t));
  memset(pe, 0, sizeof(struct perf_event_attr));
  raw.attr = pe;
  raw.size = sizeof(pfm_perf_encode_arg_t);
  const char * eventName = (*env)->GetStringUTFChars(env, jeventName, 0);
  int ret = pfm_get_os_event_encoding(eventName, PFM_PLM3, PFM_OS_PERF_EVENT, &raw);
  if (ret != PFM_SUCCESS) {
    errx(1, "error creating event '%s': %s\n", eventName, pfm_strerror(ret));
  }
  pe->read_format = PERF_FORMAT_TOTAL_TIME_ENABLED | PERF_FORMAT_TOTAL_TIME_RUNNING;
  pe->disabled = 0;
  pe->inherit = 1;
  int fd = perf_event_open(pe, 0, -1, -1, 0);
  if (fd == -1) {
    err(1, "error in perf_event_open for event %d '%s'", fd, eventName);
  }
  (*env)->ReleaseStringUTFChars(env,jeventName, eventName);
  return fd;
}

JNIEXPORT jlong JNICALL
Java_probe_PerfEventReader_read(JNIEnv * envint , jobject obj, jint jfd){
  int fd = jfd;
  size_t expectedBytes = 3 * sizeof(long long);
  long long values[3];
  int ret = read(fd, values, expectedBytes);
  if (ret < 0) {
    err(1, "error reading event: %s", strerror(errno));
  }
  if (ret != expectedBytes) {
    errx(1, "read of perf event did not return 3 64-bit values");
  }
  return (jlong)(values[0]);
}
