#include <fd/proc.h>
#include <stdio.h>

void main(void) 
{
  dump_file_structures(stdout);  
  fflush(stdout);
}
