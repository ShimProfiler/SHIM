#include <jni.h>
#include <stdio.h>

/* JNI Functions */
JNIEXPORT void JNICALL Java_probe_PTLsimProbe_begin
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("We would call switch to sim now (benchmark: %s, iteration: %d, warmup %d)\n", bm, iteration, warmup);
}
JNIEXPORT void JNICALL Java_probe_PTLsimProbe_end
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("We would call switch to native now (benchmark: %s, iteration: %d, warmup %d)\n", bm, iteration, warmup);
}
JNIEXPORT void JNICALL Java_probe_PTLsimProbe_report
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("We could print results now (benchmark: %s, iteration: %d, warmup %d)\n", bm, iteration, warmup);
}
