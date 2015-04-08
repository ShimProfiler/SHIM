include common.mk
#usage: make [OPTION=-m32]. If OPTION=-m32 is specified and the current machine is X86_64, 32bit version will be build.

all: native-code probes.jar moma

##	cd native/valgrind; $(MAKE) $(MFLAGS) all
#	cp -a native/valgrind/libvalgrind.so libvalgrind.so
native-code:
	cd native/hello_world; $(MAKE) $(MFLAGS) all
	cp -a native/hello_world/libhello_world.so libhello_world.so
	cd native/perf_event; $(MAKE) $(MFLAGS) all
	cp -a native/perf_event/perf_event_launcher perf_event_launcher
	cp -a native/perf_event/libperf_event_launcher.so libperf_event_launcher.so
	cp -a native/perf_event/libperf_event_reader.so libperf_event_reader.so
	cp -a native/perf_event/libperf_event_moma.so libperf_event_moma.so
	cd native/ptlsim; $(MAKE) $(MFLAGS) all
	cp -a native/ptlsim/libptlsim.so libptlsim.so


test: all
	$(JAVA) -Djava.library.path=`pwd` -Dprobes=HelloWorldNative -cp probes.jar:/usr/share/benchmarks/dacapo/dacapo-2006-10-MR2.jar Harness -c probe.Dacapo2006Callback fop
	$(JAVA) -Dprobes=HelloWorld -cp probes.jar:/usr/share/benchmarks/dacapo/dacapo-2006-10-MR2.jar Harness -c probe.Dacapo2006Callback fop
	$(JAVA) -Dprobes=Scenario -cp probes.jar:/usr/share/benchmarks/dacapo/dacapo-9.12-bach.jar Harness -c probe.DacapoBachCallback fop
	cd /usr/share/benchmarks/SPECjvm98; $(JAVA) -Dprobes=HelloWorld -cp $(CURDIR)/probes.jar:. SpecApplication -i1 _202_jess
	$(JAVA) -Xmx1G -Xms200M -Dprobes=HelloWorld -cp probes.jar:/usr/share/benchmarks/pjbb2005/jbb.jar:/usr/share/benchmarks/pjbb2005/check.jar spec.jbb.JBBmain -propfile SPECjbb-8x10000.props -c probe.PJBB2005Callback -n 1
	./perf_event_launcher PERF_COUNT_HW_CPU_CYCLES $(JAVA) -Djava.library.path=`pwd` -Dprobes=PerfEventLauncher -cp probes.jar:/usr/share/benchmarks/dacapo/dacapo-2006-10-MR2.jar Harness -c probe.Dacapo2006Callback fop
	$(JAVA) -Djava.library.path=`pwd` -Dprobes=PTLsim -cp probes.jar:/usr/share/benchmarks/dacapo/dacapo-2006-10-MR2.jar Harness -c probe.Dacapo2006Callback fop
	$(JAVA) -Djava.library.path=`pwd` -Dprobes=Valgrind -cp probes.jar:/usr/share/benchmarks/dacapo/dacapo-2006-10-MR2.jar Harness -c probe.Dacapo2006Callback fop
	$(JAVA) -Djava.library.path=`pwd` -Dprobes=StopJIT -Dprobe.stopjit.sleeptime=3 -Dprobe.stopjit.iteration=4 -cp probes.jar:/usr/share/benchmarks/dacapo/dacapo-2006-10-MR2.jar Harness -c probe.Dacapo2006Callback -n 5 fop


clean:
	rm -f *.so
	rm -f *.class
	rm -f *.jar
	rm -f spec/harness/*.class
	rm -f probe/*.class
	rm -f moma/*.class
	cd native/perf_event; $(MAKE) $(MFLAGS) clean
	cd native/hello_world; $(MAKE) $(MFLAGS) clean
	cd native/ptlsim; $(MAKE) $(MFLAGS) clean
	cd native/valgrind; $(MAKE) $(MFLAGS) clean

probes.jar: probe/HelloWorldNativeProbe.class probe/ScenarioProbe.class probe/StopJITProbe.class probe/ValgrindProbe.class probe/PTLsimProbe.class probe/JikesRVMPerfEventProbe.class probe/MMTkProbe.class probe/ThreadEventProbe.class probe/PowerProbe.class probe/PerfEventProbe.class probe/PerfEventReader.class probe/PerfEventLauncherProbe.class probe/ApplicationPerfEventProbe.class probe/HelloWorldProbe.class probe/Probe.class probe/ProbeMux.class probe/PJBB2005Callback.class probe/Dacapo2006Callback.class probe/DacapoBachCallback.class spec/harness/ProgramRunner.class spec/harness/RunProgram.class SpecApplication.class probe/HooverProbe.class probe/ReplayProbe.class probe/CompilerSamplesProbe.class probe/MomaProbe.class moma/MomaThread.class moma/MomaCallBack.class moma/Util.class moma/Disassemble.class probe/ShellProbe.class
	zip -r probes.jar *.class spec/harness/*.class probe/*.class moma/*.class

SpecApplication.class: SpecApplication.java
	$(JAVAC) -implicit:none -cp .:/usr/share/benchmarks/SPECjvm98 -sourcepath . $<

spec/%.class: spec/%.java
	$(JAVAC) -implicit:none -cp .:/usr/share/benchmarks/SPECjvm98 -sourcepath . $<

moma/%.class: moma/%.java
	$(JAVAC) -cp .:/home/yangxi/code/jike/jikemoma/dist/production_momaIP_x86_64-linux/jksvm.jar -sourcepath . $<
probe/ReplayProbe.class:probe/ReplayProbe.java
	$(JAVAC) -cp .:/home/yangxi/code/jike/jikemoma/dist/production_momaIP_x86_64-linux/jksvm.jar -sourcepath . $<
probe/MMTkProbe.class:probe/MMTkProbe.java
	$(JAVAC) -cp .:/home/yangxi/code/jike/jikemoma/dist/production_momaIP_x86_64-linux/jksvm.jar -sourcepath . $<
probe/MomaProbe.class: probe/MomaProbe.java
	$(JAVAC) -cp .:/home/yangxi/code/jike/jikemoma/dist/production_momaIP_x86_64-linux/jksvm.jar -sourcepath . $<
probe/ShellProbe.class: probe/ShellProbe.java
	$(JAVAC) -cp .:/home/yangxi/code/jike/jikemoma/dist/production_momaIP_x86_64-linux/jksvm.jar -sourcepath . $<
 
%.class: %.java
	$(JAVAC) -cp . $<

probe/Dacapo2006Callback.class:
	$(JAVAC) -cp .:/usr/share/benchmarks/dacapo/dacapo-2006-10-MR2.jar probe/Dacapo2006Callback.java

probe/DacapoBachCallback.class:
	$(JAVAC) -cp .:/usr/share/benchmarks/dacapo/dacapo-9.12-bach.jar probe/DacapoBachCallback.java

probe/PJBB2005Callback.class:
	$(JAVAC) -cp .:/usr/share/benchmarks/pjbb2005/jbb.jar probe/PJBB2005Callback.java
