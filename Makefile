CXX := gcc
CXXFLAGS := -Wall -g

TARGET := porterd
SRC := porterd.c
HEADERS := porterd.h daemon.h

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
