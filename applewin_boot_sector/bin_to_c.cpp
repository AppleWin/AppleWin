#include <stdio.h>
#include <stdint.h>

uint8_t BootSector[256];

int main(int nArgs, char *aArg[])
{
	if (nArgs > 1)
	{
		const char *pFileName = aArg[1];
		FILE *pFile = fopen( pFileName, "rb" );
		if (pFile)
		{
			fread( BootSector, 1, 256, pFile );
			fclose( pFile );

			printf( "uint8_t aAppleWinBootSector[256] =\n{\n    " );
			for( int iOffset = 0; iOffset < 256; iOffset++ )
			{
				printf( "0x%02X", BootSector[ iOffset ] );
				if ((iOffset & 0xF) != 0xF)
					printf( "," );
				else
					if (iOffset != 0xFF) printf( ",\n    " );
					else                 printf(  "\n};" );
			}
		}
		else
		{
			printf( "ERROR: Couldn't open file '%s'\n", pFileName );
		}
	}

	return 0;
}
