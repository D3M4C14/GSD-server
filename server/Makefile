all: parse_cfg.o server

parse_cfg.o: parse_cfg.cpp parse_cfg.h
	g++ -Wall -std=c++11 -c parse_cfg.cpp -o  parse_cfg.o 

server: main.cpp parse_cfg.o
	g++ -Wall -std=c++11 parse_cfg.o main.cpp -o server

clean:
	rm *.o server
