all: simpleShell

simpleShell: simpleShell.cpp
	g++ -o simpleShell -std=c++11 simpleShell.cpp

clean:
	rm -rf simpleShell