#include <stdio.h>

#include "shared.h"
#include "pcecrc.h"

/* Name of the loaded file */
char game_name[0x100];

/* split : 1= Split image (only needed for 512k versions of 384k images)
   flip  : 1= Bit-flip image (only for some TurboGrafx-16 images) */
int load_rom(char *filename, int split, int flip)
{
    #include "bitflip.h"
    uint8 header[0x200];
    uint8 *ptr = NULL, *buf = NULL;
    uint32 crc;
    int size;
    uint32 n;

    /* Default */
    strcpy(game_name, filename);

    if(check_zip(filename))
    {
        unzFile *fd = NULL;
        unz_file_info info;
        int ret = 0;

        /* Attempt to open the archive */
        fd = unzOpen(filename);
        if(!fd) return (-1);

        /* Go to first file in archive */
        ret = unzGoToFirstFile(fd);
        if(ret != UNZ_OK) {
            unzClose(fd);
            return (-2);
        }

        /* Get information on the file */
        ret = unzGetCurrentFileInfo(fd, &info, game_name, 0x100, NULL, 0, NULL, 0);
        if(ret != UNZ_OK) {
            unzClose(fd);
            return (-3);
        }

        /* Open the file for reading */
        ret = unzOpenCurrentFile(fd);
        if(ret != UNZ_OK) {
            unzClose(fd);
            return (-4);
        }

        /* Allocate file data buffer */
        size = info.uncompressed_size;
        buf = malloc(size);
        if(!buf) {
            unzClose(fd);
            return (-5);
        }

        /* Read (decompress) the file */
        ret = unzReadCurrentFile(fd, buf, info.uncompressed_size);
        if(ret != (int)info.uncompressed_size)
        {
            free(buf);
            unzCloseCurrentFile(fd);
            unzClose(fd);
            return (-6);
        }

        /* Close the current file */
        ret = unzCloseCurrentFile(fd);
        if(ret != UNZ_OK) {
            free(buf);
            unzClose(fd);
            return (-7);
        }

        /* Close the archive */
        ret = unzClose(fd);
        if(ret != UNZ_OK) {
            free(buf);
            return (-8);
        }
    }
    else
    {
        gzFile gd = NULL;

        /* Open file */
        gd = gzopen(filename, "rb");
        if(!gd) return (-9);

        /* Get file size */
        size = gzsize(gd);

        /* Allocate file data buffer */
        buf = malloc(size);
        if(!buf) {
            gzclose(gd);
            return (-10);
        }

        /* Read file data */
        gzread(gd, buf, size);

        /* Close file */
        gzclose(gd);
    }

    /* Check for 512-byte header */
    ptr = buf;
    if((size / 512) & 1)
    {
        memcpy(header, buf, 0x200);
        size -= 0x200;
        buf += 0x200;
    }

    /* Generate CRC and print information */
    crc = crc32(0, buf, size);

    /* Look up game CRC in the CRC database, and set up flip and
       split options accordingly */
    for(n = 0; n < (sizeof(pcecrc_list) / sizeof(t_pcecrc)); n++)
    {
        if(crc == pcecrc_list[n].crc)
        {
            if(pcecrc_list[n].flag & FLAG_BITFLIP) flip = 1;
            if(pcecrc_list[n].flag & FLAG_SPLIT) split = 1;
        }
    }

    /* Bit-flip image */
    if(flip)
    {
        uint8 temp;
        int count;

        for(count = 0; count < size; count += 1)
        {
            temp = buf[count];
            buf[count] = bitflip[temp];
        }
    }

    /* Always split 384K images */
    if(size == 0x60000)
    {
        memcpy(rom + 0x00000, buf + 0x00000, 0x40000);
        memcpy(rom + 0x80000, buf + 0x40000, 0x20000);
    }
    else /* Split 512K images if requested */
    if(split && (size == 0x80000))
    {
        memcpy(rom + 0x00000, buf + 0x00000, 0x40000);
        memcpy(rom + 0x80000, buf + 0x40000, 0x40000);
    }
    else
    {
        memcpy(rom, buf, (size > 0x100000) ? 0x100000 : size);
    }

    /* Free allocated memory and exit */
    free(ptr);

#ifdef DOS
    /* I need Allegro to handle this... */
    strcpy(game_name, get_filename(game_name));
#endif

    return (1);
}


int file_exist(char *filename)
{
    FILE *fd = fopen(filename, "rb");
    if(!fd) return (0);
    fclose(fd);
    return (1);
}


int load_file(char *filename, uint8 *buf, int size)
{
    FILE *fd = fopen(filename, "rb");
    if(!fd) return (0);
    fread(buf, size, 1, fd);
    fclose(fd);
    return (1);
}


int save_file(char *filename, uint8 *buf, int size)
{
    FILE *fd = NULL;
    if(!(fd = fopen(filename, "wb"))) return (0);
    fwrite(buf, size, 1, fd);
    fclose(fd);
    return (1);
}


int check_zip(char *filename)
{
#ifdef DOS
    if(stricmp(strrchr(filename, '.'), ".zip") == 0) return (1);
#else
    uint8 buf[2];
    FILE *fd = NULL;
    fd = fopen(filename, "rb");
    if(!fd) return (0);
    fread(buf, 2, 1, fd);
    fclose(fd);
    /* Look for hidden ZIP magic */
    if(memcmp(buf, "PK", 2) == 0) return (1);
#endif
    return (0);
}

/* Because gzio.c doesn't work */
int gzsize(gzFile gd)
{
    #define CHUNKSIZE   0x10000
    int size = 0, length = 0;
    unsigned char buffer[CHUNKSIZE];
    gzrewind(gd);
    do {
        size = gzread(gd, buffer, CHUNKSIZE);
        if(size <= 0) break;
        length += size;
    } while (!gzeof(gd));
    gzrewind(gd);
    return (length);
    #undef CHUNKSIZE
}

