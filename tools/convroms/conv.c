

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

/*
  if (processFile("pet_e000_roms/edit-4-80-b-60Hz.901474-03.bin","edit480.h","edit480") < 0)
    return (-1);

  if (processFile("pet_e000_roms/edit-4-40-b-60Hz.ts.bin","edit4.h","edit4") < 0)
    return (-1);

  if (processFile("pet_e000_roms/edit-4-80-b-50Hz.901474-04-0283.bin","edit48050.h","edit480") < 0)
    return (-1);

  if (processFile("pet_e000_roms/edit-4-40-b-50Hz.ts.bin","edit450.h","edit4") < 0)
    return (-1);

  if (processFile("pet_a000_roms/vsync.bin","vsync.h","vsyncpet") < 0)
    return (-1);


  if (processFile("pet_a000_roms/jinsam8-rom-a000_database.bin","jinsam8-rom-a000_database.h","jinsam8") < 0)
    return (-1);
  if (processFile("pet_a000_roms/kram2.0-rom-a000.bin","kram2.0-rom-a000.h","kram20") < 0)
    return (-1);
  if (processFile("pet_a000_roms/micromon80-ud11-a000.bin","micromon80-ud11-a000.h","micromon80") < 0)
    return (-1);
  if (processFile("pet_a000_roms/orga_basic101-a000.bin","orga_basic101-a000.h","orga_basic101") < 0)
    return (-1);
  if (processFile("pet_a000_roms/pal_assembler_a000.bin","pal_assembler_a000.h","pal_assembler") < 0)
    return (-1);
  if (processFile("pet_a000_roms/pascal3.0_rom-a000.bin","pascal3.0_rom-a000.h","pascal30") < 0)
    return (-1);
  if (processFile("pet_a000_roms/power_basic_8032_a000.bin","power_basic_8032_a000.h","power_basic_8032") < 0)
    return (-1);
  if (processFile("pet_a000_roms/toolkit4.0_rom-a000.bin","toolkit4.0_rom-a000.h","toolkit40") < 0)
    return (-1);
  if (processFile("pet_a000_roms/wedge.bin","wedge.h","wedge") < 0)
    return (-1);
  if (processFile("pet_a000_roms/wordpro3-rom-a000.bin","wordpro3-rom-a000.h","wordpro3") < 0)
    return (-1);
*/
  if (processFile("menuloader.bin","menuloader-9000.h","menuloader") < 0)
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


