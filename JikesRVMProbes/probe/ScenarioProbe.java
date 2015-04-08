package probe;

public class ScenarioProbe implements Probe {
  public void init() {}
  public void cleanup() {}

  public void begin(String benchmark, int iteration, boolean warmup) {
    System.err.print("====> Scenario iteration=");System.err.println(iteration);
  }

  public void end(String benchmark, int iteration, boolean warmup) {}
  public void report(String benchmark, int iteration, boolean warmup) {}
}
