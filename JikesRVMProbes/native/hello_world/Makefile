include ../../common.mk

all: libhello_world.so

libhello_world.so: hello_world.o
	gcc $(CFLAGS) -pthread -shared -o libhello_world.so hello_world.o -lc

hello_world.o: hello_world.c
	gcc $(CFLAGS) -pthread -c hello_world.c -I$(JDK)/include -I$(JDK)/include/linux/


clean:
	rm -rf *.o *.so

test: default
	$(JAVA) -Djava.library.path=`pwd`/hello_world -cp .:$(BENCHMARKS)/dacapo/dacapo-9.12-bach.jar Harness -c callback.DacapoBachCallback avrora
