CC = g++
CFLAGS = -Wall -O3 -std=c++11
LIBS = -larmadillo

main.exe : main.o random.o lib.o
	$(CC) random.o lib.o main.o -o main.exe $(LIBS)
main.o : main.cpp
	$(CC) -c main.cpp -o main.o $(CFLAGS)
random.o : random.cpp random.h
	$(CC) -c random.cpp -o random.o $(CFLAGS)
lib.o : lib.cpp lib.h
	$(CC) -c lib.cpp -o lib.o $(CFLAGS)
clean :
	rm *.o main.exe seed.out
run : main.exe
	./main.exe




