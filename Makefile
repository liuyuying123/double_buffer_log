main: log.cpp main.cpp
	g++ -o $@ $^ -lpthread

clean:
	rm -rf main