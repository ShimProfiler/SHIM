include ../../common.mk

all: libptlsim.so 

libptlsim.so: ptlsim.o
	gcc $(CFLAGS) -pthread -shared -o libptlsim.so ptlsim.o -lc

ptlsim.o: ptlsim.c
	gcc $(CFLAGS) -pthread -c ptlsim.c -I$(JDK)/include -I$(JDK)/include/linux/

clean:
	rm -rf *.o *.so

test: default
	$(JAVA) -Djava.library.path=`pwd`/ptlsim -cp .:$(BENCHMARKS)/dacapo/dacapo-9.12-bach.jar Harness -c callback.DacapoBachCallback avrora
