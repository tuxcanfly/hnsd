#include "config.h"

#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <getopt.h>
#include <sys/types.h>
#include <unistd.h>

#include "hsk.h"
#include "pool.h"
#include "ns.h"
#include "rs.h"
#include "uv.h"

extern char *optarg;
extern int optind, opterr, optopt;

typedef struct hsk_options_s {
  char *config;
  struct sockaddr *ns_host;
  struct sockaddr_storage _ns_host;
  struct sockaddr *rs_host;
  struct sockaddr_storage _rs_host;
  struct sockaddr *ns_ip;
  struct sockaddr_storage _ns_ip;
  char *rs_config;
  uint8_t identity_key_[32];
  uint8_t *identity_key;
  char *seeds;
  int pool_size;
} hsk_options_t;

static void
hsk_options_init(hsk_options_t *opt) {
  opt->config = NULL;
  opt->ns_host = (struct sockaddr *)&opt->_ns_host;
  opt->rs_host = (struct sockaddr *)&opt->_rs_host;
  opt->ns_ip = (struct sockaddr *)&opt->_ns_ip;
  assert(hsk_sa_from_string(opt->ns_host, HSK_NS_IP, HSK_NS_PORT));
  assert(hsk_sa_from_string(opt->rs_host, HSK_RS_IP, HSK_RS_PORT));
  assert(hsk_sa_from_string(opt->ns_ip, HSK_RS_A, 0));
  opt->rs_config = NULL;
  memset(opt->identity_key_, 0, sizeof(opt->identity_key_));
  opt->identity_key = NULL;
  opt->seeds = NULL;
  opt->pool_size = HSK_POOL_SIZE;
}

static void
set_logfile(const char *logfile) {
  assert(logfile);
  freopen(logfile, "a", stdout);
  freopen(logfile, "a", stderr);
#ifdef __linux
  setlinebuf(stdout);
  setlinebuf(stderr);
#endif
}

static bool
daemonize(const char *logfile) {
#ifdef __linux
  if (getppid() == 1)
    return true;
#endif

  int pid = fork();

  if (pid == -1)
    return false;

  if (pid > 0) {
    _exit(0);
    return false;
  }

#ifdef __linux
  setsid();
#endif

  fprintf(stderr, "PID: %d\n", getpid());

  freopen("/dev/null", "r", stdin);

  if (logfile) {
    set_logfile(logfile);
  } else {
    freopen("/dev/null", "w", stdout);
    freopen("/dev/null", "w", stderr);
  }

  return true;
}

static void
help(int r) {
  fprintf(stderr,
    "\n"
    "hnsd 0.0.0\n"
    "  Copyright (c) 2018, Christopher Jeffrey <chjj@handshake.org>\n"
    "\n"
    "Usage: hnsd [options]\n"
    "\n"
    "  -c, --config <config>\n"
    "    Path to config file.\n"
    "\n"
    "  -n, --ns-host <ip[:port]>\n"
    "    IP address and port for root nameserver, e.g. 127.0.0.1:5369.\n"
    "\n"
    "  -r, --rs-host <ip[:port]>\n"
    "    IP address and port for recursive nameserver, e.g. 127.0.0.1:53.\n"
    "\n"
    "  -i, --ns-ip <ip>\n"
    "    Public IP for NS records in the root zone.\n"
    "\n"
    "  -u, --rs-config <config>\n"
    "    Path to unbound config file.\n"
    "\n"
    "  -p, --pool-size <size>\n"
    "    Size of peer pool.\n"
    "\n"
    "  -k, --identity-key <hex-string>\n"
    "    Identity key for signing DNS responses as well as P2P messages.\n"
    "\n"
    "  -s, --seeds <seed1,seed2,...>\n"
    "    Extra seeds to connect to on the P2P network.\n"
    "    Example:\n"
    "      -s aorsxa4ylaacshipyjkfbvzfkh3jhh4yowtoqdt64nzemqtiw2whk@127.0.0.1\n"
    "\n"
    "  -l, --log-file <filename>\n"
    "    Redirect output to a log file.\n"
    "\n"
    "  -d, --daemon\n"
    "    Fork and background the process.\n"
    "\n"
    "  -h, --help\n"
    "    This help message.\n"
    "\n"
  );

  exit(r);
}

static void
parse_arg(int argc, char **argv, hsk_options_t *opt) {
  const static char *optstring = "c:n:r:i:u:p:k:s:l:dh";

  const static struct option longopts[] = {
    { "config", required_argument, NULL, 'c' },
    { "ns-host", required_argument, NULL, 'n' },
    { "rs-host", required_argument, NULL, 'r' },
    { "ns-ip", required_argument, NULL, 'i' },
    { "rs-config", required_argument, NULL, 'u' },
    { "pool-size", required_argument, NULL, 'p' },
    { "identity-key", required_argument, NULL, 'k' },
    { "seeds", required_argument, NULL, 's' },
    { "log-file", required_argument, NULL, 'l' },
    { "daemon", no_argument, NULL, 'd' },
    { "help", no_argument, NULL, 'h' }
  };

  int longopt_idx = -1;
  bool has_ip = false;
  char *logfile = NULL;
  bool background = false;

  optind = 1;

  for (;;) {
    int o = getopt_long(argc, argv, optstring, longopts, &longopt_idx);

    if (o == -1)
      break;

    switch (o) {
      case 'h': {
        return help(0);
      }

      case 'c': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        if (opt->config)
          free(opt->config);

        opt->config = strdup(optarg);

        break;
      }

      case 'n': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        if (!hsk_sa_from_string(opt->ns_host, optarg, HSK_NS_PORT))
          return help(1);

        break;
      }

      case 'r': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        if (!hsk_sa_from_string(opt->rs_host, optarg, HSK_RS_PORT))
          return help(1);

        break;
      }

      case 'i': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        if (!hsk_sa_from_string(opt->ns_ip, optarg, 0))
          return help(1);

        has_ip = true;

        break;
      }

      case 'u': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        if (opt->rs_config)
          free(opt->rs_config);

        opt->rs_config = strdup(optarg);

        break;
      }

      case 'p': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        int size = atoi(optarg);

        if (size <= 0 || size > 1000)
          return help(1);

        opt->pool_size = size;

        break;
      }

      case 'k': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        if (hsk_hex_decode_size(optarg) != 32)
          return help(1);

        if (!hsk_hex_decode(optarg, &opt->identity_key_[0]))
          return help(1);

        opt->identity_key = &opt->identity_key_[0];

        break;
      }

      case 's': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        if (opt->seeds)
          free(opt->seeds);

        opt->seeds = strdup(optarg);

        break;
      }

      case 'l': {
        if (!optarg || strlen(optarg) == 0)
          return help(1);

        if (logfile)
          free(logfile);

        logfile = strdup(optarg);

        break;
      }

      case 'd': {
        background = true;
        break;
      }

      case '?': {
        return help(1);
      }
    }
  }

  if (optind < argc)
    return help(1);

  if (!has_ip)
    hsk_sa_copy(opt->ns_ip, opt->ns_host);

  if (background)
    daemonize(logfile);
  else if (logfile)
    set_logfile(logfile);

  if (logfile)
    free(logfile);
}

static bool
print_identity(const uint8_t *key) {
  hsk_ec_t *ec = hsk_ec_alloc();

  if (!ec)
    return false;

  uint8_t pub[33];

  if (!hsk_ec_create_pubkey(ec, key, pub)) {
    hsk_ec_free(ec);
    return false;
  }

  hsk_ec_free(ec);

  size_t size = hsk_base32_encode_size(pub, 33, false);
  assert(size <= 54);

  char b32[54];
  hsk_base32_encode(pub, 33, b32, false);

  printf("starting with identity key of: %s\n", b32);

  return true;
}

/*
 * Main
 */

int
main(int argc, char **argv) {
  hsk_options_t opt;
  hsk_options_init(&opt);

  parse_arg(argc, argv, &opt);

  int rc = HSK_SUCCESS;
  uv_loop_t *loop = NULL;
  hsk_pool_t *pool = NULL;
  hsk_ns_t *ns = NULL;
  hsk_rs_t *rs = NULL;

  if (opt.identity_key) {
    if (!print_identity(opt.identity_key)) {
      fprintf(stderr, "invalid identity key\n");
      rc = HSK_EFAILURE;
      goto done;
    }
  }

  loop = uv_default_loop();

  if (!loop) {
    fprintf(stderr, "failed initializing loop\n");
    rc = HSK_EFAILURE;
    goto done;
  }

  pool = hsk_pool_alloc(loop);

  if (!pool) {
    fprintf(stderr, "failed initializing pool\n");
    rc = HSK_ENOMEM;
    goto done;
  }

  if (opt.identity_key) {
    if (!hsk_pool_set_key(pool, opt.identity_key)) {
      fprintf(stderr, "failed setting identity key\n");
      rc = HSK_EFAILURE;
      goto done;
    }
  }

  if (!hsk_pool_set_size(pool, opt.pool_size)) {
    fprintf(stderr, "failed setting pool size\n");
    rc = HSK_EFAILURE;
    goto done;
  }

  if (!hsk_pool_set_seeds(pool, opt.seeds)) {
    fprintf(stderr, "failed adding seeds\n");
    rc = HSK_EFAILURE;
    goto done;
  }

  ns = hsk_ns_alloc(loop, pool);

  if (!ns) {
    fprintf(stderr, "failed initializing ns\n");
    rc = HSK_ENOMEM;
    goto done;
  }

  if (!hsk_ns_set_ip(ns, opt.ns_ip)) {
    fprintf(stderr, "failed setting ip\n");
    rc = HSK_EFAILURE;
    goto done;
  }

  if (opt.identity_key) {
    if (!hsk_ns_set_key(ns, opt.identity_key)) {
      fprintf(stderr, "failed setting identity key\n");
      rc = HSK_EFAILURE;
      goto done;
    }
  }

  rs = hsk_rs_alloc(loop, opt.ns_host);

  if (!rs) {
    fprintf(stderr, "failed initializing rns\n");
    rc = HSK_ENOMEM;
    goto done;
  }

  if (opt.rs_config) {
    if (!hsk_rs_set_config(rs, opt.rs_config)) {
      fprintf(stderr, "failed setting rs config\n");
      rc = HSK_EFAILURE;
      goto done;
    }
  }

  if (opt.identity_key) {
    if (!hsk_rs_set_key(rs, opt.identity_key)) {
      fprintf(stderr, "failed setting identity key\n");
      rc = HSK_EFAILURE;
      goto done;
    }
  }

  rc = hsk_pool_open(pool);

  if (rc != HSK_SUCCESS) {
    fprintf(stderr, "failed opening pool: %s\n", hsk_strerror(rc));
    goto done;
  }

  rc = hsk_ns_open(ns, opt.ns_host);

  if (rc != HSK_SUCCESS) {
    fprintf(stderr, "failed opening ns: %s\n", hsk_strerror(rc));
    goto done;
  }

  rc = hsk_rs_open(rs, opt.rs_host);

  if (rc != HSK_SUCCESS) {
    fprintf(stderr, "failed opening rns: %s\n", hsk_strerror(rc));
    goto done;
  }

  printf("starting event loop...\n");

  rc = uv_run(loop, UV_RUN_DEFAULT);

  if (rc != 0) {
    fprintf(stderr, "failed running event loop: %s\n", uv_strerror(rc));
    rc = HSK_EFAILURE;
    goto done;
  }

done:
  if (rs)
    hsk_rs_destroy(rs);

  if (ns)
    hsk_ns_destroy(ns);

  if (pool)
    hsk_pool_destroy(pool);

  if (loop)
    uv_loop_close(loop);

  return rc;
}
