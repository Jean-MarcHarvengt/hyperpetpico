#include <stdio.h>
#include <stdlib.h>
#include <string.h>

//#define DEBUG 1
#ifdef DEBUG
#endif

static uint8_t maxGamma = 7;
static uint8_t escBits = 2;
static uint8_t extraLZPosBits = 0, rleUsed = 15;
static uint8_t table[16];

static const unsigned char *up_Data;
static uint8_t up_Mask, up_Byte;

static void up_SetInput(const unsigned char *data) 
{
	up_Data = data;
	up_Mask = 0x80;
	up_Byte = 0;
}

static uint8_t up_GetBits(int bits) 
{
	uint8_t val = 0;

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

static uint8_t up_GetValue(void) 
{
	uint8_t i = 0;

	while (i<maxGamma) {
		if (!up_GetBits(1))
				break;
		i++;
	}
	return (1<<i) | up_GetBits(i);
}

int UnPack(int loadAddr, unsigned char *data, unsigned char * outBuffer, int outbufsize) 
{
	int outPointer = 0;

	/* Signature */
	if (data[0] == 'p' && data[1] == 'u') 
	{
		uint8_t startEsc;
		long error = 0;
		int cnt = 4; // skip endAddr
		startEsc = data[cnt++];
		cnt += 2;    // skip startAddr
		escBits = data[cnt++];
		if (escBits < 0 || escBits > 8) 
		{
#ifdef DEBUG
			fprintf(stderr, "Error: Broken archive, escBits %d.\n", escBits);
#endif
			return 0;
		}
		maxGamma = data[cnt++] - 1;
		if (data[cnt++] != (1<<maxGamma) || maxGamma < 5 || maxGamma > 7) 
		{
#ifdef DEBUG
			fprintf(stderr, "Error: Broken archive, maxGamma %d.\n", maxGamma);
#endif
			return 0;
		}
		extraLZPosBits = data[cnt++];
		if (extraLZPosBits < 0 || extraLZPosBits > 4) 
		{
#ifdef DEBUG
			fprintf(stderr, "Error: Broken archive, extraLZPosBits %d.\n", extraLZPosBits);
#endif
			return 0;
		}
		cnt += 2; // skip execAddr
		rleUsed = data[cnt++];
		int j=0;
		int loop = rleUsed+1;
		cnt--;
		while (loop>0) {
		  table[j++] = data[cnt++];
		  loop--;
		}

		outPointer = 0;
		up_SetInput(&data[cnt]);
		while (!error) 
		{
			uint8_t sel = startEsc;
			if (escBits) {
					sel = up_GetBits(escBits);
			}
			if (sel == startEsc) 
			{
				int lzPos, lzLen = up_GetValue(), i;
				if (lzLen != 1) 
				{
					int lzPosHi = up_GetValue()-1, lzPosLo;
					if (lzPosHi == (2<<maxGamma)-2) 
					{
							break; /* EOF */
					}
					else 
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
						int rleLen;
						uint8_t byteCode, byte;
						if (!up_GetBits(1)) 
						{
							uint8_t newEsc = up_GetBits(escBits);
							outBuffer[outPointer++] = (startEsc<<(8-escBits)) | up_GetBits(8-escBits);
							startEsc = newEsc;
							if (outPointer > outbufsize) 
							{
#ifdef DEBUG
								fprintf(stderr, "Error: Broken archive, output buffer overrun at %d.\n", outPointer);
#endif
								return 0;
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
							byte = table[byteCode];
						} 
						else 
						{
							byte = ((byteCode-16/*32*/)<<4/*3*/) | up_GetBits(4/*3*/);
						}
						if (outPointer + rleLen + 1 > outbufsize) 
						{
#ifdef DEBUG
							fprintf(stderr, "Error: Broken archive, output buffer overrun at %d.\n", outbufsize);
#endif
							return 0;
						}
						for (i=0; i<=rleLen; i++) 
						{
							outBuffer[outPointer++] = byte;
						}
						continue;
					}
					lzPos = up_GetBits(8) ^ 0xff;
				}

				/* outPointer increases in the loop, thus its minimum is here */
				if (outPointer - lzPos -1 < 0) 
				{
#ifdef DEBUG
					fprintf(stderr, "Error: Broken archive, LZ copy position underrun at %d (%d). lzLen %d.\n", outPointer, lzPos+1, lzLen+1);
#endif
					return 0;
				}

				if (outPointer + lzLen + 1 > outbufsize) 
				{
#ifdef DEBUG
					fprintf(stderr, "Error: Broken archive, output buffer overrun at %d.\n", outbufsize);
#endif
					return 0;
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
				outBuffer[outPointer++] = byte;
				if (outPointer >= outbufsize) 
				{
#ifdef DEBUG
					fprintf(stderr, "Error: Broken archive, output buffer overrun at %d.\n", outPointer);
#endif
					return 0;
				}
			}
	  }
	}

	return outPointer;
}




