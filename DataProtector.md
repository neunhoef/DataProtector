Lockfree protection of data structures that are frequently read
===============================================================

by Max Neunhoeffer and Jan Steemann


Motivation
----------

In multi-threaded applications running on multi-core systems, it occurs
often that there are certain data structures that are frequently read
but relatively seldom changed. An example of this would be a database
server that has a list of databases that changes rarely, but needs to be
consulted for every single query hitting the database. In such sitations
one needs to guarantee fast read access as well as protection against
inconsistencies, use after free and memory leaks.

Therefore we seek a lock-free protection mechanism that scales to lots
of threads on modern machines and uses only C++11 standard library
methods. The mechanism should be easy to use and easy to understand and
prove correct. This article presents a solution to this, which is
probably not new, but which we still did not find anywhere else.


The concrete challenge at hand
------------------------------

Assume a global data structure on the heap and a single atomic pointer
P to it. If (fast) readers access this completely unprotected, then
a (slow) writer can create a completely new data structure and then
change the pointer to the new structure with an atomic operation. Since
writing is not time critical, one can easily use a mutex to ensure that
there is only a single writer at any given time. The only problem is to
decide, when it is safe to destruct the old value, because the writer
cannot easily know that no reader is still accessing the old values. The
challenge is aggravated by the fact that without thread synchronization
it is unclear, when a reader actually sees the new pointer value, in
particular on a multi-core machine with a complex system of caches.

If you want to see our solution directly, scroll down to "Source code
links". We first present a classical good approach and then try to
improve on it.


Hazard pointers and their hazards
---------------------------------

The "classical" lock-free solution to this problems are hazard pointers
(see [this paper][1] and this [article on Dr Dobbs][2]). 
The basic idea is that each reading thread first registers the location
of its own "hazard" pointer in some list, and whenever it wants to
access the data structure, it sets its own hazard pointer to the value
of P it uses to access the data, and restores it to `nullptr` when it is
done with the read access.

A writer can then replace the old value of P with a pointer to a
completely new value and then scan all registered hazard pointers to see
whether any thread still accesses the old value. If all store operations
to the hazard pointers and the one to P use `memory_order_seq_cst` (see
[this page][3] for an explanation), then it is guaranteed that if reader
thread sees the old version of P, then it observes the change of its own
hazard pointer earlier, therefore, because of the guaranteed sequential
order of all stores with `memory_order_seq_cst`, the writer thread also
observes the hazard pointer value before its own change of P.

This is a very powerful and neat argument, and it only uses the
guaranteed memory model of the C++11 standard in connection with atomic
operations in the STL. It has very good performance characteristics,
because the readers just have to ensure `memory_order_seq_cst` by means
of memory fence or similar instructions, and since one can assume that
the actual hazard pointers reside in different cache lines there is no
unnecessary cache invalidation.

However, this approach is not without its own hazards (pun intended).
The practical problems in my opinion lie in the management of the hazard
pointer allocations and deallocations and from the awkward registration
procedure. A complex multi-threaded application can have various
different types of threads, some dynamically created and joined. At the
same time it can have multiple data structures that need the sort of
protection discussed here. The position of the actual hazard pointer 
structure is thread-local information, and one needs a different one
for each instance of a data structure that needs protection. 

What makes matters worse is that at the time of thread creation the
main thread function often does not have access to the protected data at
all, due to data encapsulation and object-oriented design. One also does
not want to do the allocation of hazard pointer structure lazily, since
this hurts the fast path for read access. 

If one were to design a "DataGuardian" class that does all the
management of hazard pointers itself, then it would have to store the
locations of the hazard pointers in thread-local data, but then it would
have to be static and it would thus not be possible to use different
hazard pointers for different instances of the DataGuardian. We have
actually tried this and failed to deliver a simple and convenient
implementation. This frustration lead us to our solution, which we
describe next.


Lock-free reference counting
----------------------------

The fundamental idea is to use a special kind of reference counting
in which a reading thread uses atomic compare-and-exchange operations 
to increase a reference counter before it reads
P and the corresponding data and decreases it after it is done with the
reading. However, the crucial difference to this standard approach
is that every thread uses a different counter, all residing in pairwise
different cache lines! This is important since it means that the
compare-and-exchange operations are relatively quick since no contention
with corresponding cache invalidations happens.

This is actually a very simple approach: We administrate multiple
slots with reference counters, making sure that each resides in a
different cache line. Explain slow writer checking them all. 
Each thread chooses once and for all a slot (store number in
static thread-local storage), valid for each instance of
the DataProtector class. Put reference counters in different cache
lines by alignment. Use `memory_order_seq_cst` as above.

Number of slots is template parameter.

Present code without unuser.

Present proof that it works, similar to above.

All management in `DataProtector` class, simple usage, good
encapsulation.


Convenience with scope guards
-----------------------------

Add a mechanism for scope guards with an `UnUser` class, encapsulated in
the `DataProtector` class. It needs to store a pointer to the
`DataProtector` instance and the slot number. Return value
optimization for performance. `auto` for short code.

Present usage.

Present code.



Performance comparison with other methods
-----------------------------------------

Compare

  1. `DataGuardian` with hazard pointers
  2. unprotected access
  3. a mutex implementation
  4. a spin-lock implementation using boost atomics
  5. `DataProtector`

Explain each method briefly.

Link to test code on github.


Source code links
-----------------

Put everything into github, link to ArangoDB repo.


References
----------

[1]: http://www.research.ibm.com/people/m/michael/ieeetpds-2004.pdf "Hazard pointer article"
[2]: http://www.drdobbs.com/lock-free-data-structures-with-hazard-po/184401890 "Dr Dobbs"
[3]: http://www.cplusplus.com/reference/atomic/memory_order/   "Memory orders in C++"
