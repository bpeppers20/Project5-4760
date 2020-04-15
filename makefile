all: OSS child

OSS: OSS.o scheduler1.o
	gcc OSS.o scheduler1.o -o OSS
	
OSS.o: OSS.c scheduler.h 
	gcc -c -g OSS.c
	
scheduler1.o: scheduler1.c scheduler.h
	gcc -c -g scheduler1.c

child: child.o
	gcc child.c -o child
	
child.o: child.c
	gcc -g child.c
	
clean:
	rm *.o *.txt *.out OSS child
