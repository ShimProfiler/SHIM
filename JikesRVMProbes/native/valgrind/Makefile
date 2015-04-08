include ../../common.mk

all: libvalgrind.so 

libvalgrind.so: valgrind.o
	gcc $(CFLAGS) -pthread -shared -o libvalgrind.so valgrind.o -lc

valgrind.o: valgrind.c
	gcc $(CFLAGS) -pthread -c valgrind.c -I$(JDK)/include -I$(JDK)/include/linux/

clean:
	rm -rf *.o *.so

test: default
	$(JAVA) -Djava.library.path=`pwd`/valgrind -cp .:$(BENCHMARKS)/dacapo/dacapo-9.12-bach.jar Harness -c callback.DacapoBachCallback avrora
