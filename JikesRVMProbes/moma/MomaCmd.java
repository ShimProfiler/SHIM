package moma;

import org.jikesrvm.Options;

import static moma.MomaCmd.ProfilingPosition.REMOTECORE;
import static moma.MomaCmd.ProfilingPosition.SAMECORE;

/**
 * Created by xiyang on 13/04/15.
 * Command information for the profiler
 */
public class MomaCmd {
  public static enum ProfilingApproach {
    EVENTHISTOGRAM, CMIDHISTOGRAM, COUNTING, LOGGING
  }

  public static enum ProfilingPosition {
    SAMECORE, REMOTECORE
  }


  public static MomaCmd[] cmdIterations;

  public ProfilingApproach shimHow;
  public ProfilingPosition shimWhere;
  public String cpuFreq;
  int samplingRate;

  private MomaCmd(ProfilingPosition where, ProfilingApproach how, int rate, String freq) {
    shimHow = how;
    shimWhere = where;
    cpuFreq = freq;
    samplingRate = rate;
  }
  //momaApproach should be iteration:iteration:...
  // iteration:"[remoteCore|sameCore],[histogram|hardonly|softonly|logging],rate,[cpuFrequency]"

  public static MomaCmd[] parseShimCommand() {
    String[] its = Options.MomaApproach.split(":");
    cmdIterations = new MomaCmd[its.length];
    for (int i = 0; i < its.length; i++) {
      String[] cmds = its[i].split(",");
      System.out.println("Parse command for iteration " + i + " : " + its[i]);
      ProfilingPosition where = SAMECORE;
      if (cmds[0].equals("remote")) {
        where = REMOTECORE;
      }

      String n = cmds[1];
      ProfilingApproach how = ProfilingApproach.EVENTHISTOGRAM;

      if (n.equals("eventHistogram")) {
        how = ProfilingApproach.EVENTHISTOGRAM;
      } else if (n.equals("cmidHistogram")) {
        how = ProfilingApproach.CMIDHISTOGRAM;
      } else if (n.equals("counting")) {
        how = ProfilingApproach.COUNTING;
      } else if (n.equals("logging")) {
        how = ProfilingApproach.LOGGING;
      } else {
        System.out.println("Unknown profiling approach:" + Options.MomaApproach);
      }

      int rate = Integer.parseInt(cmds[2]);
      String freq = cmds[3];
      cmdIterations[i] = new MomaCmd(where, how, rate, freq);
      System.out.println("Iteration" + i + " CMD " + where + "," + how + "," + rate + "," + freq);
    }
    return cmdIterations;
  }

}


