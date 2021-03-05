#ifndef _HSK_CHAINDB
#define _HSK_CHAINDB

#include <leveldb/c.h>
#include <stdio.h>

/*
 * Types
 */

typedef struct hsk_chaindb_s {
    char * location;
    leveldb_t *db;
    leveldb_options_t *options;
    leveldb_readoptions_t *roptions;
    leveldb_writeoptions_t *woptions;
} hsk_chaindb_t;

int
hsk_chaindb_init();

void
hsk_chaindb_free(hsk_chaindb_t *chaindb);

void
hsk_chaindb_uninit(hsk_chaindb_t *chaindb);

hsk_chaindb_t *
hsk_chaindb_alloc();

void
hsk_chaindb_free(hsk_chaindb_t *chaindb);

void
hsk_chaindb_uninit(hsk_chaindb_t *chaindb);

int
hsk_chaindb_open(hsk_chaindb_t *chaindb);

int
hsk_chaindb_close(hsk_chaindb_t *chaindb);

int
hsk_chaindb_write(hsk_chaindb_t *chaindb, char *key, size_t key_len, char *value, size_t value_len);
#endif
