# minilogger
c++ mini logger


## build & test
build static/dynamic library:
```
mkdir build && cd build
cmake .. && cmake --build .
```

build test coverage:
```
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Coverage && cmake --build .
make minilogger_lcov_test # lcov
make minilogger_gcovr_test # gcovr
```
