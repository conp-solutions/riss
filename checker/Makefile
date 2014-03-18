.PHONY: all clean

all: certifiedSAT stc drup-check brup2drup verify cnfdd cnfuzz precochk lgl mtcnfdd wcnfuzz drat-trim 

certifiedSAT: certifiedSAT.cc 
	g++ certifiedSAT.cc -o certifiedSAT -O0 -g

drat-trim: drat-trim.c
	gcc -Wall -o drat-trim -O3 drat-trim.c -std=c99
	
stc: stc.c
	gcc stc.c -o stc -O2 -static
	
drup-check: drup-check.c
	gcc drup-check.c -o drup-check -O2 -static

brup2drup: brup2drup.c
	gcc brup2drup.c -o brup2drup -O2 -static


cnfdd: cnfdd.c Makefile
	gcc -o cnfdd cnfdd.c -Wall -O0
	
cnfuzz: cnfuzz.c Makefile
	gcc -o cnfuzz cnfuzz.c -Wall -O0

mtcnfdd: mtcnfdd.c Makefile
	gcc -o mtcnfdd mtcnfdd.c -Wall -O3 -lpthread

precochk:
	gcc precochk.c -o precochk -lz

lgl:
	cd lingeling; ./configure && make; cp lingeling ../lgl

verify: 
	g++ verify_model.c -o verify -O3 

wcnfuzz: wcnfuzz.cc
	g++ wcnfuzz.cc -o wcnfuzz -lrt

clean:
	rm -f mtcnfdd cnfuzz cnfdd precochk lgl verify
	cd lingeling; make clean
	rm -f *.o
	rm -f certifiedSAT stc drup-check brup2drup
	rm -f drat-trim

