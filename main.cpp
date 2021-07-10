// 
// tool to quickly convert an elf file into a pbp for running on a v1.00 psp
// 
// loser may 2005
// 

typedef int				s32;
typedef short			s16;
typedef char			s8;
typedef unsigned int	u32;
typedef unsigned short	u16;
typedef unsigned char	u8;

#include <stdio.h>
#include <string.h>
#include "paramsfo.h"
#include "icon0png.h"
#include "icon1pmf.h"
#include "icon1png.h"
#include "pict1png.h"
#include "snd0at3.h"
#include "datapsar.h"

s32 patchSfoTitle(u8* bufferParamSfo, s32 sizeParamSfo, const char* appTitle);
s32 generatePbp(FILE* fd, u8* elfBuffer, s32 elfSize, const char* appTitle);

char title[] =	"Elf2Pbp v0.3  -  loser 2005";
char usage[] =	"usage:  Elf2Pbp <elf filename> [app title]\n"
				"        Elf2Pbp main.elf \"My First Demo\"\n"
				"note:   app title is optional,\n"
				"        include quotes around it if it contains spaces";

#define DEFAULT_APP_TITLE	"PSPDev Application"

int main(int argc, const char* argv[])
{
	printf("%s\n", title);
	if(argc < 2 || argc > 3)
	{
		printf("%s\n", usage);
		return 1;
	}
	
	// get elf and pbp filenames
	// pbp name generation could be better..
	char elfFilename[256];
	char pbpFilename[256];
	strcpy(elfFilename, argv[1]);
//	sprintf(pbpFilename, "%s.pbp", elfFilename);
	strcpy(pbpFilename, "EBOOT.PBP");
	
	// get app title (limit it to 128 chars including null terminator)
	char appTitle[256];
	if(argc == 3)
		strncpy(appTitle, argv[2], 128);
	else
		strncpy(appTitle, DEFAULT_APP_TITLE, 128);
	
	// buffer elf file
	FILE* elf_fd = fopen(elfFilename, "rb");
	if(elf_fd == NULL)
	{
		printf("error opening %s\n", elfFilename);
		return 2;
	}
	fseek(elf_fd, 0, SEEK_END);
	s32 elfSize = ftell(elf_fd);
	fseek(elf_fd, 0, SEEK_SET);
	unsigned char* elfBuffer = new unsigned char[elfSize];
	fread(elfBuffer, 1, elfSize, elf_fd);
	fclose(elf_fd);
	
	// generate pbp file
	FILE* pbp_fd = fopen(pbpFilename, "w+b");
	if(pbp_fd == NULL)
	{
		printf("error creating %s\n", pbpFilename);
		delete[] elfBuffer;
		return 2;
	}
	generatePbp(pbp_fd, elfBuffer, elfSize, appTitle);
	
	// finish up
	printf("successfully generated pbp file\n");
	fclose(pbp_fd);
	delete[] elfBuffer;
	return 0;
}

#define PBP_VERSION			0x00000100

typedef struct {
	char	magic[4];		// "\0PBP"
	s32		version;		// 0x00000100
	s32		offsetParamSfo;	// offset of param.sfo file
	s32		offsetIcon0Png;	// offset of icon0.png file
	s32		offsetIcon1Pmf;	// offset of icon1.pmf file
	s32		offsetIcon1Png;	// offset of icon1.png file
	s32		offsetPict1Png;	// offset of pict1.png file
	s32		offsetSnd0At3;	// offset of snd0.at3  file
	s32		offsetDataPsp;	// offset of data.psp file
	s32		offsetDataPsar;	// offset of data.psar file
} PbpHeader;

// generate a pbp file from a given elf file and inbuilt everything else
s32 generatePbp(FILE* fd, u8* elfBuffer, s32 elfSize, const char* appTitle)
{
	// prepare pbp header
	PbpHeader pbpHeader;
	pbpHeader.magic[0]	= '\0';
	pbpHeader.magic[1]	= 'P';
	pbpHeader.magic[2]	= 'B';
	pbpHeader.magic[3]	= 'P';
	pbpHeader.version	= PBP_VERSION;
	pbpHeader.offsetParamSfo	= sizeof(PbpHeader);
	pbpHeader.offsetIcon0Png	= pbpHeader.offsetParamSfo + sizeParamSfo;
	pbpHeader.offsetIcon1Pmf	= pbpHeader.offsetIcon0Png + sizeIcon0Png;
	pbpHeader.offsetIcon1Png	= pbpHeader.offsetIcon1Pmf + sizeIcon1Pmf;
	pbpHeader.offsetPict1Png	= pbpHeader.offsetIcon1Png + sizeIcon1Png;
	pbpHeader.offsetSnd0At3		= pbpHeader.offsetPict1Png + sizePict1Png;
	pbpHeader.offsetDataPsp		= pbpHeader.offsetSnd0At3  + sizeSnd0At3;
	pbpHeader.offsetDataPsar	= pbpHeader.offsetDataPsp  + elfSize;
	
	// patch app title into param.sfo file
	patchSfoTitle(bufferParamSfo, sizeParamSfo, appTitle);
	
	// write header to pbp
	fwrite(&pbpHeader, 1, sizeof(pbpHeader), fd);
	// write all files to pbp
	fwrite(bufferParamSfo, 1, sizeParamSfo, fd);
	fwrite(bufferIcon0Png, 1, sizeIcon0Png, fd);
	fwrite(bufferIcon1Pmf, 1, sizeIcon1Pmf, fd);
	fwrite(bufferIcon1Png, 1, sizeIcon1Png, fd);
	fwrite(bufferPict1Png, 1, sizePict1Png, fd);
	fwrite(bufferSnd0At3,  1, sizeSnd0At3,  fd);
	fwrite(elfBuffer,      1, elfSize,      fd);
	fwrite(bufferDataPsar, 1, sizeDataPsar, fd);
	
	return 0;
}

typedef struct {
	s16		offsetString;	// offset of string in string table
	u16		flags;			// flags or date or something?
	s32		size;			// size of data
	s32		sizeBuffer;		// size of data buffer
	s32		offsetData;		// offset of data in data table
} SfoItem;

typedef struct {
	char	magic[4];		// "\0PSF"
	s32		version;		// 0x00000101
	s32		offsetStrings;	// offset of string table from start of file
	s32		offsetData;		// offset of data from start of file
	s32		numItems;		// number of items in sfo
	SfoItem	items[1];		// items
} SfoHeader;

s32 patchSfoTitle(u8* bufferParamSfo, s32 sizeParamSfo, const char* appTitle)
{
	s32 i;

	// find title part of sfo file
	SfoHeader* header;
	header = (SfoHeader*)bufferParamSfo;
	for(i=0; i<header->numItems; i++)
	{
		if(strcmp("TITLE", (char*)&bufferParamSfo[header->offsetStrings + header->items[i].offsetString]) == 0)
		{
			// this item is "TITLE", so insert new title
			header->items[i].size = (s32)strlen(appTitle);
			strncpy((char*)&bufferParamSfo[header->offsetData + header->items[i].offsetData], appTitle, header->items[i].sizeBuffer);
			break;
		}
	}
	
	return 0;
}

