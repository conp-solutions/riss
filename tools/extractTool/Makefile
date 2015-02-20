all: extract select formatResults extractBMC

extract: extract.cc Makefile
	g++ -O3 -o extract extract.cc -static

extractArgumentation: extractArgumentation.cc Makefile
	g++ -O3 -o extractArgumentation extractArgumentation.cc -static

extractBMC: extractBMC.cc Makefile
	g++ -O3 -o extractBMC extractBMC.cc -static
	
extractPcasso: extractPcasso.cc Makefile
	g++ -O3 -o extractPcasso extractPcasso.cc -static
	
select: select.cc Makefile
	g++ -O3 -o select select.cc -static

formatResults: formatResults.cc Makefile
	g++ -O0 -g -o formatResults formatResults.cc -static

clean:
	rm -f extract select extractBMC extractPcasso formatResults
