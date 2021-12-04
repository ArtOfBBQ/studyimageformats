#include "stdio.h"
#include "inttypes.h"
#include "stdlib.h"
#include "assert.h"
#include "deflate.h"

#define MAX_ORIGINAL_FILENAME_SIZE 50
#define MAX_COMMENT_SIZE 200

#pragma pack(push, 1)

/*
Each member has the following structure:

+---+---+---+---+---+---+---+---+---+---+
|ID1|ID2|CM |FLG|     MTIME     |XFL|OS | (more-->)
+---+---+---+---+---+---+---+---+---+---+
*/
typedef struct GZHeader {
    uint8_t id1; // must be 31 to be gzip
    uint8_t id2; // must be 139 to be gzip
    uint8_t CM; // CM = 8 denotes the "deflate" method
    uint8_t FLG;
    uint32_t MTIME;
    uint8_t XFLG;
    uint8_t OS;
} GZHeader;

/*
(if FLG.FEXTRA set)

+---+---+=================================+
| XLEN  |...XLEN bytes of "extra field"...| (more-->)
+---+---+=================================+


(if FLG.FCOMMENT set)

+===================================+
|...file comment, zero-terminated...| (more-->)
+===================================+

(if FLG.FHCRC set)

+---+---+
| CRC16 |
+---+---+

+=======================+
|...compressed blocks...| (more-->)
+=======================+

0   1   2   3   4   5   6   7
+---+---+---+---+---+---+---+---+
|     CRC32     |     ISIZE     |
+---+---+---+---+---+---+---+---+
*/

#pragma pack(pop)


void copy_memory(
    void * from,
    void * to,
    size_t size)
{
    uint8_t * fromu8 = (uint8_t *)from;
    uint8_t * tou8 = (uint8_t *)to;
    
    while (size > 0) {
        *tou8 = *fromu8;
        fromu8++;
        tou8++;
        size--;
    }
}

// Grab data from the front of a buffer & advance pointer
#define consume_struct(type, from) (type *)consume_chunk(from, sizeof(type))
uint8_t * consume_chunk(
    EntireFile * from,
    size_t size_to_consume)
{
    assert(from->bits_left == 0);
    assert(from->size_left >= size_to_consume);
    
    uint8_t * return_value = malloc(size_to_consume);
    
    copy_memory(
        /* from: */   from->data,
        /* to: */     return_value,
        /* size: */   size_to_consume);
    
    from->data += size_to_consume;
    from->size_left -= size_to_consume;
    
    return return_value;
}

char * consume_till_zero_terminate(
    EntireFile * from,
    size_t max_size)
{
    char * filename = malloc(
        MAX_ORIGINAL_FILENAME_SIZE * sizeof(char));
    int filename_size = 0;
    
    while (
        ((uint8_t *)from->data)[0] != 0
        && from->size_left > 0
        && filename_size < max_size)
    {
        filename[filename_size] =
            ((char *)from->data)[0];
        from->size_left -= 1;
        from->data += 1;
        filename_size += 1;
    }

    from->data += 1;
    
    return filename;
}

int main(int argc, const char * argv[]) 
{
    printf("Hello .gz files!\n");
    
    if (argc != 2) {
        printf("Please supply 1 argument (png file name)\n");
        printf("Got:");
        for (int i = 0; i < argc; i++) {
            printf(" %s", argv[i]);
        }
        return 1;
    }
    
    printf("Inspecting file: %s\n", argv[1]);
    
    FILE * imgfile = fopen(
        argv[1],
        "rb");
    fseek(imgfile, 0, SEEK_END);
    size_t fsize = ftell(imgfile);
    fseek(imgfile, 0, SEEK_SET);


    printf("creating buffer..\n");
    
    uint8_t * buffer = malloc(fsize);
    EntireFile * entire_file = malloc(sizeof(EntireFile));
    
    size_t bytes_read = fread(
        /* ptr: */
            buffer,
        /* size of each element to be read: */
            1, 
        /* nmemb (no of members) to read: */
            fsize,
        /* stream: */
            imgfile);
    printf("bytes read: %zu\n", bytes_read);
    fclose(imgfile);
    assert(bytes_read == fsize);
    
    entire_file->data = buffer;
    entire_file->size_left = bytes_read;
    
    // The first chunk must be IHDR
    assert(entire_file->size_left >= sizeof(GZHeader));
    GZHeader * gzip_header = consume_struct(
        /* type  : */ GZHeader,
        /* buffer: */ entire_file);

    printf("checking if valid gzip file..\n");
    assert(gzip_header->id1 == 31);
    assert(gzip_header->id2 == 139);

    if (gzip_header->CM != 8) {
        printf("unsupported gzip - no DEFLATE compression~\n");
        return 1;
    }

    printf("OK - passed file has gzip identifications\n");

    /*
    FLG (FLaGs)
       This flag byte is divided into bits as follows:
       bit 0   FTEXT
       bit 1   FHCRC
       bit 2   FEXTRA
       bit 3   FNAME
       bit 4   FCOMMENT
       bit 5   reserved
       bit 6   reserved
       bit 7   reserved
    */
    printf(
        "gzip_header->FLG[FTEXT]: %u\n",
        gzip_header->FLG >> 0 & 1);
    printf(
        "gzip_header->FLG[FHCRC]: %u\n",
        gzip_header->FLG >> 1 & 1);
    printf(
        "gzip_header->FLG[FEXTRA]: %u\n",
        gzip_header->FLG >> 2 & 1);
    printf(
        "gzip_header->FLG[FNAME]: %u\n",
        gzip_header->FLG >> 3 & 1);
    printf(
        "gzip_header->FLG[FCOMMENT]: %u\n",
        gzip_header->FLG >> 4 & 1);
    
    /*    
    (if FLG.FNAME set)
    
    +=========================================+
    |...original file name, zero-terminated...| (more-->)
    +=========================================+
    */
    if (gzip_header->FLG >> 3 & 1) {

        char * filename = consume_till_zero_terminate(
            /* from: */ entire_file,
            /* max_size: */ MAX_ORIGINAL_FILENAME_SIZE);
        
        printf("original filename was included: %s\n", filename);
    }

    /*
    if FLG.FCOMMENT set)
    
    +===================================+
    |...file comment, zero-terminated...| (more-->)
    +===================================+ 
    */
    if (gzip_header->FLG >> 4 & 1) {
        char * comment = consume_till_zero_terminate(
            /* from: */ entire_file,
            /* max_size: */ MAX_COMMENT_SIZE);
        
        printf("a comment was included: %s\n", comment);
    }

    printf("compressed blocks should start here...\n");
    printf("file size left: %zu\n", entire_file->size_left);
    printf(
        "8 bytes at EOF rsrved, so DEFLATE size is: %zu\n",
        entire_file->size_left - 8);
    
    uint8_t * recipient = malloc(entire_file->size_left - 8);
    deflate(
        /* recipient: */ recipient,
        /* entire_file: */ entire_file,
        /* expected_size_bytes: */ entire_file->size_left - 8);

    printf("end of gz file...\n");
    
    return 0;
}
