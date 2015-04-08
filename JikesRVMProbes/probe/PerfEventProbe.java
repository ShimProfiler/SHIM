package probe;
 
public class PerfEventProbe implements Probe {
  public void init() {
    /* Leave to Agent_OnLoad */
  }

  public void cleanup() {
    /* Leave to Agent_OnUnload */
  }

  public native void reinitialize();
  public native void begin(String benchmark, int iteration, boolean warmup);
  public native void end(String benchmark, int iteration, boolean warmup);
  public native void report(String benchmark, int iteration, boolean warmup);
}
