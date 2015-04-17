package probe;

import moma.MomaCmd;
import moma.MomaThread;
import org.jikesrvm.Options;
import org.jikesrvm.VM;
import org.jikesrvm.runtime.Magic;
import org.jikesrvm.scheduler.RVMThread;

import static org.jikesrvm.runtime.SysCall.sysCall;
import static moma.MomaCmd.ProfilingApproach.*;
import static moma.MomaCmd.ProfilingPosition.*;

public class MomaProbe implements Probe {
  public static final int StartIteration = 3;
  //Create SHIM thread for each core
  public static final int maxCoreNumber = 4;
  //each core has one corresponding shim working thread
  public static MomaThread[] shims;
  public static MomaCmd[] cmds;
  public static MomaThread profiler;
  public static int samplingRate = 1;
  //which CPU JikesRVM process are bind on
  public static int runningCPU = 4;




  public void init(){
    cmds = MomaCmd.parseShimCommand();
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
      if (cmds[0].shimWhere == REMOTECORE)
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

    int cmdindex = (iteration - StartIteration)%cmds.length;
    System.out.println("Using " + cmdindex + "th command");
    profiler.curCmd = cmds[cmdindex];

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
