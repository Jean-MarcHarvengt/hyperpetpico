#tail -c +5 starwars.bin > bitmap.bin
make
./pucrunch -c0 -d $1.raw bmp_$1.cru
#./pucrunch -c0 -d $1.raw bitmap.cru
#gcc decrunch.c -o decrunch
#./decrunch 
#mv bitmap.h $1.h
