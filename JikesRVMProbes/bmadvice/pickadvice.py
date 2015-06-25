#!/bin/python
import os,sys,re
import sys
import subprocess

usage= "python bestadvice.log logdir bestadvice.log in format as [benchmarkname invocation time], and time is in asecending order"

#affix in .ec
def copyfile(bname, invoc, targetdir,affix):
    command="find " + targetdir + " -name " + "\'" + bname + "*." + invoc + affix + "\'";
    p = subprocess.Popen(command,shell=True,stdout=subprocess.PIPE)
    filename = p.stdout.readline().rstrip('\n');
    print "copy " + filename  + " to current directory"
    copycommand= "cp " + filename + " ./" + bname + affix;
    subprocess.call(copycommand,shell=True);


def main(argv):
    if len(argv) != 3:
        print usage
    bestfilename = argv[1]
    bestf = open(bestfilename,'r');
    lastbenchmarkname="none"
    l = bestf.readline();

    while(l):
        benchname = l.split('\t')[0]
        invocation = l.split('\t')[-2]

        if (benchname != lastbenchmarkname):
            print "invocation " + invocation + " is best for " + benchname
            lastbenchmarkname = benchname
            copyfile(benchname, invocation, argv[2], ".ca");
            copyfile(benchname, invocation, argv[2], ".dc");
            copyfile(benchname, invocation, argv[2], ".ec");

        l = bestf.readline();




if __name__ == "__main__":
    print sys.argv
    main(sys.argv)
