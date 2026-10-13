#include "fff.h"
#include "main.h"
#include <assert.h>
#include <stdio.h>

void setup();
#define TEST_F(SUITE, NAME) void NAME()
#define RUN_TEST(SUITE, TESTNAME)                                                                  \
  printf(" Running %s.%s: \n", #SUITE, #TESTNAME);                                                 \
  setup();                                                                                         \
  TESTNAME();                                                                                      \
  printf(" SUCCESS\n");

DEFINE_FFF_GLOBALS;
FAKE_VALUE_FUNC(int, hal_function);

// reset before every test
void setup()
{
  RESET_FAKE(hal_function);
  FFF_RESET_HISTORY();
}

TEST_F(HelloWorld, hello_world_calls_hal_function)
{
  hello_world();
  assert(hal_function_fake.call_count == 1);
}

int main()
{
  setbuf(stdout, NULL);
  fprintf(stdout, "-------------\n");
  fprintf(stdout, "Running Tests\n");
  fprintf(stdout, "-------------\n\n");
  fflush(0);

  RUN_TEST(HelloWorld, hello_world_calls_hal_funciton);

  printf("\n-------------\n");
  printf("Complete\n");
  printf("-------------\n\n");

  return 0;
}