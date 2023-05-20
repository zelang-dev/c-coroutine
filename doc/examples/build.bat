@echo off
cl /I ../.. /D CO_DEBUG /c ../../c_coro.c
cl /I ../.. test_args.cpp c_coro.obj
cl /I ../.. test_timing.cpp c_coro.obj
cl /I ../.. example.cpp c_coro.obj
cl /I ../.. testsuite.cpp c_coro.obj
del *.obj
