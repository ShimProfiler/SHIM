package probe;

import java.lang.reflect.*;

public class JikesRVMPerfEventProbe implements Probe {
  private Method beginMethod;
  private Method endMethod;

  public void init() {
    try {
      Class harnessClass = Class.forName("org.jikesrvm.scheduler.RVMThread");
      beginMethod = harnessClass.getMethod("perfEventEnable");
      endMethod = harnessClass.getMethod("perfEventDisable");
    } catch (Exception e) {
      throw new RuntimeException("Unable to find RVMThread.perfEventEnable or RVMThread.perfEventDisable", e);
    }
  }

  public void cleanup() {
    // Nothing to do
  }

  public void begin(String benchmark, int iteration, boolean warmup) {
    if (warmup) return;
    try {
      beginMethod.invoke(null);
    } catch (Exception e) {
      throw new RuntimeException("Error running RVMThread.perfEventEnable", e);
    }
  }

  public void end(String benchmark, int iteration, boolean warmup) {
    if (warmup) return;
    try {
      endMethod.invoke(null);
    } catch (Exception e) {
      throw new RuntimeException("Error running RVMThread.perfEventDisable", e);
    }
  }

  public void report(String benchmark, int iteration, boolean warmup) {
    // Done within end.
  }
}

