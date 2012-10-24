all: verify cnfdd cnfuzz precochk lgl #mtcnfdd

cnfdd: cnfdd.c Makefile
	g++ -o cnfdd cnfdd.c -Wall -O0
	
cnfuzz: cnfuzz.c Makefile
	g++ -o cnfuzz cnfuzz.c -Wall -O0

mtcnfdd: mtcnfdd.c Makefile
	g++ -o mtcnfdd mtcnfdd.c -Wall -O3 -lpthread

precochk:
	g++ precochk.c -o precochk -lz

lgl:
	cd lingeling; ./configure && make; cp lingeling ../lgl

verify: 
	g++ verify_model.c -o verify -O3 

clean:
	rm -f mtcnfdd cnfuzz cnfdd precochk lgl verify
	cd lingeling; make clean
