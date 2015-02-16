.PHONY: all clean

all:  verify cnfdd cnfuzz lgl drat-trim

cnfdd: cnfdd.c Makefile
	gcc -o cnfdd cnfdd.c -Wall -O0
	
cnfuzz: cnfuzz.c Makefile
	gcc -o cnfuzz cnfuzz.c -Wall -O0

drat-trim: drat-trim.c
	gcc -O2 -o drat-trim drat-trim.c

lgl:
	cd lingeling; ./configure && make; cp lingeling ../lgl

verify: 
	g++ verify_model.c -o verify -O3 

clean:
	rm -f mtcnfdd cnfuzz cnfdd precochk lgl verify
	cd lingeling; make clean
	rm -f *.o
	rm -f certifiedSAT stc drup-check brup2drup
	rm -f drat-trim

