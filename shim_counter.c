#include "shim.h"

void shim_start_counters(void)
{
  shim *my = get_myshim();  
  shim_read_counters(my->begin_counters,my);
}

void shim_end_counters(void)
{
  shim *my = get_myshim();  
  shim_read_counters(my->end_counters,my);
}

void shim_report_counters(void)
{
  shim *my = get_myshim();  
  uint64_t val = 0;
  uint64_t begin, end;
  int i;

  for (i=0; i<my->nr_hw_events;i++){
    begin = my->begin_counters[HW_COUNTERS_BASE + i];
    end = my->end_counters[HW_COUNTERS_BASE + i];
    val =  end - begin;
    printf("HW:%s, end:%lld, begin:%lld, val:%lld\n", my->hw_events[i].name, end, begin, val);
  }
  printf("trustable :%d\n", shim_trustable_sample(my->begin_counters, my->end_counters, my));  
}
