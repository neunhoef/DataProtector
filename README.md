Lockfree protection of data structures that are frequently read
===============================================================

by Max Neunhoeffer and Jan Steemann

In multi-threaded applications running on multi-core systems, it occurs
often that there are certain data structures, which  are frequently read
but relatively seldom changed. An example of this would be a database
server that has a list of databases that changes rarely, but needs to be
consulted for every single query hitting the database. In such sitations
one needs to guarantee fast read access as well as protection against
inconsistencies, use after free and memory leaks.

Therefore we seek a lock-free protection mechanism that scales to lots
of threads on modern machines and uses only C++11 standard library
methods. The mechanism should be easy to use and easy to understand and
prove correct. This repository presents a solution to this, which is
probably not new, but which we still did not find anywhere else.

Usage:

    make
    ./DataGuardianTest 1 2 3 4 5 6 7 8

See the file `DataProtector.md` for more details about the code in this 
repository.

