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
	sudo mv $(TARGET) /usr/bin/

fish_completions:
	mv $(FISH_FILE) $(HOME)/.config/fish/completions/

.PHONY: clean

clean:
	rm -f $(TARGET)
