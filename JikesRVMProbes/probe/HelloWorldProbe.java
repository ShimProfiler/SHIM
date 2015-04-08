package probe;

public class HelloWorldProbe implements Probe {
  public void init() {
    System.err.println("HelloWorldProbe.init()");
  }
  public void cleanup() {
    System.err.println("HelloWorldProbe.cleanup()");
  }

  public void begin(String benchmark, int iteration, boolean warmup) {
    System.err.println("HelloWorldProbe.begin(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
  }

  public void end(String benchmark, int iteration, boolean warmup) {
    System.err.println("HelloWorldProbe.end(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
  }

  public void report(String benchmark, int iteration, boolean warmup) {
    System.err.println("HelloWorldProbe.report(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
  }
}
