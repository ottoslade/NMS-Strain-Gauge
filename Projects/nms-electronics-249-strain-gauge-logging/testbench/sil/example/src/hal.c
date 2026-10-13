#include "hal.h"
#include <stdio.h>

int hal_function()
{
  printf("This should never get printed while testing");
  return 0;
}