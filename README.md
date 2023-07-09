# Simple C compiler

To build this, simply execute from top level directory
### If you're on GNU compatible system
```
cmake -B build
cd build
make
```

### If you're on Windows
```
cmake -B build
open vs sln and build
```

By default this does a debug build. Switch this by setting the CMAKE_BUILD_TYPE variable to "Release"