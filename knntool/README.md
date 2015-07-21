# KNN Tool

## Requirements

 * boost-program-options


```bash
# if you have system access, you can install it simply with
$ sudo apt-get install libboost-program-options-dev
```

If you do not have global access to your system, you can build/install Boost
locally and point CMake to your local installation

```bash
# inside the build directory of riss
$ cmake -D Boost_DIR=<path/to/local/boost>  <other options> ..
```
