package probe;
 
public class ApplicationPerfEventProbe extends PerfEventProbe {

  private boolean reinitialized = false;

  public void begin(String benchmark, int iteration, boolean warmup) {
    if (!reinitialized) {
      /* We restart/reinitialize the counters here so they only deal with application threads */
      super.reinitialize();
      reinitialized = true;
    }
    super.begin(benchmark, iteration, warmup);
  }
}
