all: main
COMMON_CFLAGS := -Wall -O2 -I src

main:
	gcc -c src/term.c -o build/term.o $(COMMON_CFLAGS)
	ar rcs build/cuoreterm.a build/term.o
	cp src/cuoreterm.h build/
	rm build/term.o

clean:
	rm build/term.o build/cuoreterm.a