#include "servercheck.h"

/* Port to listen on. */
#define SERVER_PORT 5555
#define MAX_BUCKETS 6
#define BUCKET_SIZE 575
#define CLEANUP_TIMEOUT 300.
#define DUMP_INTERVAL 10
#define VERSION "0.0.0-26"

/* Length of each buffer in the buffer queue.  Also becomes the amount
 * of data we try to read per call to read(2). */
#define BUFLEN (20 * BUCKET_SIZE * MAX_BUCKETS) + 150

typedef struct stats {
    int connections;
    int commands;
    int adds;
    int gets;
    int pending;
    int dumps;
    time_t start_time;
} stats_t;

typedef struct dump_config {
    char *dump_file;
    char *dump_file_tmp;
    char *checkdump;
    int dump_interval;
    struct timeval tv;
    int dump_fd;
    int loading;
    server_t *server;
    check_t *check;
    server_t *server_index;
    time_t now;
    time_t completed;
    int abort;
} dump_config_t;

typedef struct vlopts {
    int max_buckets;
    int bucket_size;
    int bucket_count;
    int *buckets;
    int cleanup;
} vlopts_t;

char* parseCommand(char *buf, time_t now, server_t *server_index, vlopts_t *opts, stats_t *stats, dump_config_t *dump_config);
void dump_data(dump_config_t *dump_config);
void load_data(dump_config_t *dump_config, time_t now, vlopts_t *opts);

