package probe;

/*
 *
 */
import moma.MomaCallBack;
import org.jikesrvm.classloader.RVMMethod;
import org.jikesrvm.compilers.common.CompiledMethod;
import org.jikesrvm.compilers.common.CompiledMethods;

import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;

public class ShellProbe implements Probe {
  public void init() {
    System.err.println("ShellProbe.init()");
  }
  public void cleanup() {
    System.err.println("ShellProbe.cleanup()");
  }

  public void begin(String benchmark, int iteration, boolean warmup) {
    System.err.println("ShellProbe.begin(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
  }

  public void end(String benchmark, int iteration, boolean warmup) {
    System.err.println("Shell.end(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
    if (warmup)
      return;

    //now we want to receive command from standinput, and execute that java file with command.
    //read command line like "pprint hello world". pprint is the name of class, hello world are two parameters
    BufferedReader br  = new BufferedReader(new InputStreamReader(System.in));
    while (true){

      String cmd = null;
      try {
        cmd = br.readLine();
      }catch (IOException ioe) {
        System.out.println("IO error while reading cmd from stdin");
        System.exit(1);
      }
      System.out.println("Shell: receive command " + cmd);

      String[] cmds = cmd.split(":");
      int cmid = 0;

      try {
        cmid = Integer.parseInt(cmds[0]);
        CompiledMethod cm = CompiledMethods.getCompiledMethodUnchecked(cmid);
        RVMMethod m = CompiledMethods.getCompiledMethodUnchecked(cmid).getMethod();
        System.out.println(m.getName() + ":" + m.getDeclaringClass() + ":" + m.getSignature() + ":" + m.getDescriptor()+ ":" + cm.getEntryCodeArray().length());
        continue;
      }catch (Exception e){
        System.out.println("not a number " + cmds[0]);
      }

      Class<? extends MomaCallBack> momaClass;
      String probeClassName = "moma." + cmds[0];
      try {
        momaClass = Class.forName(probeClassName).asSubclass(MomaCallBack.class);
      } catch (ClassNotFoundException cnfe) {
        throw new RuntimeException("Could not find probe class '" + probeClassName + "'", cnfe);
      }

      MomaCallBack cb;
      try {
        cb  = momaClass.newInstance();
      } catch (Exception e) {
        throw new RuntimeException("Could not instantiate probe class '" + probeClassName + "'", e);
      }
      cb.invoke(cmds[1]);
    }
  }

  public void report(String benchmark, int iteration, boolean warmup) {
    System.err.println("HelloWorldProbe.report(benchmark = " + benchmark + ", iteration = " + iteration + ", warmup = " + warmup + ")");
  }
}
