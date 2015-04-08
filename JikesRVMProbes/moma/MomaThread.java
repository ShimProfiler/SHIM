package moma;


import org.jikesrvm.Options;

import static org.jikesrvm.runtime.SysCall.sysCall;

import org.jikesrvm.runtime.Entrypoints;
import org.jikesrvm.runtime.Magic;
import org.jikesrvm.scheduler.RVMThread;
import org.vmmagic.unboxed.Address;
import probe.MomaProbe;

/**
 * Created by xiyang on 3/09/2014.
 */
public class MomaThread extends Thread {

  //Address are used for asking shim thread to stop profiling
  public Address nativeFlag;

  //init global shims
  //returns the pid_to_signal buffer from kernel driver
  private static native int initShim(int fpOffset,int statusOffset);
  //init thread local shim thread
  //return the address as naiveFlag;
  private static native int initEvents(int nrEvents, String fileName, int flag);
  //create shim thread local counter
  private static native int createHardwareEvent(int id, String eventName);
  //update some information before start profiling
  private static native void updateRate(int rate, int targetcpu, int offset);
  //two different profiling approaches
  private static native void sampling(int phase, int iteration);
  private static native void ipc(int iteration);
  private static native void counting(int phase);
  private static native void histogram(int flag);
  //used by histogram to reset compare the accuracy of collected histogram
  public static native void resetHistogram(int cpu);
  public static native void reportHistogram(int cpu);
  public static native void setFrequency(String newfreq);
  public static native String maxFrequency();
  public static native String minFrequency();
  public static native void resetCounting(int cpu);
  public static native void reportCounting(int cpu);
  public static native void reportShimStat(int cpu);

  //which CPU this shim thread is running on
  public int cpuid;
  //which CPU this shim thread is going to measure
  public int targetcpu;

  //what kind of profiling approach this shim thread is going to do
  public int phase;

  public int countPhase = 0;
  public int iteration = 1;

  //hardware counters information from momaEvents option
  private int eventIndexes[];
  private int numberHardwareEvents = 0;
  private String eventNames[];

  //what's this shim thread's running state
  public static final int MOMA_RUNNING = 1;
  public static final int MOMA_WAIT = 2;
  public volatile int state;

  //private RVMThread targetThread;
  //private PrintStream dumpFile;

  //what's the sampling rate
  public int samplingRate = 0;

  //public static boolean dumpCMID = false;

  public static int tidToMomaEvent;

  static {
    //System.out.println("java.library.path: " + System.getProperty("java.library.path"));
    System.loadLibrary("perf_event_moma");
    tidToMomaEvent = initShim(Entrypoints.momaFramePointerField.getOffset().toInt(), Entrypoints.execStatusField.getOffset().toInt());
  }



  public MomaThread(int id, int approach, int rate)
  {
    cpuid = id;
    phase = approach;
    samplingRate = rate;
  }

  public void init() {
    //bind this shim thread to where it belongs to
    sysCall.sysCall.sysThreadBind(Magic.getThreadRegister().pthread_id, 1<<cpuid);

    //setup counters
    if (Options.MomaEvents != null) {
      eventNames = Options.MomaEvents.split(",");
      numberHardwareEvents = eventNames.length;
      eventIndexes = new int[numberHardwareEvents];
    }

    nativeFlag = Address.fromIntSignExtend(initEvents(numberHardwareEvents,"/tmp/"+ this.getName(), phase));

    for (int i = 0; i < numberHardwareEvents; i++) {
      eventIndexes[i] = createHardwareEvent(i, eventNames[i]);
    }
  }


  public void profile() {
    while(true){
      //goto sleep
//      System.out.println("Shim"+cpuid + " going to sleep");
      synchronized (this) {
        System.out.println("Shim"+cpuid + " is sleeping");
        try {
          state = MOMA_WAIT;
          this.wait();
        } catch (Exception e) {
          System.out.println(e);
        }
      }
      state = MOMA_RUNNING;
      //get some work to do
  //    System.out.println("Shim"+cpuid + " start sampling");

      updateRate(samplingRate, targetcpu, RVMThread.momaOffset);
      if (phase == MomaProbe.MOMA_APPROACH_HISTOGRAM) {
        histogram(phase);
      }else if (phase == MomaProbe.MOMA_APPROACH_COUNTING) {
        counting(countPhase);
      }else if (phase == MomaProbe.MOMA_APPROACH_LOGGING){
        sampling(1|2, iteration);
      }else if (phase == MomaProbe.MOMA_APPROACH_IPC) {
        ipc(iteration++);
      }
      //processing data
    }
  }

  public void run() {
    state = MOMA_RUNNING;
    init();
    //won't back for ever
    profile();
  }

  public void suspendMoma() {
    System.out.println("Stopping shim at CPU " + cpuid );
    nativeFlag.store(0xdead);
  }
}

//    // new buffer
//            for (RVMThread t : RVMThread.threads) {
//      if (t != null)
//        System.out.println("Thread " + t.getName());
//      if (t != null && t.getName().equals("MainThread")) {
//        targetThread = t;
//        //System.out.println("Pin MainThread to 0x1, pin myself to 0x10");
//        //sysCall.sysCall.sysThreadBind(t.pthread_id, 2<<cpuid);
//        //System.out.println("Mainthread's mask is " + sysCall.sysCall.sysGetThreadBindSet(t.pthread_id));
//        //sysCall.sysCall.sysThreadBind(Magic.getThreadRegister().pthread_id, 2<<(cpuid + 4));
//        //System.out.println("myself mask is  " + sysCall.sysCall.sysGetThreadBindSet(Magic.getThreadRegister().pthread_id));
//
//      }
//    }
//    int sampleAddress = ObjectReference.fromObject(targetThread).toAddress().plus(Entrypoints.momaAppEventField.getOffset()).toInt();
//    if(samplingApproach == 1) {
//      //pass the address of framepointer
//      sampleAddress = ObjectReference.fromObject(targetThread).toAddress().plus(Entrypoints.momaFramePointerField.getOffset()).toInt();
//    }
//    int targetcpu = pidAddress + (cpuid + 4) * 64;


//    if (dumpCMID == false)
//      return;
//
//
//    //we need to generate the dictionary to show the semantic knowledge of logged tag
//    FileInputStream dumpstream;
//    BufferedReader dumprd;
//    PrintStream dictstream;
//    try {
//      dumpstream = new FileInputStream("/tmp/dumpMoma");
//      dumprd = new BufferedReader(new InputStreamReader(dumpstream));
//      dictstream = new PrintStream("/tmp/momaDict");
//      Map cmidDict = new HashMap();
//      String dumpline;
//      Pattern p = Pattern.compile("(\"cmid\":)([-0-9]*)");
//      while ((dumpline = dumprd.readLine()) != null) {
//        Matcher m = p.matcher(dumpline);
//        if (m.find() == false) {
//          continue;
//        }
//        int cmid = Integer.parseInt(m.group(2));
//        if (cmid < 0)
//          cmid = -cmid;
//
//        Integer key = Integer.valueOf(cmid);
//        if (cmidDict.containsKey(key))
//          continue;
//
//        cmidDict.put(key, 1);
//
//        if (cmid > 0 && cmid <= CompiledMethods.currentCompiledMethodId) {
//          RVMMethod method = CompiledMethods.getCompiledMethod(cmid).method;
//          if (method != null) {
//            String className = method.getDeclaringClass().toString();
//            String methodName = method.getName().toString();
//            dictstream.println(cmid + ": " + className + " " + methodName + " " + method.getCurrentCompiledMethod().getEntryCodeArray().length());
//          }
//        }
//      }
//      dumpstream.close();
//      dictstream.close();
//    }catch(Exception e){
//      System.out.println("Caught exception while generating dict,msg: " + e);
//    }