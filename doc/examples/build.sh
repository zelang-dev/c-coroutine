cc -O3 -fomit-frame-pointer -D CO_DEBUG -I../.. -o c_coro.o -c ../../c_coro.c
c++ -O3 -fomit-frame-pointer -I../.. -c test_timing.cpp
c++ -O3 -fomit-frame-pointer -o test_timing c_coro.o test_timing.o
c++ -O3 -fomit-frame-pointer -I../.. -c test_args.cpp
c++ -O3 -fomit-frame-pointer -o test_args c_coro.o test_args.o
c++ -O3 -fomit-frame-pointer -I../.. -c example.cpp
c++ -O3 -fomit-frame-pointer -o example c_coro.o example.o
c++ -O3 -fomit-frame-pointer -I../.. -c testsuite.cpp
c++ -O3 -fomit-frame-pointer -o testsuite c_coro.o testsuite.o
rm -f *.o
