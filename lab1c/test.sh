#! /bin/sh
tmp=$0-$$.tmp
mkdir "$tmp" || exit

(
cd "$tmp" || exit

cat >test0.sh <<'EOF'
cp ../Makefile Makefile 
#case 1
../simpsh \
--verbose \
--rdonly Makefile \
--creat --trunc --wronly b \
--creat --append --wronly c \
--command 0 1 2 cat 


echo "test 1 finish!"

#case 2
../simpsh \
--verbose \
--rdonly Makefile \
--creat --trunc --wronly b \
--creat --append --wronly c \
--command 0 1 2 tr a-z A-Z 


echo "test 2 finish!"

#case 3
../simpsh \
--rdonly Makefile \
--pipe \
--pipe \
--creat --trunc --wronly b \
--creat --append --wronly c \
--catch 11 \
--command 0 2 6 sort \
--abort \
--command 1 4 6 cat b - \
--command 3 5 6 tr A-Z a-z \


echo "test 3 finish!"

#case 4 
../simpsh \
--rdonly Makefile \
--pipe \
--pipe \
--creat --trunc --wronly b \
--creat --append --wronly c \
--command 0 2 6 sort \
--command 1 4 6 cat b - \
--command 3 5 6 tr A-Z a-z \
 

echo "test 4 finish!"

#case 5 
../simpsh \
--verbose \
--rdonly Makefile \
--creat --trunc --wronly b \
--creat --append --wronly c \
--close 0 \
--command 0 1 2 cat

echo "test 5 finish!"


EOF

cat >test.exp <<'EOF'
--rdonly Makefile
--creat --trunc --wronly b
--creat --append --wronly c
--command 0 1 2 cat
test 1 finish!
--rdonly Makefile
--creat --trunc --wronly b
--creat --append --wronly c
--command 0 1 2 tr a-z A-Z
test 2 finish!
11 caught
test 3 finish!
test 4 finish!
--command: File 0 closed 
--rdonly Makefile
--creat --trunc --wronly b
--creat --append --wronly c
--close 0
test 5 finish!
EOF


chmod +x test0.sh 

./test0.sh > test.out 2>&1
diff -u test.exp test.out || exit


cd ../

rm -r "$tmp" 
)
