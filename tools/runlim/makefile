runlim: runlim.c makefile VERSION
	gcc -Wall -O3 -DNDEBUG -DVERSION=\"`cat VERSION`\" -o runlim runlim.c -lutil
#	gcc -Wall -O0 -g  -DVERSION=\"`cat VERSION`\" -o runlim runlim.c -lutil

clean:
	rm -f runlim
.PHONY: clean
