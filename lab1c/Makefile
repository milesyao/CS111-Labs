CC = gcc
CFLAGS = -Wall -O
SRCS = main.c 
AUX = Makefile test.sh README proj1c_report.pdf

all: 
	${CC} ${CFLAGS} ${SRCS} -o simpsh

dist: ${SRCS} ${AUX}
	echo lab1-'ChunnanYao' > .fname
	mkdir `cat .fname`
	ln ${SRCS} ${AUX} `cat .fname`
	tar -czf `cat .fname`.tar.gz `cat .fname`
	-rm -rf `cat .fname`

check:
	./test.sh 	
	@echo "Test Finish."

clean:
	rm simpsh
