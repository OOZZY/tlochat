# tlochat

A simple chat server.

## Build Requirements

* CMake
* C11 development environment for which CMake can generate build files
* Unix-like environment (Linux, FreeBSD, Cygwin, etc.)

## Clone, Build, and Run

Clone into tlochat directory.

```
$ git clone --branch develop --recursive <url/to/tlochat.git>
```

Build.

```
$ mkdir tlochatbuild
$ cd tlochatbuild
$ cmake -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug ../tlochat
$ make
```

Run.

```
$ ./src/tlochat_server
```
