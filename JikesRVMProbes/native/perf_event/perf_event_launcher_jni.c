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

JNIEXPORT void JNICALL 
Java_probe_PerfEventLauncherProbe_enable(JNIEnv *env, jclass cls, jint fd)
{
  if (ioctl( fd, PERF_EVENT_IOC_ENABLE) == -1)
    err(1, "error in prctl(PR_TASK_PERF_EVENTS_ENABLE)");
}

JNIEXPORT void JNICALL 
Java_probe_PerfEventLauncherProbe_disable(JNIEnv *env, jclass cls, jint fd)
{
  if (ioctl( fd, PERF_EVENT_IOC_DISABLE) == -1)
    err(1, "error in prctl(PR_TASK_PERF_EVENTS_ENABLE)");
}

JNIEXPORT void JNICALL 
Java_probe_PerfEventLauncherProbe_reset(JNIEnv *env, jclass cls, jint fd)
{
  if (ioctl( fd, PERF_EVENT_IOC_RESET) == -1)
    err(1, "error in prctl(PR_TASK_PERF_EVENTS_ENABLE)");
}

JNIEXPORT void JNICALL
Java_probe_PerfEventLauncherProbe_read(JNIEnv *env, jclass cls, jint fd, jlongArray result)
{
  uint64_t buf[3];
  size_t expectedBytes = 3 * sizeof(uint64_t);

  int ret = read(fd, buf, expectedBytes);
  if (ret < 0) {
    err(1, "error reading event: %s", strerror(errno));
  }
  if (ret != expectedBytes) {
    errx(1, "read of perf event did not return 3 64-bit values");
  }

  jlong* r = (*env)->GetLongArrayElements(env,result, NULL);
  
  for (int i=0; i<3; i++)
    r[i] = buf[i];

  (*env)->ReleaseLongArrayElements(env,result, r, 0);
}

