/*
 * @(#)SpecApplication.java	1.28 06/17/98
 *
 * Copyright (c) 1998 Standard Performance Evaluation Corporation (SPEC)
 *               All rights reserved.
 * Copyright (c) 1997,1998 Sun Microsystems, Inc. All rights reserved.
 *
 * This source code is provided as is, without any express or implied warranty.
 */

import java.awt.*;
import java.applet.*;
import spec.harness.*;

/**
 This class is used for launching the SpecJVMClien98 as an application. This mode
 of operation is only for debugging purpose. The results from this mode can or
 can't reported to the Spec team depending on the final run rules
 */

public class SpecApplication {

    static boolean autogc = false;
    static int automin = 1;
    static int automax = 99;
    static int probeRun = 0;
    static int autodelay = 0;
    static int pct100;
    static boolean useThreads = false;  // only use for stand alone option
    static java.util.Properties props = getBasicProperties();

    // Main - creates a frame (SpecApplicationFrame) and set up applet to run 
    // within that frame.

/**
 Instance main method. This method parses all the command line options for
 SpecJVMClient and sets the run parameters accordingly.
 */
    public static void main( String args[] ) {
    
	boolean runHarness = true;
	boolean autorun    = false;
			
	Context.setCachedInputFlag( false );
	
	for( int i = 0 ; i < args.length ; i++ ) {
	    String s = args[i];
	    if( s.charAt( 0 ) == '-' ) {
		switch( s.charAt( 1 ) ) {
		
		    case 'a':
  			autorun = true;
			break;
				
		    case 'b':
			// non-interactive, formerly known as batch
			// to generations of punch card programmers
			// in the days when men were men and
			// programming language was FORTRAN
			Context.setBatch(true);
			break;

		    case 'c':
			ProgramRunner.createValidityCheckFiles = true;
			Context.out.println( "Will create validity check files" );
			break;

		    case 'd':
			autodelay = getarg (s,0);
			Context.out.println ("Will delay " + autodelay +
			    " ms in between benchmarks");
			break;

		    case 'g':
			autogc = true;
			Context.out.println ("Will gc in between benchmarks" );
			break;

		    case 'i':
		        probeRun = getarg (s,0);
			Context.out.println("Will use probeRun mode with " + probeRun + " iterations.");
                        break;
		    case 'm':
			automin = getarg (s,1);
			Context.out.println ("Will run each benchmark " +
			    "at least " + automin + " times");
			props.put ("spec.initial.automin",
			    Integer.toString(automin));
			break;

		    case 'M':
			automax = getarg (s,99);
			Context.out.println ("Will run each benchmark " +
			    "at most " + automax + " times");
			props.put ("spec.initial.automax",
			    Integer.toString(automax));
			break;

		    case 'n':
  			Context.setCachedInputFlag( true );
			break;

		    case 'p': {
			char c = s.charAt( 2 );
			ProgramRunner.ThreadPriority = (int)c - 0x30;
			Context.out.println( "ThreadPriority will be " + ProgramRunner.ThreadPriority );
			break;
		    }	 		    

		    case 'P': {
			pct100 = getarg (s,300);
			double p = pct100/100.0;
			Context.out.println ("autorun threshhold is "
			    + p + "%");
			props.put ("spec.initial.percentTimes100",
			    Integer.toString (pct100));
			break;
		    }	 		    

		    case 's': {
			int val = 0;
			for( int j = 2 ; j < s.length() ; j++ ) {
			    val *= 10;
			    val += ((int)s.charAt( j )) - 0x30;
			}
			Context.setSpeed( val );
			System.out.println( "Speed will be " + val );
			break;
		    }

		    case 't': {
			useThreads = true;
			Context.out.println( "Will run using thread" );
			break;
		    }	

		    case 'u': {
			String userPropFile = s.substring(2);
			Context.out.println("User properties: " + userPropFile );
			Context.setUserPropFile (userPropFile);
			break;
		    }	
		    
		    /*
		    case 'v': {
			Context.setVerify(false);
			Context.out.println( "Will not verify" );
			break;
		    }		
		    */

		    default: {
			 usage();
			 System.exit( 1 );
		    }
		}
	    } else {		    
		 runHarness = false;		 
		 Context.setCommandLineMode (false);
		 runBenchmark( s, autorun, probeRun );	
	    }	    
	}
	
        if( runHarness ) {
            harness();
	}
	
    if( !runHarness && !useThreads )
        System.exit(0); // Should not be necessary but...

    }

/**
 Parses the arguments
 */
    private static int getarg (String s, int value){
	if (s != null && s.length() > 2){
	    try{
		value = Integer.parseInt (s.substring(2));
	    }catch (NumberFormatException e){}
	}
	return value;
    }

/**
 The usage text is displayed, if the user runs the benchmark with wrong parameters
 */
    private static void usage(){
	Context.out.println ("Usage: java [JVMoptions] SpecApplication " +
	    "[options] [_NNN_benchmark]\n" +
	    "Options:\n" +
"-a            Perform autorun sequence on a single selected benchmark\n" +
"-b            Non-interactive execution controlled by parameter settings\n" +
"-c            Create new validity check files\n" +
"-d<number>    Delay <number> milliseconds between benchmark executions\n" +
"-g            Garbage collect in between benchmark executions\n" +
"-i<number>    Use probe running mode for the number of iterations\n" +
"-m<number>    Set minimum number of executions in autorun sequence\n" +
"-M<number>    Set maximum number of executions in autorun sequence\n" +
"-n            Turn *ON* file input caching\n" +
"              >>note the sense of this switch changed in V20<<\n" +
"-p<number>    Set thread priority to <number>\n" +
"-P<number>    Set autorun improvement threshhold (hundreths of a percent)\n" +
"-s<number>    Set problem size to <number>, 1, 10, or 100\n" +
"-u<filename>  Specify <filename> property file instead of props/user\n"
	);
    }

/**
 Brings up the harness window
 */
    public static void harness() {

        // Create Toplevel Window to contain applet hello

        SpecApplicationFrame frame = new SpecApplicationFrame( "SPEC JVM Client98" );

        // Must show Frame before we size it so insets() will return valid values

        frame.show();
        frame.hide();
        frame.resize(frame.insets().left + frame.insets().right  + 530,
            frame.insets().top  + frame.insets().bottom + 375);

        // The following code starts the applet running within the frame window.
        // It also calls GetParameters() to retrieve parameter values from the
        // command line, and sets m_fStandAlone to true to prevent init() from
        // trying to get them from the HTML page.

        SpecApplet applet = new SpecApplet();

        frame.add("Center", applet);

        applet.applicationInit();
        applet.start();
	
        frame.show();	
    }

/**
 Runs the selected benchmark
 */
    public static void runBenchmark( String className, boolean autorun, int probeRun ) {
        Context.setCommandLineMode(true);
	RunProgram.run(
            className, 
            autorun, 
            probeRun,
            props,
            new SpecApplicationRunner()
            );
	RunProgram.done();
    }    

    private static java.util.Properties getBasicProperties() {
         java.util.Properties props = new java.util.Properties();
         try {
             java.io.FileInputStream fs = new java.io.FileInputStream("props/spec");    
             props.load(fs); 
             fs.close();                
         } catch(java.io.IOException x) {
             Context.out.println("Error reading props/spec"+x);
         }
         return props;
    }
}

class SpecApplicationRunner implements spec.harness.BenchmarkDone {

    public void benchmarkDone(String className, java.util.Properties results) {
	boolean firstTime = true;
	for( java.util.Enumeration e = results.keys() ; e.hasMoreElements() ; ) {
	    String key = (String)e.nextElement();
	    if( key.indexOf( "validity.error" ) != -1 ) {
		if (firstTime) {
		    Context.out.println("Validity check errors:");
		    firstTime = false;
		}	    
		Context.out.println(key+"="+results.get(key));
	    }
	}
    }
        
    public  void benchmarkPrint( String str ) {
	// just ignore
    }
}


class SpecApplicationFrame extends Frame {

    public SpecApplicationFrame( String str ) {
        super( str );
    }

    public boolean handleEvent(Event evt) {
        switch( evt.id ) {
        case Event.WINDOW_DESTROY:
            System.exit(0);
            return true;
        default:
            return super.handleEvent(evt);
        }
    }
}

