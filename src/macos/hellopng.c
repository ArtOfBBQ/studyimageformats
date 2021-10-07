#include "stdio.h"
#include "inttypes.h"
#include "stdlib.h"
#include "assert.h"


/*
I'm not sure how to deal with this in C,
I just want an array with the size stored
*/
typedef struct EntireFile {
    void * data;
    size_t size_left;
} EntireFile;

/*
A PNG file starts with an 8-byte signature
All of these seem like ancient artifacts to me,
except for bytes 2 to 4, which read 'PNG'
*/
#pragma pack(push, 1)
typedef struct PNGSignature {
    uint8_t highbit_hint;             // artifact
    char png_string[3];               // Reads 'PNG'
    char dos_style_line_ending[2];    // DOS artifact
    char end_of_file_char;            // DOS artifact
    char unix_style_line_ending;      // Unix/DOS artifact
} PNGSignature;

/*
A PNG Chunk header
Length           (4 bytes)
Chunk type/name  (4 bytes)

Will be followed by the data
Chunk data       (length bytes)
CRC checksum     (4 bytes)
*/
typedef struct PNGChunkHeader {
    uint32_t length;     // big endian
    char type[4];        // 1st letter upper = critical chunk
} PNGChunkHeader;


/*
IHDR must be the first chunk; it contains (in this order)
the image's width (4 bytes);
height (4 bytes);
bit depth (1 byte, values 1, 2, 4, 8, or 16);
color type (1 byte, values 0, 2, 3, 4, or 6);
compression method (1 byte, value 0);
filter method (1 byte, value 0);
and interlace method (1 byte, values 0 "no interlace" or 1 "Adam7 interlace") 
(13 data bytes total).
*/
typedef struct IHDRBody {
    uint32_t width;
    uint32_t height;
    uint8_t  bit_depth;
    uint8_t  color_type;
    uint8_t  compression_method;
    uint8_t  filter_method;
    uint8_t  interlace_method;
} IHDRBody;
#pragma pack(pop)


int are_equal_strings(char * str1, char * str2, size_t len) {
    for (size_t i = 0; i < len; i++) {
        if (str1[i] != str2[i]) {
            return 0;
        }
    }
    
    return 1;
}

/*
PNG files have many 4-byte big endian values
We need to able to flip them to little endian

Bitwise operations reminder:
Operator	Effect
&               Bitwise AND operator
|               Bitwise OR operator
^               Bitwise exclusive OR operator
~               Binary One’s Complement Operator
<<              Left shift 
>>              Right shift
*/
void flip_endian(uint32_t * to_flip) {
    uint32_t flipping = *to_flip;
    
    uint32_t byte_1 = ((flipping >> 0) & 255);
    uint32_t byte_2 = ((flipping >> 8) & 255);
    uint32_t byte_3 = ((flipping >> 16) & 255);
    uint32_t byte_4 = ((flipping >> 24) & 255);
    
    flipping =
        byte_1 << 24 |
        byte_2 << 16 |
        byte_3 << 8  |
        byte_4 << 0;
    
    *to_flip = flipping;
}

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

int main(int argc, const char * argv[]) 
{
    printf("Hello png files!\n");
    
    FILE * imgfile = fopen(
        "structuredart.png",
        "rb");
    fseek(imgfile, 0, SEEK_END);
    size_t fsize = ftell(imgfile);
    fseek(imgfile, 0, SEEK_SET);
    
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
    
    PNGSignature * png_signature = consume_struct(
        /* type: */ PNGSignature,
        /* from: */ entire_file);
    
    printf(
        "Signature's png_string (expecting 'PNG'): %c%c%c\n",
        png_signature->png_string[0],
        png_signature->png_string[1],
        png_signature->png_string[2]);
    
    if (!are_equal_strings(
        /* string 1: */ png_signature->png_string,
        /* string 2: */ "PNG",
        /* string length: */ 3))
    {
        printf("aborting - not a PNG file\n");
        return 1;
    }
    
    while (
        entire_file->size_left >= sizeof(PNGChunkHeader))
    {
        PNGChunkHeader * chunk_header = consume_struct(
            /* type: */   PNGChunkHeader,
            /* buffer: */ entire_file);
        assert(sizeof(chunk_header) == 8);
        flip_endian(&chunk_header->length);
        
        printf(
            "read %s header describing next %u bytes\n",
            chunk_header->type,
            chunk_header->length);
        
        if (are_equal_strings(
            chunk_header->type,
            "IHDR",
            4))
        {
            assert(chunk_header->length == 13);
            
            IHDRBody * ihdr_body = consume_struct(
                /* type: */ IHDRBody,
                /* entire_file: */ entire_file);
            
            flip_endian(&ihdr_body->width);
            flip_endian(&ihdr_body->height);
            printf(
                "ihdr_body->width: %u\n",
                ihdr_body->width);
            printf(
                "ihdr_body->height: %u\n",
                ihdr_body->height);
        }
        else if (are_equal_strings(
            chunk_header->type,
            "PLTE",
            4))
        {
            // handle palette header
            printf("found PLTE palette header...");
            break;
        }
        else if (are_equal_strings(
            chunk_header->type,
            "IDAT",
            4))
        {
            // handle image data header
            printf("found IDAT image header...");
            printf("pixel data must be extracted here!!");
            break;
        }
        else if (are_equal_strings(
            chunk_header->type,
            "IEND",
            4))
        {
            // handle image end header
            printf("found IEND image end header...");
            break;
        }
        else if ((char)chunk_header->type[0] > 'Z')
        {
            uint32_t skip = chunk_header->length;
            printf(
                "skip unhandled lowercase (noncritical) chunk\n");
            
            entire_file->data += skip;
            
            assert(skip <= entire_file->size_left); 
            entire_file->size_left -= skip;
        } else {
            printf(
                "ERROR: unhandled critical chunk header: %s\n",
                chunk_header->type);
            return 1;
        }
        
        // ignore a 4-byte CRC checksum
        assert(entire_file->size_left >= 4);
        entire_file->data += 4;
        entire_file->size_left -= 4;
    }
    
    return 0;
}

