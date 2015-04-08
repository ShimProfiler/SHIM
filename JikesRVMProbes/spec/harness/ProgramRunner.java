/*
 * @(#)ProgramRunner.java	1.56 06/17/98
 *
 * Modification suggested by Don McCauley - IBM 02/18/98
 * Do System.runFinalization() to ensure opportunity to unload classes
 * Flagged DWM 
 *
 * Copyright (c) 1998 Standard Performance Evaluation Corporation (SPEC)
 *               All rights reserved.
 * Copyright (c) 1996,1997,1998 IBM Corporation, Inc. All rights reserved.
 *
 * This source code is provided as is, without any express or implied warranty.
 */

package spec.harness;

import java.util.*;
import probe.ProbeMux;

/**
 * This class is derived from java.lang.Thread. The benchmark is started in
 * a seperate thread. The benchmark implements a method harnessMain(). The run
 * method of the Programmer runner calls the harnessMain() of the class that it
 * dynamically creates, given its name.
 */ 
public class ProgramRunner extends Thread {

/**
 * This value represents the priority with which the thread has to be
 * created.
 */
    public static int ThreadPriority = 3;
    
/**
 * SpecJava can be run as application to create the validity files. This flags
 * indicates whether SpecJVMClient98 is being run in the "Generating Validity
 * file mode"
 */
    public static boolean createValidityCheckFiles = false;
    
/**
 * String used to represent the class name
 */
    String        className;
    
/**
 * String array used to send the arguments to the benchmarks
 */
	String[]      args;   
    
/**
 * autoRun flag is used to indicate, whether the benchmark has to run
 * in autorun mode or not. In the autorun mode the benchmark is run 
 * number of times, till its runtime is within 2% range of the previous
 * run time. 
 *     <pre>      
 *              |     * 
 *              |
 *         T    |
 *         i    |
 *         m    |			 *
 *         e    |
 *              |				    *	  
 *              |						   *	  *      * 
 *              |			      			   	
 *              |
 *              |
 *              |_____|______|______|______|______|______|
 *		 		      1	     2      3      4      5      6   	
 *
 *									Run count
 *	</pre>			
 */ 
	boolean       autoRun; 
	int probeRun;

/** This parameter is used to hold the parent handle for this thread. The 
 * Programmer runner is created by SpecJava class, which extends the 
 * BenchmarkDone interface. 
 * Refer to "Using the Interface as type" concept of Java
 */
	BenchmarkDone parent;
    
/**
 * The SpecJVMClient98 has a validity mechanism. Every benchmark outputs some 
 * standard traces onto the Context. This output is verified against the
 * expected output to find the validity of the run of the benchmark.
 * Not all the traces from the benchmark run are considered for validition.
 * The traces that have to be validity checked are prefixed with some 
 * integer. This default value is stored in defaultValidityCheckvalue
 */
	char          defaultValidityCheckValue;
    
/**
 * The percentage run of the benchmark. The benchmark can be run at 1%,
 * 10% and 100% of its full length. percentTimes100 is used to represent
 * this value.  
 */
	int           percentTimes100;

/**
 Minimum number of runs to be completed in the autorun mode
 */	    
	int           automin;

/**
 Maximum number of runs allowed in the autorun mode	   
 */ 
	int           automax;

/**
 Sleep time between two runs in the autorun mode
 */	    
	int           autodelay;
/**
 Flag indicating whether Garbage collection has to be done two consecutive runs
 in the autorun mode
 */
	boolean       autogc;

/**
 Checksum of the trace bytes
 */	    
	boolean       checksum;

      
    // Put any classes into this Hashtable that might be loaded on a second run
    // of a benchmark. ByteArrayInputStream is such a case. It is use in
    // spec.io.FileInputStream	             

/**
 * This HashTable represents the class name and its cache data. Specjava
 * maintains its own cache. Specjava when run as an applet downloads the 
 * classes from the net for the first run. It also caches the classes as
 * and when it down loads. For the subsequent runs, depending on the user
 * choice, specjava uses this cached classes or downloads once again
 */
    static Hashtable classCache = new Hashtable();    
    
    static {    	
	try {
	    Class clazz;
  	    clazz = Class.forName( "java.io.ByteArrayInputStream" );
	    classCache.put( "java.io.ByteArrayInputStream", clazz );
  	    clazz = Class.forName( "spec.io.FileInputStream" );
	    classCache.put( "spec.io.FileInputStream", clazz );  
  	    clazz = Class.forName( "spec.io.PrintStream" );
	    classCache.put( "spec.io.PrintStream", clazz );  
  	    clazz = Class.forName( "spec.io.ValidityCheckOutputStream" );
	    classCache.put( "spec.io.ValidityCheckOutputStream", clazz );  
	} catch( ClassNotFoundException x ) {}
    }
    
    // This variable is used to stop a class being reloaded if it is run twice.

/**
 Last benchmark class that is executed
 */
    static Class     lastClass     = null; 
/**
 Name of the last benchmark executed
 */	
    static String    lastClassName = "";

/**
 * Overloaded constructor for the class.
 * @param className The classname of the benchmark being run
 * @param args arguments for the benchmark
 * @param autoRun The flag representing the autorun selection of the user
 * @param parent The parent of the thread. 
 * @param defaultValidityCheckValue The default prefix value that has to be 
 * added to the trace
 * @param percentTimes100 percentage run of the benchmark
 */
    public ProgramRunner( 
        String		className, 
	String[]	args, 
	boolean		autoRun, 
	int probeRun, 
	BenchmarkDone	parent, 
	char		defaultValidityCheckValue,
	int		percentTimes100,
	int		automin,
	int		automax,
	int		autodelay,
	boolean		autogc,
	boolean		checksum
    ){
	this.className                 = className;
        this.args                      = args;        
	this.autoRun                   = autoRun;
	this.probeRun                   = probeRun;
        this.parent                    = parent;
	this.defaultValidityCheckValue = defaultValidityCheckValue;
	this.percentTimes100           = percentTimes100;
        this.automin		       = automin;
        this.automax		       = automax;
        this.autodelay		       = autodelay;
        this.autogc		       = autogc;
        this.checksum		       = checksum;
    }

    
/**
 * The overloaded run method of the benchmark. This method just
 * set the thread priority. and calls the runBenchmark method
 */
    public void run() {
        setPriority( ThreadPriority );
        runBenchmark();
    }

/** This function is called when the user presses the 'Stop' button during the
 * executionof the benchmark.
 */
    public void stopBenchmark(){
	Context.out.println ("Program runner received stop message");
	if (parent != null)
	    parent.benchmarkDone( className, null );
	stop();
    }

/**
 * This function clears the FileInputStream's cache and calls runBenchmark2
 * method. It closes all the file streams once the benchmark finishes the run.
 * The benchmarkDone method of the parent is called at the end of the function.
 * @see spec.harness.BenchmarkDone
 */
    void runBenchmark() {
        Properties results = null;
        if( className == null ) {
            Context.out.println( "No class selected in runBenchmark" );
        } else {        
            // The following should cause the last class run to be flushed
	    // from the cache if it is not the one we are about to run.   
            if( !className.equals( lastClassName ) ) {
                lastClass = null;
                lastClassName = "";
		// this is now done in runBenchmark2 prior to optional pause
                //spec.io.FileInputStream.clearCache();
            }
            results =  runBenchmark2();    
        }           
        try {
	    spec.io.FileInputStream.closeAll();
	} catch( java.io.IOException x ) {}
        if( parent != null ) {
            parent.benchmarkDone( className, results );         
        }
    }

/**
 * This function loads the instance of the class given the benchmark name.
 * It calls the harnessMain method of the class instance, the start time is noted 
 * before calling the harnessMain method . If the benchmark runs successfully,
 * the benchmark run parameters are returned. The system garbage collector is 
 * also called after finishing the benchmark run
 * @return Properties of the benchmark run information.
 */
    Properties runBenchmark2() { 		
	boolean valid = false;
	long startTime;
	String fullName = "spec.benchmarks."+className+".Main";	
	Properties results  = new Properties();	    
	int speed = Context.getSpeed();
	int intersperse = 0;
	if (speed == 100)
	    intersperse = 10;
	else if (speed == 10)
	    intersperse = 1;
	try {
	    System.gc();
	    System.runFinalization();   /* DWM */
	    startTime = System.currentTimeMillis();
	    Class clazz = Class.forName( fullName );		
	    if( clazz == null ) {
	        Context.out.println( "Could not load class " + fullName );
		return null;
	    }
	    lastClassName = className;
	    lastClass     = clazz;
            Object prog   = clazz.newInstance();
	    if( (prog instanceof SpecBenchmark) == false ) {
		Context.out.println( fullName +
		    " does not implement the SpecBenchmark interface" );
		return null;
	    }
	    Context.out.print( "Caching " +
		(Context.isCachedInput() ? "On" : "Off") );
	    Context.out.print( " Speed = " + speed );
	    if( autoRun ) {
	        Context.out.println( " Auto run mode " );
	    }
	    if( probeRun>0 ) {
	        Context.out.println( " Probe run mode " );
	        ProbeMux.init();
	    } else {
	    Context.out.print( "\n" );
	    Context.out.println( "======= " + className + " Starting =======");
	    Context.setBenchmarkRelPath ((new String(
		fullName.toCharArray(),0,fullName.lastIndexOf(".")+1)).
		    replace('.','/') );   
}
	    double lastTime = Double.MAX_VALUE;		 
	    for (int run = 0 ;; run++) {
	    if (probeRun>0) {
	    Context.out.print( "\n" );
	    Context.out.println( "======= " + className + " Starting =======");
	    Context.setBenchmarkRelPath ((new String(
		fullName.toCharArray(),0,fullName.lastIndexOf(".")+1)).
		    replace('.','/') );
}  
		String pref = "spec.results." + className + ".run" + run;
                Context.out.println("Run "+run+" start. " +
                    "Total memory="+Runtime.getRuntime().totalMemory()+
                    " free memory=" + Runtime.getRuntime().freeMemory());
                results.put( pref + ".stats.TotalMemoryStart" ,
                    ""+Runtime.getRuntime().totalMemory() );            
                results.put( pref + ".stats.FreeMemoryStart" ,
                    ""+Runtime.getRuntime().freeMemory() );                 
	        if (probeRun>0) ProbeMux.begin(className, run+1 < probeRun);
		BenchmarkTime time = runOnce
		    (prog, run, startTime, speed, results);
	        if (probeRun>0) ProbeMux.end((run+1) < probeRun);
                Context.out.println( "======= " + className +
                    " Finished in " + time.toLongString() );
                results.put( pref + ".time" ,  time.toString() );       
                results.put( pref + ".speed" , ""+ speed );
		saveResults (pref, results);
		tellParent( className + " - " + time.toLongString() + "\n");
                if (probeRun > 0) {
                  if (run+1 == probeRun) break;
                } else {
		if( autoRun == false)
		    break;
		// This is an autorun sequence
		// Check for run time threshhold termination of sequence
		if (run >= automin - 1 &&
		    (lastTime/time.secs() < (1+percentTimes100/10000.0) )) {
		    break;
		}
		// Check for automax termination of sequence
		if (run >= automax - 1){
		    break;
		}
		/* In an autorun sequence, intersperse untimed executions
		 * of the next smaller size in order to avoid
		 * uncharacteristic optimization feedback from one
		 * execution to the next
		 */
		if (intersperse > 0){
		    BenchmarkTime ignore =
			runOnce (prog, run, 0, intersperse, results);
	            Context.out.println ("======= " + className + " " +
			intersperse + "% execution - timing ignored");
		}}
		lastTime  = time.secs();
		startTime = System.currentTimeMillis();
	    }//end for (run)
	    if (probeRun>0) ProbeMux.cleanup();
            spec.io.FileInputStream.clearCache();
	    if (autogc){
	        Context.out.println ("requested gc");
	        System.gc();
		System.runFinalization();   /* DWM */
	    }
	    pause (autodelay);
        } catch( Throwable e ) {    
	    String msg = ">>>> "+fullName+" exited with exception: " +
		e + " <<<<\n";	        
            Context.out.print( msg );
	    e.printStackTrace( Context.out );   
	    Context.out.println( "" );   
	    tellParent( msg );
	    return null;	    
	}
	return results;
    }

/** Runs the benchmark only once. 
 * @param prog Benchmark Object
 * @param run Run number
 * @param startTime Time at which benchmark is started
 * @param speed Speed at which the benchmark is run (1%, 10% or 100%)
 * @param Properties Run results of the benchmark
 */    
	private BenchmarkTime runOnce (Object prog, int run, long startTime,
	int speed, Properties results) throws java.io.IOException
    {
        Context.clearIOtime();
	Context.setSpeed (speed);
        spec.io.FileInputStream.clearIOStats();
        java.io.PrintStream save = null;
        spec.io.ValidityCheckOutputStream vcos = null;
	// Trust but verify - Ronald Reagan
        //if (Context.getVerify()) {              
            vcos = new spec.io.ValidityCheckOutputStream(
                className, "validity"+ speed +".dat",
                defaultValidityCheckValue);
            save = Context.out;               
            Context.out = new spec.io.PrintStream(vcos);
        //}
        spec.io.FileOutputStream.clearCount();             
        ((SpecBenchmark)prog).harnessMain( args );      
        spec.io.FileOutputStream.printCount(checksum);
        //if (Context.getVerify()) {
            Context.out.close();
            Context.out = save; 
        //}                                                       
        BenchmarkTime time = new BenchmarkTime(
	    System.currentTimeMillis() - startTime);
        String statsString = getStatsString( );            
        Context.out.println( statsString );                 
        Context.out.println("Run "+run+" end. " +
            "Total memory="+Runtime.getRuntime().totalMemory()+
            " free memory=" + Runtime.getRuntime().freeMemory());
	//if (Context.getVerify()) {
	    if (createValidityCheckFiles) {
		vcos.createValidityFile();
	    } else if (Context.getVerify()) {
		if (! vcos.validityCheck(results, run))
		    time.valid = false;
	    }
	//}
        return time;
    }

/**
 * pauses for given time. This method is called between the consecutive runs 
 * of the benchmark
 */
 	private void pause (long millis){
	if (millis > 0){
            Context.out.println ("pausing for " + millis + " ms");
	    try{
	        sleep (millis);
	    }catch (InterruptedException e){}
	}
    }

/** 
 * Formats the run results of the benchmark and stores them in the Properties
 * object.
 * @param pref Prefix string for the properties. This is the benchamark name
 * @param results The Properties object which stores the run results
 */    
	private void saveResults (String pref, Properties results){
        results.put( pref + ".stats.TotalMemoryEnd" ,
            ""+Runtime.getRuntime().totalMemory() );            
        results.put( pref + ".stats.FreeMemoryEnd"  ,
            ""+Runtime.getRuntime().freeMemory() );     
        results.put( pref + ".stats.cache",
            (Context.isCachedInput() ? "true" : "false"));      
        results.put( pref + ".stats.IOTime" ,
            ""+(spec.io.FileInputStream.getIOtime()/1000.0) );          
        results.put( pref + ".stats.CacheTime" , 
            ""+(spec.io.FileInputStream.getCachingtime()/1000.0) );
        results.put( pref + ".stats.FileOpens" , 
            ""+ spec.io.FileInputStream.getNumUsedFiles() );    
        results.put( pref + ".stats.NumCacheByteReads" , 
            ""+ spec.io.FileInputStream.getNumCacheByteReads() );       
        results.put( pref + ".stats.NumFileByteReads" ,  
            ""+ spec.io.FileInputStream.getNumFileByteReads() );
        results.put( pref + ".stats.NumUrlByteReads" ,   
            ""+ spec.io.FileInputStream.getNumUrlByteReads() );
        results.put( pref + ".stats.NumCachedFiles" , 
            ""+ spec.io.FileInputStream.getNumCachedFiles() );  
        results.put( pref + ".stats.CachedDataSize" , 
            ""+ spec.io.FileInputStream.getCachedDataSize() );  
        results.put( pref + ".stats.NumCacheHits" ,   
            ""+ spec.io.FileInputStream.getNumCacheHits() );    
        results.put( pref + ".stats.NumCacheHits" ,
            ""+ spec.io.FileInputStream.getNumCacheHits() );    
        results.put( pref + ".stats.totalRetries" ,
            ""+ spec.io.FileInputStream.getTotalRetries() );    
    }

/**
 * This function calls the benchmark run information printing routine of the
 * parent. The results are output to the Console Window
 */
    void tellParent( String str ) {
		if( parent != null ) {
				parent.benchmarkPrint( str );
		}
    }

/**
 * Converts the run details into the string format
 * @return String Run infomation
 * @see spec.io.FileInputStream
 */
    String getStatsString( ) {
	StringBuffer b = new StringBuffer();
	b.append("\n#### IO Statistics for this Run ####");
	b.append("\n## IO time                      : " + (spec.io.FileInputStream.getIOtime()/1000.0) + " seconds" );
	//b.append("\n## Caching time                 : " + (spec.io.FileInputStream.getCachingtime()/1000.0) + " seconds" );
	b.append("\n## No. of File opens            : " + spec.io.FileInputStream.getNumUsedFiles() );
	b.append("\n## No. of Byte Reads from cache : " + spec.io.FileInputStream.getNumCacheByteReads() );
	b.append("\n## No. of Byte Reads from File  : " + spec.io.FileInputStream.getNumFileByteReads() );
	b.append("\n## No. of Byte Reads from Url   : " + spec.io.FileInputStream.getNumUrlByteReads() ); 		   
	b.append("\n#### Cumulative Cache Stats:");
	b.append(" N " + spec.io.FileInputStream.getNumCachedFiles() );
	//debug only:   b.append("\n## Files cached                 : " + spec.io.FileInputStream.getListCachedFiles() );
	b.append(", B " + spec.io.FileInputStream.getCachedDataSize());
	b.append(", H " + spec.io.FileInputStream.getNumCacheHits() );
	b.append(", M " + spec.io.FileInputStream.getNumCacheMisses() );
	b.append("\n#### No. of HTTP retries          : " + spec.io.FileInputStream.getTotalRetries() );
	b.append("\n");
	return b.toString();
    }
}
