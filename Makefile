HTTPSERVER_INCLUDE=./include

INCLUDE=-I$(HTTPSERVER_INCLUDE)

BINARY=$(patsubst %.cpp,%.o,$(wildcard *.cpp))
FLAG=-g -Wall -lpthread
TARGET=HttpServer
all:$(TARGET)

$(TARGET):$(BINARY)
	g++ $(FLAG) $^ -o $@

%.o:%.cpp
	g++ $(FLAG) $< -o $@ -c $(INCLUDE)

clean:
	rm -rf *.o

