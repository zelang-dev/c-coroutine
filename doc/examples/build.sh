cc -O3 -fomit-frame-pointer -D CO_DEBUG -I../.. -o coroutine.o -c ../../coroutine.c
c++ -O3 -fomit-frame-pointer -I../.. -c test_timing.cpp
c++ -O3 -fomit-frame-pointer -o test_timing coroutine.o test_timing.o
c++ -O3 -fomit-frame-pointer -I../.. -c test_args.cpp
c++ -O3 -fomit-frame-pointer -o test_args coroutine.o test_args.o
c++ -O3 -fomit-frame-pointer -I../.. -c example.cpp
c++ -O3 -fomit-frame-pointer -o example coroutine.o example.o
c++ -O3 -fomit-frame-pointer -I../.. -c testsuite.cpp
c++ -O3 -fomit-frame-pointer -o testsuite coroutine.o testsuite.o
c++ -O3 -fomit-frame-pointer -I../.. -c test_resume.cpp
c++ -O3 -fomit-frame-pointer -o test_resume coroutine.o test_resume.o
rm -f *.o
