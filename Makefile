CXX := g++
CXXFLAGS := -Wall -g

TARGET := porterd
SRC := porterd.cpp

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

clean:
	rm -f $(TARGET)
