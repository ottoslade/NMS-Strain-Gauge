#include "hal.h"
#include <stdio.h>

void hello_world()
{
  hal_function();
  printf("Hello World!");
}

#ifndef NMS_TESTING // selectively compile main() based on whether we are testing or not
int main(int argc, char *argv[])
{
  hello_world();
  return 0;
}
#endif // NMS_TESTING