#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int maxGamma = 7, reservedBytes = 2;
static int escBits = 2, escMask = 0xc0;
static int extraLZPosBits = 0, rleUsed = 15;


#define LRANGE    (((2<<maxGamma)-3)*256) /* 0..125, 126 -> 1..127 */
#define MAXLZLEN  (2<<maxGamma)
#define MAXRLELEN (((2<<maxGamma)-2)*256) /* 0..126 -> 1..127 */
#define DEFAULT_LZLEN LRANGE

static int lrange, maxlzlen, maxrlelen;


static int bitMask = 0x80;


static const unsigned char *up_Data;
static int up_Mask, up_Byte;
void up_SetInput(const unsigned char *data) 
{
    up_Data = data;
    up_Mask = 0x80;
    up_Byte = 0;
}

int up_GetBits(int bits) 
{
    int val = 0;

    while (bits--) {
  		val <<= 1;
  		if ((*up_Data & up_Mask))
     		val |= 1;
  		up_Mask >>= 1;
  		if (!up_Mask) 
  		{
      	up_Mask = 0x80;
      	up_Data++;
      	up_Byte++;
  		}
    }
    return val;
}

int up_GetValue(void) 
{
    int i = 0;

    while (i<maxGamma) {
  		if (!up_GetBits(1))
      		break;
  		i++;
    }
    return (1<<i) | up_GetBits(i);
}

int UnPack(int loadAddr, unsigned char *data, unsigned char * outBuffer, int outbufsize) {
	long size, startEsc, endAddr, execAddr, headerSize;
	long startAddr, error = 0;
	int outPointer = 0;

	int i, overlap;
	unsigned char * byteCodeVec;

    /* Search for the right code */
    if (data[0] == 'p' && data[1] == 'u') 
    {
		/* was saved without decompressor */
		int cnt = 2;

		endAddr = (data[cnt] | (data[cnt+1]<<8)) + 0x100;
		cnt += 2;

		startEsc = data[cnt++];
		startAddr = data[cnt] | (data[cnt+1] << 8);
		cnt += 2;

		escBits = data[cnt++];
		if (escBits < 0 || escBits > 8) 
		{
			fprintf(stderr, "Error: Broken archive, escBits %d.\n", escBits);
      		return 20;
  		}
		maxGamma = data[cnt++] - 1;
		if (data[cnt++] != (1<<maxGamma) || maxGamma < 5 || maxGamma > 7) 
		{
      		fprintf(stderr, "Error: Broken archive, maxGamma %d.\n", maxGamma);
      		return 20;
  		}
    	lrange = LRANGE;
    	maxlzlen = MAXLZLEN;
    	maxrlelen = MAXRLELEN;

		extraLZPosBits = data[cnt++];
  		if (extraLZPosBits < 0 || extraLZPosBits > 4) 
  		{
      		fprintf(stderr, "Error: Broken archive, extraLZPosBits %d.\n", extraLZPosBits);
      		return 20;
  		}

  		execAddr = data[cnt] | (data[cnt+1]<<8);
  		cnt += 2;

  		rleUsed = data[cnt++];
  		byteCodeVec = &data[cnt - 1];

  		overlap = 0;

  		size = endAddr-startAddr;
  		headerSize = cnt + rleUsed;

  		endAddr = loadAddr + size;
	}

/*
    {
  		fprintf(stderr,
			"Load 0x%04x, Start 0x%04lx, exec 0x%04lx\n",
    		loadAddr, startAddr, execAddr);
  		fprintf(stderr, "Escape bits %d, starting escape 0x%02lx\n",
    		escBits, (startEsc<<(8-escBits)));
  		fprintf(stderr,
    		"Decompressor size %ld, max length %d, LZPOS LO bits %d\n",
    		headerSize+2, (2<<maxGamma), extraLZPosBits+8);
  		fprintf(stderr, "rleUsed: %d\n", rleUsed);
    }
    if (rleUsed > 15) {
  		fprintf(stderr, "Error: Old archive, rleUsed %d > 15.\n", rleUsed);
  		return 20;
    }
*/
	
    outPointer = 0;
    up_SetInput(data + headerSize);
    while (!error) {
  		int sel = startEsc;
/*
  		if (startAddr + outPointer >= up_Byte + endAddr - size) 
  		{
      		if (!error)
    			fprintf(stderr, "Error: Target %5ld exceeds source %5ld..\n",
      			startAddr + outPointer, up_Byte + endAddr - size);
      		error++;
  		}
  		if (up_Byte > size + overlap) 
  		{
      		fprintf(stderr, "Error: No EOF symbol found (%d > %d).\n",
        		up_Byte, size + overlap);
      		error++;
  		}
*/
  		if (escBits)
      		sel = up_GetBits(escBits);
  		if (sel == startEsc) 
  		{
      		int lzPos, lzLen = up_GetValue(), i;
      		if (lzLen != 1) 
      		{
    			int lzPosHi = up_GetValue()-1, lzPosLo;
    			if (lzPosHi == (2<<maxGamma)-2) 
    			{
        			break; /* EOF */
    			} else 
    			{
        			if (extraLZPosBits) 
        			{
      					lzPosHi = (lzPosHi<<extraLZPosBits) |
            			up_GetBits(extraLZPosBits);
        			}
        			lzPosLo = up_GetBits(8) ^ 0xff;
        			lzPos = (lzPosHi<<8) | lzPosLo;
    			}
      		} 
      		else 
      		{
    			if (up_GetBits(1)) 
    			{
        			int rleLen, byteCode, byte;

        			if (!up_GetBits(1)) 
        			{
      					int newEsc = up_GetBits(escBits);

      					outBuffer[outPointer++] = (startEsc<<(8-escBits)) | up_GetBits(8-escBits);
/*fprintf(stdout, "%5ld %5ld  *%02x\n",
  outPointer-1, up_Byte, outBuffer[outPointer-1]);*/
      					startEsc = newEsc;
      					if (outPointer >= outbufsize) 
      					{
          					fprintf(stderr, "Error: Broken archive, "
           						"output buffer overrun at %d.\n",
            					outPointer);
          					return 20;
      					}
      					continue;
        			}
        			rleLen = up_GetValue();
        			if (rleLen >= (1<<maxGamma)) 
        			{
      					rleLen = ((rleLen-(1<<maxGamma))<<(8-maxGamma)) | up_GetBits(8-maxGamma);
      					rleLen |= ((up_GetValue()-1)<<8);
        			}
        			byteCode = up_GetValue();
        			if (byteCode < 16/*32*/) 
        			{
      					byte = byteCodeVec[byteCode];
        			} else 
        			{
      					byte = ((byteCode-16/*32*/)<<4/*3*/) | up_GetBits(4/*3*/);
        			}

/*fprintf(stdout, "%5ld %5ld RLE %5d 0x%02x\n", outPointer, up_Byte, rleLen+1,
  byte);*/
        			if (outPointer + rleLen + 1 >= outbufsize) 
        			{
      					fprintf(stderr, "Error: Broken archive, "
        					"output buffer overrun at %d.\n",
        					outbufsize);
      					return 20;
        			}
        			for (i=0; i<=rleLen; i++) 
        			{
      					outBuffer[outPointer++] = byte;
        			}
        			continue;
    			}
    			lzPos = up_GetBits(8) ^ 0xff;
      		}
/*fprintf(stdout, "%5ld %5ld LZ %3d 0x%04x\n",
  outPointer, up_Byte, lzLen+1, lzPos+1);*/

     		 /* outPointer increases in the loop, thus its minimum is here */
      		if (outPointer - lzPos -1 < 0) 
      		{
    			fprintf(stderr, "Error: Broken archive, "
      				"LZ copy position underrun at %d (%d). "
      				"lzLen %d.\n",
      				outPointer, lzPos+1, lzLen+1);
    			return 20;
      		}
      		if (outPointer + lzLen + 1 >= outbufsize) 
      		{
    			fprintf(stderr, "Error: Broken archive, "
      				"output buffer overrun at %d.\n",
      				outbufsize);
    			return 20;
      		}
      		for (i=0; i<=lzLen; i++) 
      		{
    			outBuffer[outPointer] = outBuffer[outPointer - lzPos - 1];
    			outPointer++;
      		}
  		} 
  		else 
  		{
      		int byte = (sel<<(8-escBits)) | up_GetBits(8-escBits);
/*fprintf(stdout, "%5ld %5ld  %02x\n",
  outPointer, up_Byte, byte);*/
      		outBuffer[outPointer++] = byte;
      		if (outPointer >= outbufsize) 
      		{
    			fprintf(stderr, "Error: Broken archive, "
      				"output buffer overrun at %d.\n",
      				outPointer);
    			return 20;
      		}
  		}
    }
    if (error) {
  		fprintf(stderr, "Error: Target exceeded source %5ld times.\n",
    		error);
    }

    return outPointer;
}




