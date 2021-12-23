all: work.o parse_cfg.o server

work.o: work.cpp work.h
	g++ -Wall -std=c++11 -c work.cpp -lpthread -o work.o

parse_cfg.o: parse_cfg.cpp parse_cfg.h
	g++ -Wall -std=c++11 -c parse_cfg.cpp -o parse_cfg.o 

server: main.cpp parse_cfg.o work.o
	g++ -Wall -std=c++11 work.o parse_cfg.o main.cpp -lpthread  -o server

clean:
	rm *.o server
