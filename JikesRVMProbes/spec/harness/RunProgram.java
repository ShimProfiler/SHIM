/*
 * @(#)RunProgram.java	1.7 06/17/98
 *
 * Copyright (c) 1998 Standard Performance Evaluation Corporation (SPEC)
 *               All rights reserved.
 * Copyright (c) 1997,1998 Sun Microsystems, Inc. All rights reserved.
 *
 * This source code is provided as is, without any express or implied warranty.
 */

package spec.harness;
import java.util.Properties;

public class RunProgram{

public static ProgramRunner runner;

public static void done(){
    runner = null;
}

public static void run (String className, boolean autoRun, int probeRun, Properties props,
    BenchmarkDone parent)
{
    if (runner != null){
        Context.out.println ("Cannot start " + className + 
            " because " + runner.className + " is running");
        return;
    }
    char defaultValidityValue = '0';
    String validityString = (String) props.get( "spec.validity."+className );
    if (validityString != null) {
        defaultValidityValue = validityString.charAt(0);
    }
    int automin = getIntProp (props, "spec.initial.automin", 1);
    int automax = getIntProp (props, "spec.initial.automax", 20);
    int autodelay = getIntProp (props, "spec.initial.autodelay", 0);
    int autodelaymax = getIntProp (props, "spec.initial.autodelaymax",
	Integer.MAX_VALUE);
    if (autodelay > autodelaymax)
	autodelay = autodelaymax;
    boolean autogc = getBoolProp (props, "spec.initial.autogc", true);
    int pct = getIntProp (props, "spec.initial.percentTimes100",-99);
    boolean doChecksum = getBoolProp (props,
        "spec.validity.checksum."+className, true);
    runner = new ProgramRunner (
        className,
        new String[0],
        autoRun,
        probeRun,
        parent,
        defaultValidityValue,
        pct,
        automin,
        automax,
        autodelay,
        autogc,
        doChecksum
        );
    if (Context.getCommandLineMode())
        runner.run();	// only one thread for run from command line
    else
        runner.start(); // multi-threaded from harness
}

public static void stop(){
    if (runner == null){
        Context.out.println ("Cannot stop. No benchmark is running.");
        return;
    }
    runner.stopBenchmark();
    Context.out.println(" manually stopped");
    done();
}

private static boolean getBoolProp (Properties props, String key, boolean value){
    if (props != null){
        String s = (String) props.get (key);
        if (s != null){
            if (s.equalsIgnoreCase("true"))
                return true;
            if (s.equalsIgnoreCase("false"))
                return false;
        }
    }
    return value;
}

private static int getIntProp (Properties props, String key, int value){
    if (props != null){
        String s = (String) props.get (key);
        if (s != null)
            try{
                value = Integer.parseInt (s);
            }catch (NumberFormatException e){} // keep default value
    }
    return value;
}

}//end class
