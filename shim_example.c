#include "shim.h"

void  shim_start_counters();
void  shim_end_counters();
void  shim_report_counters();

int
main(int argc, char **argv)
{
  bind_processor(1);
  shim_init(8);  
  shim_thread_init(argc-1, argv+1, 0, NULL, NULL, NULL);  

  shim_start_counters();
  printf("hello world\n");
  shim_end_counters();
  shim_report_counters();
}
