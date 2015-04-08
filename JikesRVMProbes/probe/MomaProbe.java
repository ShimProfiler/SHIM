package probe;


import moma.MomaThread;
import org.jikesrvm.Options;
import org.jikesrvm.VM;

/**
 * Created by xiyang on 1/09/2014.
 */

public class MomaProbe implements Probe {

  //our prototype only runs on haswell which has 4 cores 8 threads total
  public static final int NR_CPU = 4;

  //each core has one corresponding shim working thread
  public static MomaThread[] shims;

  //sampling rate we need
  public int rate;

  public boolean changeFrequency;
  public String[] freqs;

  //what we are going to measure
  public static final int MOMA_APPROACH_HISTOGRAM = 1;
  public static final int MOMA_APPROACH_LOGGING = 2;
  public static final int MOMA_APPROACH_COUNTING = 3;
  public static final int MOMA_APPROACH_IPC = 4;
  public static int counting_phase = 0;
  int momaApproach = 0;

  //where we are going to measure
  public static final int MOMA_LOCAL = 1;
  public static final int MOMA_REMOTE = 2;
  public static final int MOMA_BOTH = 3;
  public static final int MOMA_NONE = 4;
  int momaStation = 0;


  public void init(){
    if (Options.MomaApproach == null)
      return;

    //momaApproach should be "[remote|same],[histogram|logging|xxx],rate"
    String[] ops = Options.MomaApproach.split(",");
    String cmd = ops[0];
    //where we do the measurment
    if (cmd.equals("remote")){
      momaStation = MOMA_REMOTE;
    }else if (cmd.equals("same")){
      momaStation = MOMA_LOCAL;
    }else if (cmd.equals("all")) {
      momaStation = MOMA_BOTH;
    }else if (cmd.equals("none")){
      momaStation = MOMA_NONE;
    }else{
      System.out.println("momaApproach option is not in right format " + Options.MomaApproach);
    }

    //what we are going to measure
    String n = ops[1];
    if (n.equals("histogram")){
      momaApproach = MOMA_APPROACH_HISTOGRAM;
    }else if (n.equals("logging")){
      momaApproach = MOMA_APPROACH_LOGGING;
    }else if (n.equals("hardonly")) {
      momaApproach = MOMA_APPROACH_COUNTING;
      //hardonly
      counting_phase = 2;
    }else if (n.equals("softonly")) {
      momaApproach = MOMA_APPROACH_COUNTING;
      //software only
      counting_phase = 1;
    }else if (n.equals("IPC")) {
      momaApproach = MOMA_APPROACH_IPC;
    }else{
      System.out.println("momaApproach option is not in right format " + Options.MomaApproach);
    }

    rate = Integer.parseInt(ops[2]);

    if (ops.length > 3) {
      changeFrequency = true;
      freqs = new String[2];
      freqs[0] = MomaThread.maxFrequency();
      freqs[1] = MomaThread.minFrequency();
      System.out.println("SHIM will switch frequencies between " + freqs[0] + " and " + freqs[1]);
    }

    shims = new MomaThread[NR_CPU];
    for (int i = 0; i < NR_CPU; i++) {
      shims[i] = new MomaThread(i, momaApproach, rate);
      shims[i].setName("shim" + i);
      shims[i].start();
    }
  }

  private void waitForShims(){
    System.out.println("hello i am in wait for shims");
    try {
      for (MomaThread t : shims) {
        //suspend the shim thread if it is running.
        if (t.state == t.MOMA_RUNNING) {
          VM.sysWriteln("SUSPEND SHIM", t.cpuid);
          t.suspendMoma();
          while (t.state == t.MOMA_RUNNING)
            ;
          MomaThread.reportShimStat(t.cpuid);
        }
      }
    }catch (Exception e){
      System.out.println("Exception is happened when asking shim threads to stop e:" + e);
    }
  }

  public void cleanup() {
    System.err.println("MomaProbe.cleanup()");
  }

  public void begin(String benchmark, int iteration, boolean warmup) {
    System.out.println("MomaProbe.begin(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
    if (iteration < 3)
      return;

    if (Options.MomaApproach == null)
      return;


    if (momaApproach == MOMA_APPROACH_HISTOGRAM) {
      if (momaStation == MOMA_REMOTE) {
        MomaThread t1 = shims[0];
        t1.targetcpu = 7;
        t1.phase = MOMA_APPROACH_HISTOGRAM;
        synchronized (t1) {
          t1.notifyAll();
        }
      } else if(momaStation == MOMA_LOCAL){
        MomaThread t1 = shims[3];
        t1.targetcpu = 7;
        t1.phase = MOMA_APPROACH_HISTOGRAM;
        synchronized (t1) {
          t1.notifyAll();
        }
      } else if (momaStation == MOMA_BOTH){
        MomaThread t1 = shims[0];
        t1.targetcpu = 7;
        t1.phase = MOMA_APPROACH_HISTOGRAM;
        MomaThread t2 = shims[3];
        t2.targetcpu = 7;
        //MomaThread.resetHistogram(shimid);
        t2.phase = MOMA_APPROACH_HISTOGRAM;

        synchronized (t1) {
          t1.notifyAll();
        }

        synchronized (t2) {
          t2.notifyAll();
        }
      }
      // we may go to native to clear both mutator and producer's histogram buffer
    } else if (momaApproach == MOMA_APPROACH_LOGGING){
      if (momaStation == MOMA_REMOTE) {
        MomaThread t1 = shims[0];
        t1.targetcpu = 7;
        t1.phase = MOMA_APPROACH_LOGGING;
        synchronized (t1) {
          t1.notifyAll();
        }
      } else if(momaStation == MOMA_LOCAL){
        MomaThread t1 = shims[3];
        t1.targetcpu = 7;
        t1.phase = MOMA_APPROACH_LOGGING;
        t1.iteration = iteration;
        synchronized (t1) {
          t1.notifyAll();
        }
      } else if (momaStation == MOMA_BOTH){
        MomaThread t1 = shims[0];
        t1.targetcpu = 7;
        t1.phase = MOMA_APPROACH_LOGGING;
        MomaThread t2 = shims[3];
        t2.targetcpu = 7;
        //MomaThread.resetHistogram(shimid);
        t2.phase = MOMA_APPROACH_LOGGING;

        synchronized (t1) {
          t1.notifyAll();
        }

        synchronized (t2) {
          t2.notifyAll();
        }
      }
    } else if (momaApproach == MOMA_APPROACH_COUNTING){
      int shimid = 0;
      int targetcpu = 0;

      if (momaStation == MOMA_REMOTE) {
        shimid = 0;
        targetcpu = 7;
        MomaThread t = shims[shimid];
        t.targetcpu = targetcpu;
        //MomaThread.resetCounting(shimid);
        t.phase = MOMA_APPROACH_COUNTING;
        t.countPhase = counting_phase;

        synchronized (t) {
          t.notifyAll();
        }

      } else if(momaStation == MOMA_LOCAL){
        shimid = 3;
        targetcpu = 7;
        MomaThread t = shims[shimid];
        t.targetcpu = targetcpu;
        //MomaThread.resetCounting(shimid);
        t.phase = MOMA_APPROACH_COUNTING;
        t.countPhase = counting_phase;

        synchronized (t) {
          t.notifyAll();
        }
      } else if (momaStation == MOMA_NONE){
        MomaThread.resetCounting(7);
        // DO NOTHING here
      } else {
        System.out.println("Where do you want me to count");
      }
    } else if (momaApproach == MOMA_APPROACH_IPC) {

      MomaThread t = shims[3];
      t.targetcpu = 7;
      //MomaThread.resetCounting(shimid);
      t.phase = MOMA_APPROACH_IPC;
      System.out.println("Wake up shim thread " + t.getName());
      synchronized (t) {
        t.notifyAll();
      }
    }
  }

  public void end(String benchmark, int iteration, boolean warmup) {
    System.out.println("MomaProbe.end(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + "approach " + momaApproach + ")");
    System.out.println("moma approach " + momaApproach);
    if (iteration < 3)
      return;

    if (Options.MomaApproach == null)
      return;

    if (momaApproach == MOMA_APPROACH_HISTOGRAM) {
      waitForShims();
      if (momaStation == MOMA_REMOTE) {
        MomaThread.reportHistogram(1);
      } else if (momaStation == MOMA_LOCAL){
        MomaThread.reportHistogram(2);
      } else if (momaStation == MOMA_BOTH){
        MomaThread.reportHistogram(3);
      }
    }else if (momaApproach == MOMA_APPROACH_COUNTING){
      waitForShims();
    } else if (momaApproach == MOMA_APPROACH_LOGGING || momaApproach == MOMA_APPROACH_IPC){
      System.out.println("stop all shim threads");
      waitForShims();
    }


  }

  public void report(String benchmark, int iteration, boolean warmup) {
    System.out.println("HelloWorldProbe.report(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
  }

  /*
    switch(momaApproach) {
      case MOMA_APPROACH_HISTOGRAM
        break;
      case 2:
        //LET'S DO GLOBAL PROFILING IN THE FIRST ITERATION
        System.out.println("Moma Phase 1");
        //wakeup two threads on core 0 and core 1 to monitor threads on
        for (int i = 0; i < 2; i++) {
          MomaThread t = shims[i];
          t.targetcpu = i + 6;
          t.phase = 1;
          t.samplingRate = rate;
          //wake this thread
          synchronized (t) {
            t.notifyAll();
          }
        }
        break;
      case 3:
        System.out.println("Moma Phase 2");
        //wakeup two threads on core 0 and core 1 to monitor threads on
        for (int i = 0; i < 4; i++) {
          MomaThread t = shims[i];
          t.targetcpu = i + 4;
          t.phase = 2;
          //wake this thread
          synchronized (t) {
            t.notifyAll();
          }
        }
        break;
      default:
        System.out.println("MomaThread should be here");
    }*/
}
