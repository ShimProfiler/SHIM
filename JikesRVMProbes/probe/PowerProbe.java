package probe;
import java.io.*;

public class PowerProbe implements Probe {
  private static String powerDevice = System.getProperty("probe.power.device", "/dev/ttyACM0");
  private static double powerMultiplier = Double.parseDouble(System.getProperty("probe.power.multiplier", "-28.45"));
  private static double powerIntercept = Double.parseDouble(System.getProperty("probe.power.intercept", "14591.1"));
  private Process child;
  private BufferedReader in;
  private StringBuilder current;

  public void init() {
    try {
      child = Runtime.getRuntime().exec("cat " + powerDevice);
      BufferedReader err = new BufferedReader(new InputStreamReader(child.getErrorStream(), "UTF-8"));
      in = new BufferedReader(new InputStreamReader(child.getInputStream(), "UTF-8"));
      if (in.readLine() == null) {
        String errorString = "";
        while (err.ready()) errorString += err.readLine();
        int result = child.waitFor();
        throw new RuntimeException("Child process died: " + errorString);
      }
    } catch (Exception e) {
      throw new RuntimeException("Error opening power device '" + powerDevice + "', try setting with -Dprobe.power.device=X", e);
    }
  }

  public void cleanup() {
    try {
      in.close();
      child.destroy(); 
    } catch (IOException e) {
      throw new RuntimeException("Error closing power device", e);
    }
  }

  public void begin(String benchmark, int iteration, boolean warmup) {
    try {
      // Flush all data
      while(in.ready()) in.readLine();
    } catch (IOException e) {
      throw new RuntimeException("Error reading power device", e);
    }
  };

  public void end(String benchmark, int iteration, boolean warmup) {
    current = new StringBuilder();
    try {
      while(in.ready()) current.append(" " + in.readLine());
    } catch (IOException e) {
      throw new RuntimeException("Error reading power device", e);
    }
  }

  public void report(String benchmark, int iteration, boolean warmup) {
    String[] values = current.toString().split("[^0-9]+");
    long x = 0;
    int n = 0;
    for(String v: values) {
      if (!v.equals("")) {
        x += Integer.parseInt(v);
        n++;
      }
    }
    double avg = (double)x / (double)n;
    double watts = (powerMultiplier * avg + powerIntercept) * 12 / 1000;
    System.err.println("============================ Tabulate Statistics ============================");
    System.err.println("power.n power.sum power.avg power.watts");
    System.err.println(n + " " + x + " " + avg + " " + watts);
    System.err.println("=============================================================================");
    current = null;
  }
}
