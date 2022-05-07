all: server client

server: bin/sdstored

client: bin/sdstore

bin/sdstored: obj/sdstored.o
	gcc -O2 -g obj/sdstored.o -o bin/sdstored

obj/sdstored.o: src/sdstored.c
	gcc -O2 -Wall -g -c src/sdstored.c -o obj/sdstored.o

bin/sdstore: obj/sdstore.o
	gcc -O2 -g obj/sdstore.o -o bin/sdstore
	
obj/sdstore.o: src/sdstore.c
	gcc -O2 -Wall -g -c src/sdstore.c -o obj/sdstore.o
	
dir:
	mkdir bin obj tmp
	
clean:
	rm obj/* bin/{sdstore,sdstored}  tmp/*