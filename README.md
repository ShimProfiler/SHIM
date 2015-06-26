SHIM is a high frequency profiler. The design and implementation of SHIM
is discussed in this paper (./papers/ISCA-SHIM.pdf) X. Yang, S. M. Blackburn, 
and K. S. McKinley, ”Computer Performance Microscopy with SHIM”, appeared in Proceedings of
the 42nd International Symposium on Computer Architecture (ISCA 2015) , 
Portland, OR, June 13-17, 2015.

Core data structures and functions of SHIM is in "shim.h" and "shim_core.c". Applications could
build different profilers based on them. JikesRVMProbes has an example for profiling JikesRVM.
Please have a look ./JikesRVMProbes/README.md for how to profile JikesRVM with SHIM.
