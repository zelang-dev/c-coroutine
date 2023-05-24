@echo off
cl /I ../.. /D CO_DEBUG /c ../../coroutine.c
cl /I ../.. test_args.cpp coroutine.obj
cl /I ../.. test_timing.cpp coroutine.obj
cl /I ../.. example.cpp coroutine.obj
cl /I ../.. testsuite.cpp coroutine.obj
cl /I ../.. test_resume.cpp coroutine.obj
del *.obj
