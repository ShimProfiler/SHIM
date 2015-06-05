#include "shim.h"
#define MAX_HW_COUNTERS (10)
#define INDEX_HW_COUNTERS (2)
int
main(int argc, char **argv)
{
  int i;
  int trustable;

  shim_init();
  shim * my = (shim *)calloc(1, sizeof(shim));
  shim_thread_init(my, 0, argc-1, argv+1);
  
  uint64_t begin[MAX_HW_COUNTERS];
  uint64_t end[MAX_HW_COUNTERS];
  
  shim_read_counters(begin, my);
  printf("hello world\n");
  shim_read_counters(end, my);
 
  trustable = shim_trustable_sample(begin, end, 99, 101);
  
  printf("Trustable samples:\n");
  for (i=0; i<my->nr_hw_events; i++){
    printf("%s, end: %lld, begin: %lld, diff: %lld\n",
	   my->hw_events[i].name,
	   (unsigned long long)end[INDEX_HW_COUNTERS + i],
	   (unsigned long long)begin[INDEX_HW_COUNTERS + i],
	   (unsigned long long)end[INDEX_HW_COUNTERS + i] - (unsigned long long)begin[INDEX_HW_COUNTERS + i]);
  }  
}
