#include "DataProtector.h"
template<int Nr> thread_local int DataProtector<Nr>::_mySlot = -1;
template<int Nr> std::atomic<int> DataProtector<Nr>::_last(0);
template class DataProtector<64>;
