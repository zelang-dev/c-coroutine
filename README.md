# c-coroutine

[![windows & linux & macOS](https://github.com/zelang-dev/c-coroutine/actions/workflows/ci.yml/badge.svg)](https://github.com/zelang-dev/c-coroutine/actions/workflows/ci.yml)[![macOS](https://github.com/zelang-dev/c-coroutine/actions/workflows/ci_macos.yml/badge.svg)](https://github.com/zelang-dev/c-coroutine/actions/workflows/ci_macos.yml)[![armv7, aarch64, ppc64le](https://github.com/zelang-dev/c-coroutine/actions/workflows/ci_qemu_others.yml/badge.svg)](https://github.com/zelang-dev/c-coroutine/actions/workflows/ci_qemu_others.yml)[![riscv64 & s390x by ucontext  .](https://github.com/zelang-dev/c-coroutine/actions/workflows/ci_qemu.yml/badge.svg)](https://github.com/zelang-dev/c-coroutine/actions/workflows/ci_qemu.yml)

## Table of Contents

* [Introduction](#introduction)
* [Synopsis](#synopsis)
* [Usage](#usage)
* [Installation](#installation)
* [Contributing](#contributing)
* [License](#license)

## Introduction

This branch `main`, is now an **WIP** to incorporate [libuv](http://docs.libuv.org) with [c-raii](https://github.com/zelang-dev/c-raii). See `branches` for previous setup.

## Synopsis

```c

```

## Usage

### See [examples](https://github.com/zelang-dev/c-coroutine/tree/main/examples) folder

## Installation

The build system uses **cmake**, that produces _single_ **static** library stored under `built`, and the complete `include` folder is needed.

**Linux**

```shell
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Debug/Release -DBUILD_EXAMPLES=ON # use to build files in examples folder
cmake --build .
```

**Windows**

```shell
mkdir build
cd build
cmake .. -D BUILD_EXAMPLES=ON # use to build files in examples folder
cmake --build . --config Debug/Release
```

## Contributing

Contributions are encouraged and welcome; I am always happy to get feedback or pull requests on Github :) Create [Github Issues](https://github.com/zelang-dev/c-coroutine/issues) for bugs and new features and comment on the ones you are interested in.

## License

The MIT License (MIT). Please see [License File](LICENSE.md) for more information.
