#include "DataGuardian.h"
#include "DataProtector.h"

#include <iostream>
#include <thread>
#include <vector>
#include <time.h>

#include <boost/atomic.hpp>

class SpinLock
{
public:
    void Acquire()
    {
        while (true)
        {
            if (!m_locked.test_and_set(boost::memory_order_acquire))
            {
                return;
            }
            usleep(250);
        }
    }
    void Release()
    {
        m_locked.clear(boost::memory_order_release);
    }

private:
    boost::atomic_flag m_locked;
};

#define T 10
#define maxN 64

using namespace std;

struct Dingsbums {
    Dingsbums(int i) : nr(i), isseganz(true) {
    };
    ~Dingsbums() {
      isseganz = false;
    }
    int nr;
    bool isseganz;
};

Dingsbums const* unprotected = nullptr;
DataGuardian<Dingsbums, maxN> guardian;

atomic<Dingsbums*> geschuetzt(nullptr);

DataProtector<64> prot;

mutex mut;
SpinLock spin;

uint64_t total = 0;

atomic<uint64_t> nullptrsSeen;
atomic<uint64_t> alarmsSeen;

void reader_good (int id) {
  uint64_t count = 0;
  time_t start = time(nullptr);
  while (time(nullptr) < start + T) {
    for (int i = 0; i < 1000; i++) {
      count++;
      Dingsbums const* p = guardian.lease(id);
      if (p == nullptr) {
        nullptrsSeen++;
      }
      else {
        if (! p->isseganz) {
          alarmsSeen++;
        }
      }
      guardian.unlease(id);
    }
  }
  lock_guard<mutex> locker(mut);
  total += count;
}

void reader_protector (int id) {
  uint64_t count = 0;
  time_t start = time(nullptr);
  while (time(nullptr) < start + T) {
    for (int i = 0; i < 1000; i++) {
      count++;
      //int id = prot.use();
      auto unuser(prot.use());
      Dingsbums const* p = geschuetzt;
      if (p == nullptr) {
        nullptrsSeen++;
      }
      else {
        if (! p->isseganz) {
          alarmsSeen++;
        }
      }
      //prot.unUse(id);
    }
  }
  lock_guard<mutex> locker(mut);
  total += count;
}

void reader_unprotected (int) {
  uint64_t count = 0;
  time_t start = time(nullptr);
  while (time(nullptr) < start + T) {
    for (int i = 0; i < 1000; i++) {
      count++;
      Dingsbums const* p = unprotected;
      if (p == nullptr) {
        nullptrsSeen++;
      }
      else {
        if (! p->isseganz) {
          alarmsSeen++;
        }
      }
    }
  }
  lock_guard<mutex> locker(mut);
  total += count;
}

void reader_mutex (int) {
  uint64_t count = 0;
  time_t start = time(nullptr);
  while (time(nullptr) < start + T) {
    for (int i = 0; i < 1000; i++) {
      count++;
      lock_guard<mutex> locker(mut);
      Dingsbums const* p = unprotected;
      if (p == nullptr) {
        nullptrsSeen++;
      }
      else {
        if (! p->isseganz) {
          alarmsSeen++;
        }
      }
    }
  }
  lock_guard<mutex> locker(mut);
  total += count;
}

void reader_spinlock (int) {
  uint64_t count = 0;
  time_t start = time(nullptr);
  while (time(nullptr) < start + T) {
    for (int i = 0; i < 1000; i++) {
      count++;
      spin.Acquire();
      Dingsbums const* p = unprotected;
      if (p == nullptr) {
        nullptrsSeen++;
      }
      else {
        if (! p->isseganz) {
          alarmsSeen++;
        }
      }
      spin.Release();
    }
  }
  lock_guard<mutex> locker(mut);
  total += count;
}


void writer_good () {
  Dingsbums* p;
  for (int i = 0; i < T+2; i++) {
    p = new Dingsbums(i);
    guardian.exchange(p);
    usleep(1000000);
  }
  guardian.exchange(nullptr);
}

void writer_protector () {
  Dingsbums* p;
  Dingsbums* q;
  for (int i = 0; i < T+2; i++) {
    p = new Dingsbums(i);
    q = geschuetzt;
    geschuetzt = p;
    prot.scan();
    delete q;
    usleep(1000000);
  }
  q = geschuetzt;
  geschuetzt = nullptr;
  prot.scan();
  delete q;
}

void writer_unprotected () {
  Dingsbums* p;
  for (int i = 0; i < T+2; i++) {
    Dingsbums const* q = unprotected;
    p = new Dingsbums(i);
    unprotected = p;
    usleep(1000000);
    delete q;
  }
  delete unprotected;
  unprotected = nullptr;
}

void writer_mutex () {
  Dingsbums* p;
  for (int i = 0; i < T+2; i++) {
    p = new Dingsbums(i);
    {
      lock_guard<mutex> locker(mut);
      delete unprotected;
      unprotected = p;
    }
    usleep(1000000);
  }
  delete unprotected;
  unprotected = nullptr;
}

void writer_spinlock () {
  Dingsbums* p;
  for (int i = 0; i < T+2; i++) {
    p = new Dingsbums(i);
    {
      spin.Acquire();
      delete unprotected;
      unprotected = p;
      spin.Release();
    }
    usleep(1000000);
  }
  delete unprotected;
  unprotected = nullptr;
}

char const* modes[] = {"guardian", "unprotected", "std::mutex", "spinlock",
                       "protector"};

int main (int argc, char* argv[]) {
  std::vector<double> totals;
  std::vector<double> perthread;
  std::vector<int> nrThreads;

  for (int mode = 0; mode < 5; mode++) {
    for (int j = 1; j < argc; j++) {
      nullptrsSeen = 0;
      alarmsSeen = 0;
      total = 0;
      int N = atoi(argv[j]);
      cout << "Mode: " << modes[mode] << endl;
      cout << "Nr of threads: " << N << endl;
      vector<thread> viele;
      viele.reserve(N);
      thread* nocheiner;

      switch (mode) {
        case 0: nocheiner = new thread(writer_good); break;
        case 1: nocheiner = new thread(writer_unprotected); break;
        case 2: nocheiner = new thread(writer_mutex); break;
        case 3: nocheiner = new thread(writer_spinlock); break;
        case 4: nocheiner = new thread(writer_protector); break;
      }
      
      usleep(500000);
      for (int i = 0; i < N; i++) {
        switch (mode) {
          case 0: viele.emplace_back(reader_good, i); break;
          case 1: viele.emplace_back(reader_unprotected, i); break;
          case 2: viele.emplace_back(reader_mutex, i); break;
          case 3: viele.emplace_back(reader_spinlock, i); break;
          case 4: viele.emplace_back(reader_protector, i); break;
        }
      }
      nocheiner->join();
      for (int i = 0; i < N; i++) {
        viele[i].join();
      }
      delete nocheiner;
      nocheiner = nullptr;
      cout << "Total: " << total/1000000.0/T << "M/s, per thread: "
                        << total/1000000.0/N/T << "M/(thread*s)" << endl;
      cout << "nullptr values seen: " << nullptrsSeen
           << ", alarms seen: " << alarmsSeen << endl << endl;
      totals.push_back(total/1000000.0/T);
      perthread.push_back(total/1000000.0/N/T);
      nrThreads.push_back(N);
    }
  }
  for (size_t i = 0; i < totals.size(); i++) {
    std::cout << i << "\t" << nrThreads[i] << "\t" << totals[i] << "\t"
              << perthread[i] << std::endl;
  }
  return 0;
}

