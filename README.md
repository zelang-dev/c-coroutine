# c-coroutine

**c-coroutine** is a cooperative multithreading library written in C89. This library is a rework/refactor and merge of [libco](https://github.com/higan-emu/libco) and [minicoro](https://github.com/edubart/minicoro).

Although cooperative multithreading is limited to a single CPU core, it scales substantially better than preemptive multithreading.

For applications that need 100,000 or more context switches per second, the kernel overhead involved in preemptive multithreading can end up becoming the bottleneck in the application. c-coroutine can easily scale to 10,000,000 or more context switches per second.

Ideal use cases include servers (HTTP, RDBMS) and emulators (CPU cores, etc.)

It currently includes backends for:

* x86 CPUs
* amd64 CPUs
* PowerPC CPUs
* PowerPC64 ELFv1 CPUs
* PowerPC64 ELFv2 CPUs
* ARM 32-bit CPUs
* ARM 64-bit (AArch64) CPUs
* POSIX platforms (setjmp)
* Windows platforms (fibers)

See [doc/targets.md] for details.

See [doc/usage.md] for documentation.

## License

**c-coroutine** is released under the ISC license.
