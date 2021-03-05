#include <stdio.h>
#include <stdlib.h>

#include <leveldb/c.h>

#include "chaindb.h"
#include "error.h"


/* Prototypes
 *
 */

static void
hsk_chaindb_log(hsk_chaindb_t *chaindb, const char *fmt, ...);


/* ChainDB
 *
 */

int
hsk_chaindb_init(hsk_chaindb_t *chaindb) {
  if (!chaindb)
    return HSK_EBADARGS;

  chaindb->location = "testdb";
  chaindb->options = leveldb_options_create();
  chaindb->woptions = leveldb_writeoptions_create();
  chaindb->roptions = leveldb_readoptions_create();
  leveldb_options_set_create_if_missing(chaindb->options, 1);

  return HSK_SUCCESS;
}

hsk_chaindb_t *
hsk_chaindb_alloc() {
  hsk_chaindb_t *chaindb = malloc(sizeof(hsk_chaindb_t));

  if (!chaindb)
    return NULL;

  if (hsk_chaindb_init(chaindb) != HSK_SUCCESS) {
    hsk_chaindb_free(chaindb);
    return NULL;
  }

  return chaindb;
}

void
hsk_chaindb_free(hsk_chaindb_t *chaindb) {
  if (!chaindb)
    return;

  hsk_chaindb_uninit(chaindb);
  free(chaindb);
}

void
hsk_chaindb_uninit(hsk_chaindb_t *chaindb) {
  if (!chaindb)
    return;

  chaindb->location = NULL;
  chaindb->db = NULL;
  chaindb->options = NULL;
  chaindb->roptions = NULL;
  chaindb->woptions = NULL;
}

int
hsk_chaindb_open(hsk_chaindb_t *chaindb) {
  if (!chaindb)
    return HSK_EBADARGS;

  char *err = NULL;
  chaindb->db = leveldb_open(chaindb->options, chaindb->location, &err);

  if (err != NULL) {
      hsk_chaindb_log(chaindb, "error opening chaindb", err);
      return HSK_EFAILURE;
  }

  return HSK_SUCCESS;
}

int
hsk_chaindb_write(hsk_chaindb_t *chaindb, char *key, size_t key_len, char *value, size_t value_len) {
  char *err = NULL;
  leveldb_put(chaindb->db, chaindb->woptions, key, key_len, value, value_len, &err);

  if (err != NULL) {
    hsk_chaindb_log(chaindb, "error writing chaindb", err);
    return HSK_EFAILURE;
  }

  return HSK_SUCCESS;
}

int
hsk_chaindb_read(hsk_chaindb_t *chaindb, char *key, size_t key_len, char *value) {
  char *err = NULL;
  size_t read_len;
  value = leveldb_get(chaindb->db, chaindb->woptions, key, key_len, &read_len, &err);

  if (err != NULL) {
    hsk_chaindb_log(chaindb, "error reading chaindb", err);
    return HSK_EFAILURE;
  }

  return HSK_SUCCESS;
}

int hsk_chaindb_close(hsk_chaindb_t *chaindb) {
  leveldb_close(chaindb->db);
  return HSK_SUCCESS;
}

static void
hsk_chaindb_log(hsk_chaindb_t *chaindb, const char *fmt, ...) {
  printf("chaindb: ");

  va_list args;
  va_start(args, fmt);
  vprintf(fmt, args);
  va_end(args);
}
