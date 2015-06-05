include ../../common.mk

objs = perf_event_launcher libperf_event_launcher.so libperf_event_reader.so libperf_event_shim.so shim_core.o

all: $(objs)

perf_event_launcher: perf_event_launcher.c
	g++ $(CFLAGS)  -fPIC perf_event_launcher.c -o perf_event_launcher -pthread -lpfm

libperf_event_launcher.so:perf_event_launcher_jni.c
	gcc $(CFLAGS) -std=c99 -fPIC -lpfm -shared perf_event_launcher_jni.c -o libperf_event_launcher.so -I$(JDK)/include -I$(JDK)/include/linux/

libperf_event_reader.so:perf_event_reader_jni.c
	gcc $(CFLAGS) -shared -std=c99 -fPIC perf_event_reader_jni.c -o libperf_event_reader.so -I$(JDK)/include -I$(JDK)/include/linux/ -lpfm

shim_core.o: ../../../shim_core.c
	gcc $(CFLAGS) -c $< -o $@ -pthread -lpfm -lrt

libperf_event_shim.so:perf_event_shim.c shim_core.o
	gcc $(CFLAGS) -shared -std=c99 -fPIC perf_event_shim.c ./shim_core.o -o $@ -I$(JDK)/include -I$(JDK)/include/linux/ -I../../../ -lpfm -lrt -pthread
#	gcc $(CFLAGS) -shared -std=c99 -fPIC perf_event_shim.c  -o $@ -I$(JDK)/include -I$(JDK)/include/linux/ -I../../../ -lpfm -lrt -pthread


.PHONY: clean
clean:
	-rm $(objs)
