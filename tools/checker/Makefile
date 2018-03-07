.PHONY: all clean

all: verify cnfdd cnfuzz drat-trim

cnfdd.c: src
cnfuzz.c: src
drat-trim.c: src

always:

src: always
	@curl https://www.cs.utexas.edu/~marijn/drat-trim/drat-trim.c > drat-trim.c
	@curl http://fmv.jku.at/cnfuzzdd/cnfuzzdd2013.zip > fuzz-suite.zip
	@unzip -j -n fuzz-suite.zip cnfuzzdd2013/cnfdd.c -d .
	@unzip -j -n fuzz-suite.zip cnfuzzdd2013/cnfuzz.c -d .

cnfdd: cnfdd.c Makefile
	gcc -o cnfdd cnfdd.c -Wall -O0
	
cnfuzz: cnfuzz.c Makefile
	gcc -o cnfuzz cnfuzz.c -Wall -O0 -lrt

drat-trim: drat-trim.c
	gcc -O2 -o drat-trim drat-trim.c -static -lrt

verify: verify_model.c
	g++ verify_model.c -o verify -O3 

clean:
	rm -f cnfuzz cnfdd verify
	rm -f *.o
	rm -f drat-trim

