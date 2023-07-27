#include "../include/coroutine.h"

static int div_err(int a, int b)
{
  if (b == 0)
    throw(division_by_zero);

  return a/b;
}

int co_main(int argc, char **argv)
{
  div_err(1,0);
  printf("never reached\n");

  return 0;
}
