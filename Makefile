all: main
COMMON_CFLAGS := -Wall -O2 -I src

main:
	mkdir -p build
	gcc -c src/term.c -o build/cuoreterm.o $(COMMON_CFLAGS)
	ar rcs build/cuoreterm.a build/cuoreterm.o
	cp src/cuoreterm.h build/

clean:
	rm -f build/cuoreterm.o build/cuoreterm.a