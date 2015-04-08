#include <jni.h>
#include <stdio.h>

/* JNI Functions */
JNIEXPORT void JNICALL Java_probe_HelloWorldNativeProbe_begin
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("HelloWorldNativeProbe_begin.(benchmark = %s, iteration = %d, warmup = %d)\n", bm, iteration, warmup);
}
JNIEXPORT void JNICALL Java_probe_HelloWorldNativeProbe_end
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("HelloWorldNativeProbe_end.(benchmark = %s, iteration = %d, warmup = %d)\n", bm, iteration, warmup);
}
JNIEXPORT void JNICALL Java_probe_HelloWorldNativeProbe_report
  (JNIEnv *env, jobject o, jstring benchmark, jint iteration, jboolean warmup) {
  jboolean iscopy;
  const char *bm = (*env)->GetStringUTFChars(env, benchmark, &iscopy);
  printf("HelloWorldNativeProbe_report.(benchmark = %s, iteration = %d, warmup = %d)\n", bm, iteration, warmup);
}
