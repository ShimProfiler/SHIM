package probe;

import java.lang.reflect.*;

public class HooverProbe implements Probe {
  private Method beginMethod;
  private Method endMethod;

  public void init() {
    try {
      Class harnessClass = Class.forName("org.mmtk.utility.PTGProfiler");
      beginMethod = harnessClass.getMethod("harnessBegin");
      endMethod = harnessClass.getMethod("harnessEnd");
    } catch (Exception e) {
	throw new RuntimeException("Unable to find MMTk org.mmtk.utility.PTGProfiler.harnessBegin and/or org.mmtk.utility.PTGProfiler.harnessBegin", e);
    }
  }
  public void cleanup() {}

  public void begin(String benchmark, int iteration, boolean warmup) {
    if (warmup) return;
    try {
      beginMethod.invoke(null);
    } catch (Exception e) {
      throw new RuntimeException("Error running PTGProfiler.harnessBegin", e);
    }
  }

  public void end(String benchmark, int iteration, boolean warmup) {
    if (warmup) return;
    try {
      endMethod.invoke(null);
    } catch (Exception e) {
      throw new RuntimeException("Error running PTGProfiler.harnessEnd", e);
    }
  }

  public void report(String benchmark, int iteration, boolean warmup) {}
}