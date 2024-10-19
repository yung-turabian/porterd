CXX := gcc
CXXFLAGS := -Wall -g

TARGET := porter
SRC := porter.c
HEADERS := porter.h daemon.h

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

install: all
	sudo mv $(TARGET) /usr/bin/

.PHONY: clean

clean:
	rm -f $(TARGET)
