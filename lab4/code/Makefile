CC = gcc
CFLAGS = -Wall -O
AUX = Makefile 

all: 
	${CC} ${CFLAGS} addtest.c -o addtest -pthread -lrt
	${CC} ${CFLAGS} sltest.c SortedList.c -o sltest -pthread -lrt

dist: ${SRCS} ${AUX}
	echo lab1-'ChunnanYao' > .fname
	mkdir `cat .fname`
	ln ${SRCS} ${AUX} `cat .fname`
	tar -czf `cat .fname`.tar.gz `cat .fname`
	-rm -rf `cat .fname`

clean:
	rm addtest
	rm sltest
