package probe;

import java.lang.reflect.*;

public class CompilerSamplesProbe implements Probe {
  private Method startMethod;
  private Method endMethod;

  public void init() {
    try {
      Class harnessClass = Class.forName("org.jikesrvm.adaptive.controller.Controller");
      startMethod = harnessClass.getMethod("startCompilationSamples");
      endMethod = harnessClass.getMethod("endCompilationSamples");
    } catch (Exception e) {
      throw new RuntimeException("Unable to find callback methods", e);
    }
  }

  public void cleanup() {
    // Nothing to do
  }

  public void begin(String benchmark, int iteration, boolean warmup) {
    try {
      startMethod.invoke(null);
    } catch (Exception e) {
      throw new RuntimeException("Error running update compilation sample method", e);
    }
  }

  public void end(String benchmark, int iteration, boolean warmup) {
    try {
      endMethod.invoke(null);
    } catch (Exception e) {
      throw new RuntimeException("Error running update or dump methods", e);
    }
  }

  public void report(String benchmark, int iteration, boolean warmup) {
    // Done within end.
  }
}

