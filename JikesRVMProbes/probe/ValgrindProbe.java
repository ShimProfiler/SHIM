package probe;

public class ValgrindProbe implements Probe {
  static {
    System.loadLibrary("valgrind");
  }

  public void init() {
    // Nothing to do
  }

  public void cleanup() {
    // Nothing to do
  }

  public native void begin(String benchmark, int iteration, boolean warmup);
  public native void end(String benchmark, int iteration, boolean warmup);
  public native void report(String benchmark, int iteration, boolean warmup);
}

