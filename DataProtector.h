#include <atomic>
#include <unistd.h>

template<int Nr>
class DataProtector {
    struct alignas(64) Entry {
      std::atomic<int> _count;
    };

    Entry* _list;

    std::atomic<int> _last;
    static thread_local int _mySlot;

  public:

    // A class to automatically unuse the DataProtector:
    class UnUser {
        DataProtector* _prot;
        int _id;

      public:
        UnUser (DataProtector* p, int i) : _prot(p), _id(i) {
        }

        ~UnUser () {
          if (_prot != nullptr) {
            _prot->unUse(_id);
          }
        }

        // A move constructor
        UnUser (UnUser&& that) : _prot(that._prot), _id(that._id) {
          // Note that return value optimization will usually avoid
          // this move constructor completely. However, it has to be
          // present for the program to compile.
          that._prot = nullptr;
        }

        // Explicitly delete the others:
        UnUser (UnUser const& that) = delete;
        UnUser& operator= (UnUser const& that) = delete;
        UnUser& operator= (UnUser&& that) = delete;
        UnUser () = delete;
    };

    DataProtector () : _last(0) {
      _list = new Entry[Nr];
      // Just to be sure:
      for (size_t i = 0; i < Nr; i++) {
        _list[i]._count = 0;
      }
    }

    ~DataProtector () {
      delete[] _list;
    }

    UnUser use () {
      int id = _mySlot;
      if (id < 0) {
        id = _last++;
        if (_last > Nr) {
          _last = 0;
        }
        _mySlot = id;
      }
      _list[id]._count++;   // this is implicitly using memory_order_seq_cst
      return UnUser(this, id);  // return value optimization!
    }

    void scan () {
      for (size_t i = 0; i < Nr; i++) {
        while (_list[i]._count > 0) {
          usleep(250);
        }
      }
    }

  private:

    void unUse (int id) {
      _list[id]._count--;   // this is implicitly using memory_order_seq_cst
    }
};

