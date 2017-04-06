.PHONY: all
all : libcrawler.so file_tester 

file_tester : file_tester.c libcrawler.so
	gcc -g -L. -lcrawler -lpthread file_tester.c -Wall  -o file_tester -pthread

libcrawler.so : crawler.c
	gcc -g -fpic -c crawler.c -Wall -o crawler.o -pthread 
	gcc -g -shared -o libcrawler.so crawler.o

.PHONY: clean
clean :
	rm -f file_tester libcrawler.so *.o *~


