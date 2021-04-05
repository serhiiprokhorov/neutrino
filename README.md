# neutrino
neutrino project (exercise in process monitoring)

## Why?
This project is a part of remote process monitoring system, which will be implemented some day %).
Somewhere in a universe a system exists with only purpose to monitor remote processes.
System includes:
* PRODUCER, a process to monitor, produces a series of small messages (neutrinos) as it runs through its lifecycle
* CONSUMER_AGGREGATOR, a process which consumes neutrinos, matches them to pre-defined PATTERNS, sends them to CONSUMER_OVERWATCH
* PATTERN associates non-random series of neutrino messages (with possible exclusions) to a customer-defined name and category (good, bad, fatal, any other custom meaning...) 
* CONSUMER_OVERWATCH accepts PATTERNS and represents status of a processes.

This project represents C API to that system, withsome UT. 
Other components will follow soon.

**Do not attempt to use it, far from complete, out of pure curiosity.** 

---

## Status at 2021/04/05 03:46:PM
does not compile due to UT changes

## TODO:

* implement NETWORK byte order serializer
* implement JSON byte order serializer
* fix UT compilation problems
* ensure UT validates API -> consumer_stub(serializer -> buffered ep) -> channel -> endpoint_impl(deserializer -> consumer)
