package probe;

import moma.MomaThread;
import org.jikesrvm.Options;
import org.jikesrvm.VM;
import org.jikesrvm.runtime.Magic;
import static org.jikesrvm.runtime.SysCall.sysCall;

public class MomaProbe implements Probe {
  public static final int StartIteration = 3;
  //Create SHIM thread for each core
  public static final int maxCoreNumber = 4;
  //each core has one corresponding shim working thread
  public static MomaThread[] shims;
  public static MomaThread profiler;
  public static int samplingRate = 1;
  //which CPU JikesRVM process are bind on
  public static int runningCPU = 4;
  public static enum ProfilingApproach{
    EVENTHISTOGRAM, CMIDHISTOGRAM, COUNTING, LOGGING
  };
  public static enum ProfilingPosition{
    SAMECORE, REMOTECORE
  };
  public static ProfilingApproach shimHow;
  public static ProfilingPosition shimWhere;

  private void parseCmd(){
    //momaApproach should be "[remoteCore|sameCore],[histogram|hardonly|softonly|logging],rate"
    String[] ops = Options.MomaApproach.split(",");
    String cmd = ops[0];
    //where we do the measurement
    if (cmd.equals("remote")){
      shimWhere = ProfilingPosition.REMOTECORE;
    }else if (cmd.equals("same")){
      shimWhere = ProfilingPosition.SAMECORE;
    }else{
      System.out.println("Unknown profiling position:" + Options.MomaApproach);
    }

    //what we are going to measure
    String n = ops[1];
    if (n.equals("eventHistogram")){
      shimHow = ProfilingApproach.EVENTHISTOGRAM;
    }else if (n.equals("cmidHistogram")){
      shimHow = ProfilingApproach.CMIDHISTOGRAM;
    }else if (n.equals("counting")){
      shimHow = ProfilingApproach.COUNTING;
    }else if (n.equals("logging")){
      shimHow = ProfilingApproach.LOGGING;
    }else{
      System.out.println("Unknown profiling approach:" + Options.MomaApproach);
    }
    samplingRate = Integer.parseInt(ops[2]);
  }

  public void init(){
    parseCmd();
    long cpumask = sysCall.sysCall.sysGetThreadBindSet(Magic.getThreadRegister().pthread_id);
    for (int i=0; i<64; i++) {
      if ((cpumask & (1 << i)) != 0) {
        runningCPU = i;
        break;
      }
    }
    shims = new MomaThread[maxCoreNumber];
    for (int i = 0; i < maxCoreNumber; i++) {
      int targetCPU = i + maxCoreNumber;
      if (shimWhere == ProfilingPosition.REMOTECORE)
          targetCPU = maxCoreNumber * 2 - i;

      shims[i] = new MomaThread(i, i, targetCPU);
      shims[i].setName("shim" + i);
      shims[i].start();
      if (targetCPU == runningCPU)
        profiler = shims[i];
    }
    System.out.println("Benchmark is running at CPU " + runningCPU + " target SHIM thread is " + profiler.toString());
  }

  private void waitForShims(){
    try {
      for (MomaThread t : shims) {
        //suspend the shim thread if it is running.
        if (t.state == t.MOMA_RUNNING) {
          t.suspendMoma();
          while (t.state == t.MOMA_RUNNING)
            ;
          t.resetControlFlag();
        }
      }
    }catch (Exception e){
      System.out.println("Exception is happened while asking shim threads to stop e:" + e);
    }
  }

  public void cleanup() {
    System.err.println("MomaProbe.cleanup()");
  }

  public void begin(String benchmark, int iteration, boolean warmup) {
    System.out.println("MomaProbe.begin(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
    if (iteration < StartIteration)
      return;

    synchronized (profiler) {
      profiler.notifyAll();
    }
  }

  public void end(String benchmark, int iteration, boolean warmup) {
    System.out.println("MomaProbe.end(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
    if (iteration < StartIteration)
      return;
    waitForShims();
  }

  public void report(String benchmark, int iteration, boolean warmup) {
    System.out.println("MomaProbe.report(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
  }
}
