package probe;

import java.lang.reflect.*;

public class ThreadEventProbe implements Probe {
  private Method beginMethod;
  private Method endMethod;

  public void init() {
    try {
      Class harnessClass = Class.forName("org.jikesrvm.scheduler.RVMThread");
      beginMethod = harnessClass.getMethod("perfEventStart");
      endMethod = harnessClass.getMethod("perfEventStop");
    } catch (Exception e) {
      throw new RuntimeException("Unable to find RVMThread.perfEventStart and/or perfEventStop", e);
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
      throw new RuntimeException("Error running perfEventStart", e);
    }
  }

  public void end(String benchmark, int iteration, boolean warmup) {
    if (warmup) return;
    try {
      endMethod.invoke(null);
    } catch (Exception e) {
      throw new RuntimeException("Error running perfEventStop", e);
    }
  }

  public void report(String benchmark, int iteration, boolean warmup) {
    // Done within end.
  }
}

