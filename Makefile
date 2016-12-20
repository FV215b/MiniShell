cc = g++
cflags = -Werror -Wall -pedantic -std=gnu++11 -ggdb3
myShell: myShell.cpp
	$(cc) -o $@ $(cflags) $<
.PHONY: clean
clean:
	rm -f myShell
