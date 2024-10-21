CXX := gcc
CXXFLAGS := -Wall -g -DDEBUG

TARGET := porter
SRC := porter.c extio.c DaemonUtil.c
FISH_FILE := porter.fish
HEADERS := porter.h DaemonUtil.h

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

install: all
	sudo cp $(TARGET) /usr/bin/
	sudo cp $(FISH_FILE) /usr/share/fish/completions/

.PHONY: clean

clean:
	rm -f $(TARGET)
