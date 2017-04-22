# Clone, Build, and Run

```
$ git clone --recursive <url/to/tlochat.git> # clone into tlochat directory
$ mkdir tlochatbuild
$ cd tlochatbuild
$ cmake -G 'Unix Makefiles' -DCMAKE_BUILD_TYPE=Debug ../tlochat
$ make
$ ./tlochat_server
```
