CXX := gcc
CXXFLAGS := -Wall -g

TARGET := porter
SRC := porter.c
FISH_FILE := porter.fish
HEADERS := porter.h daemon.h

all: $(TARGET)

$(TARGET): $(SRC)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)

install: all
	sudo cp $(TARGET) /usr/bin/

installWFish: all
	sudo cp $(TARGET) /usr/bin/
	sudo cp $(FISH_FILE) /usr/share/fish/completions/

.PHONY: clean

clean:
	rm -f $(TARGET)
