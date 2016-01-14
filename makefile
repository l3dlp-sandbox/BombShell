sh: sh.o
	g++ sh.o -o sh

sh.o: sh.cpp
	g++ -c sh.cpp

clean:
	rm sh.o sh