#! /bin/sh

#case 1
./simpsh \
--verbose \
--rdonly Makefile \
--creat --trunc --wronly b \
--creat --append --wronly c \
--command 0 1 2 cat

rm b
rm c

#case 2
./simpsh \
--verbose \
--rdonly Makefile \
--creat --trunc --wronly b \
--creat --append --wronly c \
--command 0 1 2 tr a-z A-Z

rm b
rm c


