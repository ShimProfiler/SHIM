#put your JDK path here
JDK=/opt/jdk1.6.0
#put your JikesRVMSHIM shimIP jar path here
JIKESRVMJAR=../../jikesrvmshim/dist/production_shimIP_x86_64-linux/jksvm.jar
JAVAC=$(JDK)/bin/javac
JAVA=$(JDK)/bin/java
#put your Dacapo benchmark directory here
BENCHMARKS=/usr/share/benchmarks
CFLAGS=-O2 -g  -D_GNU_SOURCE -fPIC
ifeq (-m32,$(findstring -m32,$(OPTION)))
M32_FLAG = y
CFLAGS += -m32
endif
