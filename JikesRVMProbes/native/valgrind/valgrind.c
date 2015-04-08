#include <jni.h>
#include <stdio.h>
#include "cachegrind.h"

/* JNI Functions */
JNIEXPORT void JNICALL Java_probe_ValgrindProbe_begin
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("ValgrindProbe_begin(benchmark = %s, iteration = %d, warmup = %d)\n", bm, iteration, warmup);
  CACHEGRIND_PROBE_BEGIN(iteration, warmup);
}
JNIEXPORT void JNICALL Java_probe_ValgrindProbe_end
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("ValgrindProbe_end(benchmark = %s, iteration = %d, warmup = %d)\n", bm, iteration, warmup);
  CACHEGRIND_PROBE_END(iteration, warmup);
}
JNIEXPORT void JNICALL Java_probe_ValgrindProbe_report
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("ValgrindProbe_report(benchmark = %s, iteration = %d, warmup = %d)\n", bm, iteration, warmup);
  CACHEGRIND_PROBE_REPORT(iteration, warmup);
}
