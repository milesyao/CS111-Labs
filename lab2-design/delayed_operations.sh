#! /bin/sh

./osprdaccess -w -z
(sleep 10.0; echo sjtu | ./osprdaccess -w -o 9000) &
(sleep 10.5; cat data12 | ./osprdaccess -w -o 3100) &
(sleep 11.0; ./osprdaccess -w -z) &






