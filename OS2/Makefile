hello:main.cpp my_print.o
	g++ -o hello main.cpp my_print.o -std=c++11

my_print.o:my_print.asm
	nasm -f elf -o my_print.o my_print.asm
