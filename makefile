sh: sh.o
	g++ sh.o -o sh

sh.o: sh.cpp
	g++ -c -std=c++11 sh.cpp

clean:
	rm sh.o sh