package moma;

import org.jikesrvm.Options;
import static org.jikesrvm.runtime.SysCall.sysCall;
import org.jikesrvm.VM;
import org.jikesrvm.runtime.Magic;
import org.jikesrvm.runtime.SysCall;

/**
 * Created by xiyang on 11/08/2014.
 */
public class testrdpmc {

  public static boolean enable;


  private static int indexes[];
  private static int numberEvents;
  private static String eventNames[];

  private static void init() {
    eventNames = Options.MomaEvents.split(",");
    numberEvents = eventNames.length;
    sysCall.sysMomaEventInit(numberEvents);
    indexes = new int[numberEvents];
    for (int i = 0; i < numberEvents; i++) {
      indexes[i] = sysCall.sysMomaEventCreate(i, eventNames[i].concat("\0").getBytes());
      VM.sysWriteln("Creat counter", eventNames[i], "at index", indexes[i]);
    }
  }

  public static long test()
  {

    long v1 = 0;
    long v2 = 0;
    for(int i=0;i<100000;i=i+1){
       v1 = Magic.getPerformanceCounter(indexes[0]);
      v2 = Magic.getPerformanceCounter(indexes[0]);     
    }
    return v2 - v1;

  }

  public static void main(String args[]) {
    //start profiling now, checking Options.
    init();
    long cpumask = sysCall.sysGetThreadBindSet(sysCall.sysGetThreadId());
    VM.sysWriteln("cpumask is ", cpumask);
    long count = 0;
    for(int i=0;i<1000000;i=i+1)
      count += test();
    System.out.println("count is " + count);



    /*while(true) {
      for (int i = 0; i < 8; i++) {
        long whichcpu = 1 << i;
        sysCall.sysCall.sysThreadBind(sysCall.sysGetThreadId(), whichcpu);
        //wait for 100k instructions
        VM.sysWriteln("Moving to CPU ", i);
        long base = Magic.getPerformanceCounter(indexes[0]);
        while (Magic.getPerformanceCounter(indexes[0]) < base + 100000)
          ;
      }*/




  }
}
