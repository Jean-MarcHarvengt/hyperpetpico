

#include <stdio.h>
#include <stdlib.h>


int processFile(char * infile, char * outfile, char * arrname) 
{
  FILE *fp_rd = stdin;
  FILE *fp_wr = stdout;

  int cnt=0;

  if ((fp_rd = fopen (infile, "rb")) == NULL)
  {
    fprintf (stderr, "Error:  can not open file %s\n", infile);
    return -1;  	
  }	
  if ((fp_wr = fopen (outfile, "wb")) == NULL)
  {
    fprintf (stderr, "Error:  can not create file %s\n", outfile);
    return -1;  	
  }
  

  fseek(fp_rd, 0L, SEEK_END);
  int size = ftell(fp_rd);
  fseek(fp_rd, 0L, SEEK_SET);

  printf ("Reading %d bytes\n", size);
  fprintf(fp_wr, "const UBYTE %s[%d] = {\n", arrname, size);

  cnt = 0;
  for (int i = 0; i < size; i++) {
  	unsigned char b;
  	if (fread(&b, 1, 1, fp_rd) != 1) {
  		fprintf (stderr, "Error:  can not read more bytes\n");
   		fclose (fp_wr);
  		fclose (fp_rd);   		
  		return -1;
  	}
    //b = ~b;	
    cnt++;
    if (cnt == 16) {
      fprintf(fp_wr, "0x%02X,\n",b);
    }  
    else {
      fprintf(fp_wr, "0x%02X,",b);
    }  
    cnt &= 15;
  }  
  fprintf(fp_wr, "};\n");

  fclose (fp_wr);
  fclose (fp_rd);
  return 1;  
}


int main(int argc, char *argv[]) {


  if (processFile("edit-4-80-b-60Hz.901474-03.bin","edit480.h","edit480") < 0)
    return (-1);

  if (processFile("edit-4-40-b-60Hz.ts.bin","edit4.h","edit4") < 0)
    return (-1);

  if (processFile("edit-4-80-b-50Hz.901474-04-0283.bin","edit48050.h","edit480") < 0)
    return (-1);

  if (processFile("edit-4-40-b-50Hz.ts.bin","edit450.h","edit4") < 0)
    return (-1);

  if (processFile("vsyncbin.bin","rom_a000.h","rom_a000") < 0)
    return (-1);

/*
  if (processFile("pet.bin","petfont.h","petfont") < 0)
    return (-1);
  if (processFile("ark.dmp","arksid.h","siddmp") < 0)
    return (-1);
  if (processFile("ggs.dmp","ggssid.h","siddmp") < 0)
    return (-1);
*/

  return 0;
}


