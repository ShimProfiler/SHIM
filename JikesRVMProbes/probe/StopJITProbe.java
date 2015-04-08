package probe;

public class StopJITProbe implements Probe {
  private static int stoppingIteration = Integer.parseInt(System.getProperty("probe.stopjit.iteration", "2"));
  private static int sleepSeconds = Integer.parseInt(System.getProperty("probe.stopjit.sleeptime", "0"));

  public void init() {}
  public void cleanup() {}
  public void begin(String benchmark, int iteration, boolean warmup) {}
  public void end(String benchmark, int iteration, boolean warmup) {
    if (iteration == stoppingIteration) {
      System.out.println("========== JIT DISABLED (via java.lang.Compiler.disable()"+((sleepSeconds > 0) ? ", sleeping for " + sleepSeconds + " seconds" : "") +") ==========");
      java.lang.Compiler.disable();
      try {
        Thread.currentThread().sleep(1000*sleepSeconds);
      } catch (java.lang.InterruptedException ie) {}
    }
  }
  public void report(String benchmark, int iteration, boolean warmup) {}
}
