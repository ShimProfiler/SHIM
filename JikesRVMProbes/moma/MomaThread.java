package moma;


import org.jikesrvm.Options;

import static org.jikesrvm.runtime.SysCall.sysCall;

import org.jikesrvm.compilers.common.CompiledMethods;
import org.jikesrvm.runtime.Entrypoints;
import org.jikesrvm.runtime.Magic;
import org.jikesrvm.scheduler.RVMThread;
import org.vmmagic.unboxed.Address;
import probe.MomaProbe;
import static probe.MomaProbe.ProfilingApproach.*;

/**
 * Created by xiyang on 3/09/2014.
 */
public class MomaThread extends Thread {

  //shared by all shim threads
  public static final int fpOffset = Entrypoints.momaFramePointerField.getOffset().toInt();
  public static final int execStateOffset = Entrypoints.execStatusField.getOffset().toInt();
  public static final int cmidOffset = Entrypoints.momaAppEventField.getOffset().toInt();

  public static String maxCPUFreq;
  public static String minCPUFreq;


  //what's this shim thread's running state
  public static final int MOMA_RUNNING = 1;
  public static final int MOMA_STANDBY = 2;
  public volatile int state;

  public int shimid;
  public int workingHWThread;
  public int targetHWThread;

  //Address are used for asking shim thread to stop profiling
  public Address controlFlag;

  private static native void initShimProfiler(int numberShims, int fpOffset, int execStatOffset, int cmidOffset);
  private static native int initShimThread(int cpuid, String[] events, int targetcpu, String outputFileName);
  private static native void shimCounting();
  private static native void shimEventHistogram(int samplingRate);
  private static native void shimCMIDHistogram(int samplingRate, int maxCMID);

  private static native String getMaxFrequency();
  private static native String getMinFrequency();
  private static native void setCurFrequency(String newFreq);



  //which CPU this shim thread is running on

  static {
    System.loadLibrary("perf_event_shim");
    initShimProfiler(MomaProbe.maxCoreNumber, fpOffset, execStateOffset, cmidOffset);
    maxCPUFreq = getMaxFrequency();
    minCPUFreq = getMinFrequency();
  }

  public MomaThread(int id, int workingCore, int targetCore)
  {
    System.out.println(" New jshim thread " + id + " running at cpu " + workingCore + " target cpu " + targetCore);
    shimid = id;
    workingHWThread = workingCore;
    targetHWThread = targetCore;
    state = MOMA_STANDBY;
  }

  public void run() {
    initThisThread();
    profile();
  }

  public void initThisThread() {
    System.out.println(" init jshim thread " + workingHWThread + "target thread" + targetHWThread);
    String[] events = Options.MomaEvents.split(",");
    for (String str :events){
      System.out.println(str);
    }
    controlFlag = Address.fromIntSignExtend(initShimThread(workingHWThread, events, targetHWThread, "/tmp/"+this.getName()));
  }


  public void profile() {
    int nr_iteration = 0;
    while (true) {
      synchronized (this) {
        try {
          this.wait();
        } catch (Exception e) {
          System.out.println(e);
        }
      }
      state = MOMA_RUNNING;
      //get some work to do
      System.out.println("Shim" + shimid + " start sampling");
      switch (MomaProbe.shimHow) {
        case COUNTING:
          shimCounting();
          break;
        case EVENTHISTOGRAM:
          shimEventHistogram(MomaProbe.samplingRate);
          break;
        case CMIDHISTOGRAM:
          System.out.println("Current last CMID " + CompiledMethods.currentCompiledMethodId);
//          if ((nr_iteration & 0x1) == 0) {
//            System.out.println("Set CPU frequency to " + maxCPUFreq);
//            setCurFrequency(maxCPUFreq);
//          }else {
//            System.out.println("Set CPU frequency to " + minCPUFreq);
//            setCurFrequency(minCPUFreq);
//          }
          nr_iteration += 1;
          shimCMIDHistogram(MomaProbe.samplingRate, CompiledMethods.currentCompiledMethodId);
          break;
      }
      state = MOMA_STANDBY;
    }
  }

  public void suspendMoma() {
    System.out.println("Stop shim" + shimid);
    controlFlag.store(0xdead);
  }

  public void resetControlFlag(){
    controlFlag.store(0x0);
  }
}
