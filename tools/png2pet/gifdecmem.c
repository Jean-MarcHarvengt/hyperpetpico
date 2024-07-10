#include "gifdecmem.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>


#define MIN(A, B) ((A) < (B) ? (A) : (B))
#define MAX(A, B) ((A) > (B) ? (A) : (B))

typedef struct Entry {
    uint16_t length;
    uint16_t prefix;
    uint8_t  suffix;
} Entry;

typedef struct Table {
    int bulk;
    int nentries;
    Entry *entries;
} Table;

static void
read_n(gd_GIF *gif, uint8_t * buf, int n)
{
//    read(gif->fd, buf, n);
    while (n>0) {
        n--;
        *buf++ = gif->input[gif->fd++]; 
    }    
}

static int
seek_n(gd_GIF *gif, int n, int dir)
{
//    int retval = lseek(gif->fd, n, SEEK_CUR);
//    return retval;
    gif->fd += n;
    return gif->fd;
}

static int
seek_set_n(gd_GIF *gif, int n, int dir)
{
//    int retval = lseek(gif->fd, n, SEEK_SET);
//    return retval;
    gif->fd = n;
    return gif->fd;
}

static uint16_t
read_num(gd_GIF *gif)
{
    uint8_t bytes[2];

    read_n(gif, bytes, 2);
    return bytes[0] + (((uint16_t) bytes[1]) << 8);
}


static void
discard_sub_blocks(gd_GIF *gif)
{
    uint8_t size;

    do {
        read_n(gif, &size, 1);
        seek_n(gif, size, SEEK_CUR);
    } while (size);
}

static Table *
new_table(int key_size)
{
    int key;
    int init_bulk = MAX(1 << (key_size + 1), 0x100);
    Table *table = malloc(sizeof(*table) + sizeof(Entry) * init_bulk);
    printf("malloc size = %d\n", sizeof(*table) + sizeof(Entry) * init_bulk);    
    if (table) {
        table->bulk = init_bulk;
        table->nentries = (1 << key_size) + 2;
        table->entries = (Entry *) &table[1];
        for (key = 0; key < (1 << key_size); key++)
            table->entries[key] = (Entry) {1, 0xFFF, key};
    }
    return table;
}

/* Add table entry. Return value:
 *  0 on success
 *  +1 if key size must be incremented after this addition
 *  -1 if could not realloc table */
static int
add_entry(Table **tablep, uint16_t length, uint16_t prefix, uint8_t suffix)
{
    Table *table = *tablep;
    if (table->nentries == table->bulk) {
        table->bulk *= 2;
        table = realloc(table, sizeof(*table) + sizeof(Entry) * table->bulk);
        printf("realloc size = %d\n", sizeof(*table) + sizeof(Entry) * table->bulk);    
        if (!table) return -1;
        table->entries = (Entry *) &table[1];
        *tablep = table;
    }
    table->entries[table->nentries] = (Entry) {length, prefix, suffix};
    table->nentries++;
    if ((table->nentries & (table->nentries - 1)) == 0)
        return 1;
    return 0;
}

static uint16_t
get_key(gd_GIF *gif, int key_size, uint8_t *sub_len, uint8_t *shift, uint8_t *byte)
{
    int bits_read;
    int rpad;
    int frag_size;
    uint16_t key;

    key = 0;
    for (bits_read = 0; bits_read < key_size; bits_read += frag_size) {
        rpad = (*shift + bits_read) % 8;
        if (rpad == 0) {
            /* Update byte. */
            if (*sub_len == 0) {
                read_n(gif, sub_len, 1); /* Must be nonzero! */
                if (*sub_len == 0)
                    return 0x1000;
            }
            read_n(gif, byte, 1);
            (*sub_len)--;
        }
        frag_size = MIN(key_size - bits_read, 8 - rpad);
        key |= ((uint16_t) ((*byte) >> rpad)) << bits_read;
    }
    /* Clear extra bits to the left. */
    key &= (1 << key_size) - 1;
    *shift = (*shift + key_size) % 8;
    return key;
}



/* Decompress image pixels.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image_data(gd_GIF *gif)
{
    uint8_t sub_len, shift, byte;
    int init_key_size, key_size, table_is_full;
    int frm_off, frm_size, str_len, i, p, x, y;
    uint16_t key, clear, stop;
    int ret;
    Table *table;
    Entry entry;
    off_t start, end;

    read_n(gif, &byte, 1);
    key_size = (int) byte;
    if (key_size < 2 || key_size > 8)
        return -1;
    
    start = seek_n(gif, 0, SEEK_CUR);
    discard_sub_blocks(gif);
    end = seek_n(gif, 0, SEEK_CUR);
    seek_set_n(gif, start, SEEK_SET);
    clear = 1 << key_size;
    stop = clear + 1;
    table = new_table(key_size);
    key_size++;
    init_key_size = key_size;
    sub_len = shift = 0;
    key = get_key(gif, key_size, &sub_len, &shift, &byte); /* clear code */
    frm_off = 0;
    ret = 0;
    frm_size = gif->fw*gif->fh;
    while (frm_off < frm_size) {
        if (key == clear) {
            key_size = init_key_size;
            table->nentries = (1 << (key_size - 1)) + 2;
            table_is_full = 0;
        } else if (!table_is_full) {
            ret = add_entry(&table, str_len + 1, key, entry.suffix);
            if (ret == -1) {
                free(table);
                return -1;
            }
            if (table->nentries == 0x1000) {
                ret = 0;
                table_is_full = 1;
            }
        }
        key = get_key(gif, key_size, &sub_len, &shift, &byte);
        if (key == clear) continue;
        if (key == stop || key == 0x1000) break;
        if (ret == 1) key_size++;
        entry = table->entries[key];
        str_len = entry.length;
        for (i = 0; i < str_len; i++) {
            p = frm_off + entry.length - 1;
            x = p % gif->fw;
            y = p / gif->fw;         
            gif->frame[(gif->fy + y) * gif->width + gif->fx + x] = entry.suffix;
            if (entry.prefix == 0xFFF)
                break;
            else
                entry = table->entries[entry.prefix];
        }
        frm_off += str_len;
        if (key < table->nentries - 1 && !table_is_full)
            table->entries[table->nentries - 1].suffix = entry.suffix;
    }
    free(table);
    if (key == stop)
        read_n(gif, &sub_len, 1); /* Must be zero! */
    seek_n(gif, end, SEEK_SET);
    return 0;
}

/* Read image.
 * Return 0 on success or -1 on out-of-memory (w.r.t. LZW code table). */
static int
read_image(gd_GIF *gif)
{
    uint8_t fisrz;

    /* Image Descriptor. */
    gif->fx = read_num(gif);
    gif->fy = read_num(gif);
    
    if (gif->fx >= gif->width || gif->fy >= gif->height)
        return -1;
    
    gif->fw = read_num(gif);
    gif->fh = read_num(gif);
    
    gif->fw = MIN(gif->fw, gif->width - gif->fx);
    gif->fh = MIN(gif->fh, gif->height - gif->fy);
    
    read_n(gif, &fisrz, 1);

    /* Image Data. */
    return read_image_data(gif);
}

int decode_gif(gd_GIF * gif, unsigned char * input, unsigned char * output)
{
    uint8_t sigver[3];
    uint16_t width, height, depth;
    uint8_t fdsz, bgidx, aspect;
    int gct_sz;
    char sep;

    gif->fd = 0;
    gif->input = input;

    /* Header */
    read_n(gif, sigver, 3);
    if (memcmp(sigver, "GIF", 3) != 0) {
        //fprintf(stderr, "invalid signature\n");
        goto fail;
    }
    /* Version */
    read_n(gif, sigver, 3);
    if (memcmp(sigver, "89a", 3) != 0) {
        //fprintf(stderr, "invalid version\n");
        goto fail;
    }
    /* Width x Height */
    width  = read_num(gif);
    height = read_num(gif);
    /* FDSZ */
    read_n(gif, &fdsz, 1);
    /* Presence of GCT */
    if (!(fdsz & 0x80)) {
        //fprintf(stderr, "no global color table\n");
        goto fail;
    }
    /* Color Space's Depth */
    depth = ((fdsz >> 4) & 7) + 1;
    /* Ignore Sort Flag. */
    /* GCT Size */
    gct_sz = 1 << ((fdsz & 0x07) + 1);
    /* Background Color Index */
    read_n(gif, &bgidx, 1);
    /* Aspect Ratio */
    read_n(gif, &aspect, 1);
    gif->width  = width;
    gif->height = height;
    /* Read GCT */
    read_n(gif, gif->palette, 3 * gct_sz);
    gif->frame = output;
    read_n(gif, &sep, 1);
    while (sep != ',') {
        if (sep == ';')
            return 0;
        else return -1;
        read_n(gif, &sep, 1);
    }
    if (read_image(gif) == -1)
        return -1;
    return 0;
fail:
    return -1;
}


