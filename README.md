# neutrino
neutrino project (exercise in process monitoring)

## Why?
Somewhere in a universe a system exists with only purpose to monitor remote processes.
System includes:
* PRODUCER, a process to monitor, produces a series of small messages (neutrinos) as it runs through its lifecycle
* CONSUMER_AGGREGATOR, a process which consumes neutrinos, matches them to pre-defined PATTERNS, sends them to CONSUMER_OVERWATCH
* PATTERN associates non-random series of neutrino messages (with possible exclusions) to a customer-defined name and category (good, bad, fatal, any other custom meaning...) 
* CONSUMER_OVERWATCH accepts PATTERNS and represents status of a processes.

This project implements C API to that system, also a transport components (serializers and buffers) and some UT. 
Other components will follow soon.

**Do not attempt to use it, far from complete, out of pure curiosity.** 

---

## Dependency
GTest via VCPKG

## Status at 2021/04/05 03:46:PM
does not compile due to UT changes

## TODO:
* legwork CMake x64
* DOXYGEN
* implement NETWORK byte order serializer
* implement JSON byte order serializer
* fix UT compilation problems
* ensure UT validates API -> consumer_stub(serializer -> buffered ep) -> channel -> endpoint_impl(deserializer -> consumer)
* async TCP/IP endpoint
* CLI with self test
* CONSUMER_AGGREGATOR executable
* CONSUMER_OVERWATCH backend

---
## VSCode

### setup ctest debug

https://github.com/microsoft/vscode-cmake-tools/blob/main/docs/debug-launch.md#debugging-tests

## Known issues

###  /lib/x86_64-linux-gnu/libstdc++.so.6: version `GLIBCXX_3.4.32' not found

    sudo add-apt-repository ppa:ubuntu-toolchain-r/test
    sudo apt-get update
    sudo apt-get install --only-upgrade libstdc++6

### 