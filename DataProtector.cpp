#include "DataProtector.h"
template<int Nr> thread_local int DataProtector<Nr>::_mySlot = -1;
template class DataProtector<64>;
