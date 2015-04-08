package probe;

//PERF_EVENT_NAMES is set to EVENT,EVENT

public class PerfEventReader{

  public String[] names;
  public int[] fids;
  public long[] values;

  public static long rapl_unit;

  private static native void init();
  private static native int create(String eventName);
  private static native long read(int fd);
  private static native long raplInit();
  private static native long raplPackage();
  private static native long raplCore();
  private static native long raplUncore();
  private static native long raplDram();

  static {
    System.out.println("java.library.path: " + System.getProperty("java.library.path"));
    System.loadLibrary("perf_event_reader");
    init();
    rapl_unit = raplInit();
    System.out.println("rapl_unit is " + rapl_unit);
  }

  /*rapl counters are RAPL_PACKAGE (-3), RAPL_CORE (-4), RAPL_UNCORE(-5) RAPL_DRAM*/
  public PerfEventReader(String events){
    if (events.length() == 0) {
      // no initialization needed
      return;
    }
    // initialize perf event
    names = events.split(",");
    int n = names.length;

    fids = new int[n];
    values = new long[n];

    for (int i = 0; i < n; i++) {
      System.out.println("Create event " + names[i]);
      if (names[i].equals("RAPL_PACKAGE")){
	fids[i] = -3;
      }else if (names[i].equals("RAPL_CORE")){
	fids[i] = -4;
      }else if (names[i].equals("RAPL_UNCORE")){
	fids[i] = -5;
      }else if (names[i].equals("RAPL_DRAM")){
	fids[i] = -6;
      }else{
	fids[i] = create(names[i]);
      }
      System.out.println("Event " + names[i] + " at fd" + fids[i]);
    }
  }

  public void read(long[] val){
    for (int i=0; i<fids.length; i++){
      if (fids[i] >=0){
	val[i] = read(fids[i]);
      } else {
	if (fids[i] == -3)
	  val[i] = raplPackage();
	else if (fids[i] == -4)
	  val[i] = raplCore();
	else if (fids[i] == -5)
	  val[i] = raplUncore();
	else if (fids[i] == -6)
	  val[i] = raplDram();
	else
	  System.out.println("fd should not be this val " + val[i]);
      }
    }
  }

  public void update(){
    read(this.values);
  }

  public void report(){
    System.out.println("=======events begin =======");
    for (int i = 0; i< fids.length; i++){
	System.out.print(names[i] + ":" + values[i] + "    ");
    }
    System.out.println("");
    System.out.println("=======events end=======");
  }
}
