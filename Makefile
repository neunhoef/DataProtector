all: DataProtectorTest

DataProtectorTest:	DataProtectorTest.cpp DataGuardian.h Makefile DataProtector.h DataProtector.cpp
	g++ DataProtectorTest.cpp DataProtector.cpp -o DataProtectorTest -std=c++11 -Wall -O3 -g -lpthread
