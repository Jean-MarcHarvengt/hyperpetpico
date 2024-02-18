tail -c +5 starwars.bin > bitmap.bin
make
./pucrunch -c0 -d bitmap.bin bitmap.cru
gcc decrunch.c -o decrunch
./decrunch 
