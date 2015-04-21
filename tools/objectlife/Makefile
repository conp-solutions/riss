INJECTORS = myMem.so

all: $(INJECTORS)

%.so: %.c
	g++ -Wall -Wextra -O3 $< -fPIC -shared -o $@ -ldl $(ARGS)

clean:
	rm -rf $(INJECTORS)
