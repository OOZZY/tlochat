# tlochat

A simple chat server.

## Build Requirements

* CMake
* C11 development environment for which CMake can generate build files
* Unix-like environment (Linux, FreeBSD, Cygwin, etc.)

## Clone, Build, and Run

```
$ git clone --recursive <url/to/tlochat.git> # clone into tlochat directory
$ mkdir tlochatbuild
$ cd tlochatbuild
$ cmake -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug ../tlochat
$ make
$ ./tlochat_server
```
