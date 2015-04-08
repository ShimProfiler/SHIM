package moma;


import org.jikesrvm.Options;

import static org.jikesrvm.runtime.SysCall.sysCall;

import org.jikesrvm.VM;
import org.jikesrvm.classloader.Atom;
import org.jikesrvm.classloader.RVMMethod;
import org.jikesrvm.compilers.common.CompiledMethod;
import org.jikesrvm.compilers.common.CompiledMethods;
import org.jikesrvm.runtime.Magic;
import org.jikesrvm.scheduler.RVMThread;
import org.vmmagic.unboxed.Word;

import java.io.FileOutputStream;
import java.io.PrintStream;
import java.io.PrintWriter;
import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;

/**
 * Created by xiyang on 3/09/2014.
 */
public class MoMaLogger extends MoMaProfiler {

  public static final int BUFSIZE = 1024*1024;

  //counters[SIZE][NumberofEvents + 1]

  private long tempCounter[];
  private  long momaCounters[][];
  private  long dumpLines = 0;
  private  int numberItems;

  //index key for reading perf counters cheaply in userlevel
  private int eventIndexes[];
  private int numberPerfEvent;
  private String eventNames[];


  private RVMThread targetThread;
  private PrintStream dumpFile;


  private void reportThreadName() {
    for (RVMThread t : RVMThread.threads) {
      if (t != null)
        System.out.println(" Thread" + t.getId() + t.getName());
    }
  }


  public void init() {
    System.out.println("haha, moma profiler thread is here.");
    if (Options.MomaEvents == null) {
      System.out.println("MomaEvents is not set.");
      return;
    }

    //setup counters
    eventNames = Options.MomaEvents.split(",");
    numberPerfEvent = eventNames.length;
    sysCall.sysMomaEventInit(numberPerfEvent);
    eventIndexes = new int[numberPerfEvent];

    // new buffer
    tempCounter = new long[numberPerfEvent + 1];
    momaCounters = new long[BUFSIZE][numberPerfEvent + 1];

    for (RVMThread t : RVMThread.threads) {
      if (t != null && t.getName().equals("MainThread")) {
        targetThread = t;
        System.out.println("Pin MainThread to 0x1, pin myself to 0x10");
        sysCall.sysCall.sysThreadBind(t.pthread_id, 0x2);
        System.out.println("Mainthread's mask is " + sysCall.sysCall.sysGetThreadBindSet(t.pthread_id));
        sysCall.sysCall.sysThreadBind(Magic.getThreadRegister().pthread_id, 0x20);
        System.out.println("myself mask is  " + sysCall.sysCall.sysGetThreadBindSet(Magic.getThreadRegister().pthread_id));

      }
    }

    for (int i = 0; i <  numberPerfEvent; i++) {
      eventIndexes[i] = sysCall.sysMomaEventCreate(i, eventNames[i].concat("\0").getBytes());
      VM.sysWriteln("Creat counter", eventNames[i], "at index", eventIndexes[i]);
    }

    try {
      dumpFile = new PrintStream("/tmp/momaDump");
    } catch (Exception e) {
      System.out.println("Can't create /tmp/momaDump");
    }

    DateFormat dateFormat = new SimpleDateFormat("yyyy/MM/dd HH:mm:ss");
    Date date = new Date();

    dumpFile.println("{\n \"date\":\"" + dateFormat.format(date) + "\",");

    for (int i =0; i <  numberPerfEvent; i++){
      dumpFile.println("\"event" + i + "\":\"" + eventNames[i] + "\",");
    }
  }

  private void logCounters(long counters[], long tag) {
    for (int i = 0; i <  numberPerfEvent; i++) {
      counters[i] = Magic.getPerformanceCounter(eventIndexes[i]);
    }
    counters[numberPerfEvent] = tag;
  }


  public void profile() {
    //System.out.println("moma profile");
    int i = 0;
    logCounters(momaCounters[i], (long)targetThread.momaAppEvent);
    long lastTag = (int)momaCounters[i++][numberPerfEvent];

    while (true) {
      long now = targetThread.momaAppEvent;
      if (now != lastTag) {
        logCounters(momaCounters[i++], now);
        lastTag = now;
        if (i == BUFSIZE || enabled == false) {
          numberItems = i;
          break;
        }
      }
    }
  }

  public  void beginProfile() {
    dumpFile.print("\"items\":[\n");

  }

  public  void endProfile() {
    dumpFile.println("]}\n");
  }


  //return jason style
  //{"seqId":global,"bufId":bufid,"package":"packagename", "method":"methodName", "cmid", cmid, "event0":counters,... "eventn":}
  public String formatEventsToString(long counters[], int bufId)
  {
    int tag = (int)counters[numberPerfEvent];
    int cmid = tag;

    if (tag < 0) {
      cmid = -tag;
    }

    dumpLines++;

    RVMMethod method;
    String className = "XXXX";
    String methodName = "XXXX";
    if (cmid > 0 && cmid <= CompiledMethods.currentCompiledMethodId) {
      method = CompiledMethods.getCompiledMethod(cmid).method;
      if (method != null) {
        className = method.getDeclaringClass().toString();
        methodName = method.getName().toString();
      }
    }

    //dump message to dumpFile;
    String formatString;

    if (dumpLines == 1)
      formatString = "{";
    else
      formatString = ",\n{";

    formatString += "\"seqId\":" + dumpLines + "," + "\"bufId\":" + bufId + "," + "\"cmid\":" + tag + "," +  "\"className\":\"" + className + "\"," + "\"methodName\":\"" + methodName + "\"";
    for (int j = 0; j <  numberPerfEvent; j++) {
      formatString += ",\"event" + j + "\":" + counters[j];
    }
    formatString += "}";
    return formatString;
  }

  public void process() {
    dumpFile.print(formatEventsToString(momaCounters[0], 0));

    for (int i = 1; i < numberItems; i++) {
      for (int j=0;j<numberPerfEvent; j++) {
        tempCounter[j] = momaCounters[i][j] - momaCounters[i-1][j];
      }
      tempCounter[numberPerfEvent] = momaCounters[i][numberPerfEvent];

      dumpFile.print(formatEventsToString(tempCounter, i));
    }

  }

}







