include ../../common.mk

all: perf_event_launcher libperf_event_launcher.so libperf_event_reader.so libperf_event_moma.so

perf_event_launcher: perf_event_launcher.c
	g++ $(CFLAGS)  -fPIC perf_event_launcher.c -o perf_event_launcher -pthread -lpfm

libperf_event_launcher.so:perf_event_launcher_jni.c
	gcc $(CFLAGS) -std=c99 -fPIC -lpfm -shared perf_event_launcher_jni.c -o libperf_event_launcher.so -I$(JDK)/include -I$(JDK)/include/linux/

libperf_event_reader.so:perf_event_reader_jni.c
	gcc $(CFLAGS) -shared -std=c99 -fPIC perf_event_reader_jni.c -o libperf_event_reader.so -I$(JDK)/include -I$(JDK)/include/linux/ -lpfm

libperf_event_moma.so:perf_event_moma.c
	gcc $(CFLAGS) -shared -std=c99 -fPIC perf_event_moma.c -o $@ -I$(JDK)/include -I$(JDK)/include/linux/ -lpfm -lrt


.PHONY: clean
clean:
	-rm perf_event_launcher libperf_event_launcher.so libperf_event_reader.so libperf_event_moma.so