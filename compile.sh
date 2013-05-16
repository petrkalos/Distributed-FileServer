#rm -f client client.o mynfs.o fileservice database.txt files.txt

gcc -Wall -c mynfs.c
gcc -Wall -c cmynfs.c -lm
gcc -Wall -c client.c

gcc -Wall -o client client.o cmynfs.o mynfs.o -lm

#gcc -Wall directoryservice.c -o directoryservice
#gcc -Wall fileservice.c -o fileservice
