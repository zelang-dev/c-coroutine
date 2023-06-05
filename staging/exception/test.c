#include <stdlib.h>
#include <stdio.h>
#include "exception.h"

void test0(void) {
  static _Bool x = 1;
  try {
    if(x) {
      throw(42);
    } else {
      printf("C");
    };
    printf("B");
  } catch(42) {
    printf("A");
    x = 0;
    retry;
  };
};
void test1(void) {
  try {
    printf("D");
    throw(1);
    printf("E");
  }
  catch(1) comeback;
  finally printf("F");
};
void test2(void) {
  FILE *x;
  try {
    check x = fopen("foo.txt", "r");
    printf("H");
  } catch(2) {
    printf("G\n");
    comeback;
  };
};
void test3(void) {
  try {
    printf("I");
    try {
      printf("J");
      throw(42);
      printf("L");
      throw(51);
    } catch(51) {
      printf("M");
      throw(666);
    };
  } catch(42) {
    printf("K");
    comeback;
  } rescue {
    printf("N\n");
  } finally {
    printf("O");
  };
};
void test4(void) {
  struct foo {
    const char *bar;
    int baz;
  }
  a = {"P", 10},
  b = {"Q", 20},
  c = {"R", 30};
  try {
    throw(&a);
  } catch(&b) {
    printf("%s", b.bar);
    throw(&c);
  } catch(&a) {
    printf("%s", a.bar);
    throw(&b);
  } rescue {
    struct foo *err = (struct foo *)errno;
    printf("%s", err->bar);
  };
};
void test5(void) {
  try {
    throw(20);
  } catch(10) {
    printf("T");
  } rescue {
    printf("S");
    throw(10);
  };
};
void test6(void) {
  auto void recursion(int a) {
    if(a == 5) {
      printf("V");
    } else {
      throw(a * 10);
      recursion(a + 1);
    };
    throw(recursion);
  };
  try {
    recursion(0);
  } catch(40) {
    printf("U\n");
    comeback;
  } catch(recursion) {
    comeback;
  } rescue {
    comeback;
  };
};
void test7(void) {
  try {
    try {
      throw(10);
    } finally {
      printf("X");
    };
  } catch(10) {
    printf("W");
  } finally printf("Y");
};
void test8(void) {
  try {
    const char *a = "3\n";
    try {
      a = "2";
      throw(10);
      printf("%s", a);
    } finally {
      printf("%s", a);
    };
  } catch(10) {
    printf("1");
    comeback;
  };
};
void test9(void) {
  try {
    const char *a = "5";
    try {
      a = "foo";
      throw("foo");
    } catch("bar") {
      printf("%s\n", a);
    };
  } catch("foo") {
    printf("4");
    throw("bar");
  };
};

int main(int argc, char **argv) {
  try {
    test0();
    test1();
    test2();
    test3();
    test4();
    test5();
    test6();
    test7();
    test8();
    test9();
  };
  return EXIT_SUCCESS;
};
