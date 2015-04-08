package probe;


//PERF_EVENT_NAMES is set to EVENT1,fd0,fd1,...fdn;EVENT2,fd0,fd1,....

public class PerfEventLauncherProbe implements Probe {

  private static String[] eventNames;
  private static int[][] eventFds;

  private static native void enable(int fd);
  private static native void disable(int fd);
  private static native void reset(int fd);
  private static native void read(int fd, long[] result);

  static {
    System.loadLibrary("perf_event_launcher");
  }

  public void init()
  {
    System.out.println("PERF_EVENT_NAMES is " + System.getenv("PERF_EVENT_NAMES"));
      
    String[] events = System.getenv("PERF_EVENT_NAMES").split(";");
    System.out.println(events.length);
    eventNames = new String[events.length];
    eventFds = new int[events.length][];
    for (int i=0; i< events.length; i++){
      String[] str = events[i].split(",");
      eventNames[i] = str[0];
      eventFds[i] = new int[str.length-1];
      for (int j=1;j<str.length;j++)
	eventFds[i][j-1] = Integer.decode(str[j]).intValue();	
    }
  }

  public void cleanup(){
    System.out.println("cleanup is called");
  }

  public void begin(String benchmark, int iteration, boolean warmup){
    System.out.println("begin is called at iteration " + iteration);
    if (!warmup){
      for(int i=0; i< eventNames.length; i++){
	for(int j=0; j<eventFds[i].length; j++){
	  enable(eventFds[i][j]);
	}
      }
    }
  }

  public void end(String benchmark, int iteration, boolean warmup){
    System.out.println("end is called at iteration " + iteration);
    if (!warmup){
      for(int i=0; i< eventNames.length; i++){
	for(int j=0; j<eventFds[i].length; j++){
	  disable(eventFds[i][j]);
	}
      }
    }
  }

  public void reset(){
      for(int i=0; i< eventNames.length; i++){
	for(int j=0; j<eventFds[i].length; j++){
	  reset(eventFds[i][j]);
	}
      }  
  }

  public void report(String benchmark, int iteration, boolean warmup)
  {
    System.out.println("report is called at iteration " + iteration);

    if (!warmup){
      System.out.println("============================ Perf Counter Totals ============================");
      for(int i=0; i< eventNames.length; i++)
	for(int j=0; j<eventFds[i].length; j++)
	  System.out.print(eventNames[i] + ":CPU" + j + "    ");
      
      System.out.println("");

      for(int i=0; i< eventNames.length; i++){
	for(int j=0; j<eventFds[i].length; j++){
	  long[] results = new long[3];
	  read(eventFds[i][j], results);
	  
	  if (results[1] == results[2])
	    System.out.print(results[0] + "    ");
	  else
	    System.out.print(results[0] + "Scaled" + "    ");
	}
      }

      System.out.println("");
      System.out.println("------------------------------ Perf Counter Statistics -----------------------------");
    }
  }
}

