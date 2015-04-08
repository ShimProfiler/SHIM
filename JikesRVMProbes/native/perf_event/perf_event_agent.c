/*
 * self.c - example of a simple self monitoring task
 *
 * Copyright (c) 2009 Google, Inc
 * Contributed by Stephane Eranian <eranian@gmail.com>
 *
 * Based on:
 * Copyright (c) 2002-2007 Hewlett-Packard Development Company, L.P.
 * Contributed by Stephane Eranian <eranian@hpl.hp.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
 * of the Software, and to permit persons to whom the Software is furnished to do so,
 * subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A
 * PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 * CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE
 * OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 * This file is part of libpfm, a performance monitoring support library for
 * applications on Linux.
 */

#include <sys/types.h>
#include <sys/ioctl.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <signal.h>
#include <locale.h>
#include <err.h>
#include <pthread.h>

#include <perfmon/pfmlib_perf_event.h>
#include "perf_util.h"

#include "jvmti.h"
#include "jni.h"


/*static const char *gen_events[]={
  "PERF_COUNT_HW_CPU_CYCLES",
  "PERF_COUNT_HW_INSTRUCTIONS",
  NULL
};*/

static perf_event_desc_t* perfevent_fds = NULL;
static int                perfevent_num_fds = 0;
static char*              perfevent_events = NULL;


void perfevent_initialize(char*events) {
  int i, ret;
  perfevent_events = strdup(events);

  /*
   * Initialize pfm library (required before we can use it)
   */
  ret = pfm_initialize();
  if (ret != PFM_SUCCESS)
    errx(1, "Cannot initialize library: %s", pfm_strerror(ret));

  ret = perf_setup_list_events(events, &perfevent_fds, &perfevent_num_fds);
  if (ret || !perfevent_num_fds)
    errx(1, "cannot setup events");

  perfevent_fds[0].fd = -1;
  for(i=0; i < perfevent_num_fds; i++) {
    /* request timing information necessary for scaling */
    perfevent_fds[i].hw.read_format = PERF_FORMAT_SCALE;

    perfevent_fds[i].hw.disabled = (i == 0); /* do not start now */
    perfevent_fds[i].hw.inherit = 1; /* pass on to child threads */

    /* each event is in an independent group (multiplexing likely) */
    perfevent_fds[i].fd = perf_event_open(&perfevent_fds[i].hw, 0, -1, perfevent_fds[0].fd, 0);
    if (perfevent_fds[i].fd == -1)
      err(1, "cannot open event %d", i);
  }
}
void perfevent_enable() {
  int ret;
  /*
   * start counting now
   */
  ret = ioctl(perfevent_fds[0].fd, PERF_EVENT_IOC_ENABLE, 0);
  if (ret)
    err(1, "ioctl(enable) failed");
}
void perfevent_disable() {
  int ret;
  /*
   * stop counting
   */
  ret = ioctl(perfevent_fds[0].fd, PERF_EVENT_IOC_DISABLE, 0);
  if (ret)
    err(1, "ioctl(disable) failed");
}
void perfevent_reset() {
  int i, ret;
  /*
   * stop counting
   */
  for(i=0; i < perfevent_num_fds; i++) {
    ret = ioctl(perfevent_fds[i].fd, PERF_EVENT_IOC_RESET, 0);
    if (ret)
      err(1, "ioctl(reset) failed");
  }
}

void perfevent_read() {
  int i, ret;
  uint64_t values[3];
  /*
   * now read the results. We use pfp_event_count because
   * libpfm guarantees that counters for the events always
   * come first.
   */
  memset(values, 0, sizeof(values));

  fprintf(stderr, "============================ Tabulate Statistics ============================\n");
  for (i=0; i < perfevent_num_fds; i++) {
    fprintf(stderr, "%s	", perfevent_fds[i].name);
  }
  fprintf(stderr, "\n");
  for (i=0; i < perfevent_num_fds; i++) {
    uint64_t val;
    double ratio;

    ret = read(perfevent_fds[i].fd, values, sizeof(values));
    if (ret < sizeof(values)) {
      if (ret == -1)
        err(1, "cannot read results: %s", strerror(errno));
      else
        warnx("could not read event%d", i);
    }
    /*
     * scaling is systematic because we may be sharing the PMU and
     * thus may be multiplexed
     */
    val = perf_scale(values);
    ratio = perf_scale_ratio(values);

    if (ratio == 1.0)
      fprintf(stderr, "%lld	", (long long int) val);
    else
      if (ratio == 0.0)
        fprintf(stderr, "NO_VALUE	");
      else
        fprintf(stderr, "%lld-SCALED-%.2f%%	", (long long int) val, ratio*100.0);
  }
  fprintf(stderr, "\n=============================================================================\n");
}
void perfevent_cleanup() {
  int i;
  for (i=0; i < perfevent_num_fds; i++) {
    close(perfevent_fds[i].fd);
  }
  free(perfevent_fds);
  perfevent_fds = NULL;
  perfevent_num_fds = 0;
}

/* JVMTI Functions */
JNIEXPORT jint JNICALL
Agent_OnLoad(JavaVM *vm, char *options, void *reserved) {
  perfevent_initialize(options);
  return JNI_OK;
}
JNIEXPORT void JNICALL
Agent_OnUnload(JavaVM *vm) {
  perfevent_cleanup();
}

/* JNI Functions */
JNIEXPORT void JNICALL Java_probe_PerfEventProbe_begin
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  perfevent_enable();
}
JNIEXPORT void JNICALL Java_probe_PerfEventProbe_end
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  perfevent_disable();
}
JNIEXPORT void JNICALL Java_probe_PerfEventProbe_report
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  perfevent_read();
  perfevent_reset();
}
JNIEXPORT void JNICALL Java_probe_PerfEventProbe_reinitialize
  (JNIEnv *env, jobject o) {
  perfevent_cleanup();
  perfevent_initialize(perfevent_events);
}
