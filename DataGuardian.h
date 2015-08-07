#include <mutex>
#include <atomic>
#include <unistd.h>

#include <iostream>

template<typename T, int maxNrThreads>
class DataGuardian {

    struct TPtr {
      std::atomic<T const*> ptr;
      char padding[64-sizeof(std::atomic<T const*>)];
    };

  public:
    DataGuardian () {
      _P[0].ptr = nullptr;
      _P[1].ptr = nullptr;
      for (int i = 0; i < maxNrThreads; i++) {
        _H[i].ptr = nullptr;
      }
      _V = 0;
    }

    ~DataGuardian () {
      std::lock_guard<std::mutex> lock(_mutex);
      while (isHazard(_P[_V].ptr)) {
        usleep(250);
      }
      T const* temp = _P[_V].ptr.load();
      delete temp;  // OK, if nullptr
      _P[_V].ptr = nullptr;
    }

    bool isHazard (T const* p) {
      for (int i = 0; i < maxNrThreads; i++) {
        T const* g = _H[i].ptr.load(std::memory_order_relaxed);
        if (g != nullptr && g == p) {
          return true;
        }
      }
      return false;
    }

    T const* lease (int myId) {
      int v;
      T const* p;

      while (true) {
        v = _V.load(std::memory_order_consume);           // (XXX)
        // This memory_order_consume corresponds to the change to _V
        // in exchange() below which uses memory_order_seq_cst, which
        // implies release semantics. This is important to ensure that
        // we see the changes to _P just before the version _V
        // is flipped.
        p = _P[v].ptr.load(std::memory_order_relaxed);
        _H[myId].ptr = p;                  // implicit memory_order_seq_cst
        if (_V.load(std::memory_order_relaxed) != v) {    // (YYY)
          _H[myId].ptr = nullptr;   // implicit memory_order_seq_cst
          continue;
        }
        break;
      };
      return p;
    }

    void unlease (int myId) {
      _H[myId].ptr = nullptr;   // implicit memory_order_seq_cst
    }

    void exchange (T const* replacement) {
      std::lock_guard<std::mutex> lock(_mutex);

      int v = _V.load(std::memory_order_relaxed);
      _P[1-v].ptr.store(replacement, std::memory_order_relaxed);
      _V = 1-v;      // implicit memory_order_seq_cst, whoever sees this
                     // also sees the two above modifications!
      // Our job is essentially done, we only need to destroy
      // the old value. However, this might be unsafe, because there might
      // be a reader. All readers have indicated their reading activity
      // with a store(std::memory_order_seq_cst) to _H[<theirId>]. After that
      // indication, they have rechecked the value of _V and have thus
      // confirmed that it was not yet changed. Therefore, we can simply
      // observe _H[*] and wait until none is equal to _P[v]:
      T const* p = _P[v].ptr.load(std::memory_order_relaxed);
      while (isHazard(p)) {
        usleep(250);
      }
      // Now it is safe to destroy _P[v]
      delete p;
      _P[v].ptr = nullptr;
    }

  private:
    TPtr _P[2];
    TPtr _H[maxNrThreads];
    std::atomic<int> _V;
    char padding3[64-sizeof(std::atomic<int>)];
    std::mutex _mutex;

  // Here is a proof that this is all OK: The mutex only ensures that there is
  // always only at most one mutating thread. All is standard, except that
  // we must ensure that whenever _V is changed the mutating thread knows
  // about all readers that are still using the old version, which is
  // done through _H[myId] where id is the id of a thread.
  // The critical argument needed is the following: Both the change to
  // _H[myId] in lease() and the change to _V in exchange() use
  // memory_order_seq_cst, therefore they happen in some sequential
  // order and all threads observe the same order. If the reader in line
  // (YYY) above sees the same value as before in line (XXX), then any
  // write to _V must be later in the total order of modifications than
  // the change to _H[myId]. Therefore the mutating thread must see the change
  // to _H[myId], after all, it sees its own change to _V. Therefore it is
  // ensured that the delete to _P[v] only happens when all reading threads
  // have terminated their lease through unlease().
};

