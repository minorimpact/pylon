/*
 * yum install gcc libevent-devel
 * gcc -levent placeholder.c pylon.c servercheck.c valuelist.c daemon.c -o pylon
 */

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>

/* For inet_ntoa. */
#include <arpa/inet.h>

/* Required by event.h. */
#include <sys/time.h>

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <err.h>
#include <stdbool.h>

/* Easy sensible linked lists. */
#include <sys/queue.h>

/* Libevent. */
#include <event.h>

#include "pylon.h"

/*
#define EVENT_DBG_NONE 0
#define EVENT_DBG_ALL 0xffffffffu
#
#void event_enable_debug_logging(ev_uint32_t which);
*/

struct bufferq {
    u_char *buf;
    int len;
    int offset;
    TAILQ_ENTRY(bufferq) entries;
};

struct client {
    struct event ev_read;
    struct event ev_write;
    unsigned long client_s_addr;

    TAILQ_HEAD(, bufferq) writeq;
};

typedef struct overflow_buffer {
    unsigned long int s_addr;
    char *command_overflow_buffer;
    struct overflow_buffer *prev;
    struct overflow_buffer *next;
} overflow_buffer_t;

overflow_buffer_t *command_overflow_buffers;
vlopts_t *opts;
stats_t *stats;
time_t now;
struct event_base *event_base;
char* max_memory = 0;
server_t *server_index;

int setnonblock(int fd) {
    int flags;

    flags = fcntl(fd, F_GETFL);
    if (flags < 0) {
        return flags;
    }
    flags |= O_NONBLOCK;
    if (fcntl(fd, F_SETFL, flags) < 0) {
        return -1;
    }

    return 0;
}

void on_read(int fd, short ev, void *arg) {
    struct client *client = (struct client *)arg;
    struct bufferq *bufferq;
    u_char *buf;
    int len;
    stats->connections++;

    buf = malloc(BUFLEN * sizeof(u_char));
    if (buf == NULL) {
        printf("malloc pylon on_read buf FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    printf("malloc pylon on_read buf %p\n", buf);

    len = read(fd, buf, BUFLEN);
    if (len == 0) {
        //printf("Client disconnected.\n");
        close(fd);
        event_del(&client->ev_read);
        printf("free pylon on_read client-1 %p\n", client);
        free(client);
        printf("free pylon on_read buf-1 %p\n", buf);
        free(buf);
        return;
    } else if (len < 0) {
        printf("Socket failure, disconnecting client: %s\n", strerror(errno));
        close(fd);
        event_del(&client->ev_read);
        printf("free pylon on_read client-2 %p\n", client);
        free(client);
        printf("free pylon on_read buf-2 %p\n", buf);
        free(buf);
        return;
    }
    stats->pending++;

    strcpy(buf, "placeholder|EOF");
    char *output_buf = parseCommand(buf, client->client_s_addr, now, server_index, opts, stats);

    if (output_buf != NULL && strlen(output_buf) > 0) {
        bufferq = malloc(sizeof(struct bufferq));
        if (bufferq == NULL) {
            printf("malloc pylon on_read bufferq FAILED\n");
            fflush(stdout);
            exit(-1);
        }
        printf("malloc pylon on_read bufferq %p\n", bufferq);
        bufferq->buf = output_buf;
        bufferq->len = strlen(output_buf);
        bufferq->offset = 0;
        TAILQ_INSERT_TAIL(&client->writeq, bufferq, entries);

        event_add(&client->ev_write, NULL);
    } else if (output_buf != NULL) {
        printf("free pylon on_read output_buf %p\n", output_buf);
        free(output_buf);
    }

    printf("free pylon on_read buf %p\n", buf);
    free(buf);
}

void on_write(int fd, short ev, void *arg) {
    struct client *client = (struct client *)arg;
    struct bufferq *bufferq;
    int len;

    bufferq = TAILQ_FIRST(&client->writeq);
    if (bufferq == NULL) {
        return;
    }

    //len = bufferq->len - bufferq->offset;
    len = write(fd, bufferq->buf + bufferq->offset, bufferq->len - bufferq->offset);

    if (len == -1) {
        if (errno == EINTR || errno == EAGAIN) {
            event_add(&client->ev_write, NULL);
            return;
        } else {
            printf("ERR:write\n");
        }
    } else if ((bufferq->offset + len) < bufferq->len) {
        bufferq->offset += len;
        event_add(&client->ev_write, NULL);
        return;
    }

    TAILQ_REMOVE(&client->writeq, bufferq, entries);
    printf("free pylon on_write bufferq->buf %p\n", bufferq->buf);
    free(bufferq->buf);
    printf("free pylon on_write bufferq %p\n", bufferq);
    free(bufferq);
    if (stats->pending > 0) {
        stats->pending--;
    }
}

void on_accept(int fd, short ev, void *arg) {
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct client *client;

    client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd == -1) {
        printf("WARN:accept failed\n");
        return;
    }

    if (setnonblock(client_fd) < 0) printf("WARN:failed to set client socket non-blocking\n");

    client = malloc(sizeof(struct client));
    if (client == NULL) {
        printf("malloc pylon on_accept client FAILED\n");
    }
    printf("malloc pylon on_accept client %p\n", client);

    event_set(&client->ev_read, client_fd, EV_READ|EV_PERSIST, on_read, client);

    event_add(&client->ev_read, NULL);

    event_set(&client->ev_write, client_fd, EV_WRITE, on_write, client);

    TAILQ_INIT(&client->writeq);

    //printf("Accepted connection from %s (%d)\n", inet_ntoa(client_addr.sin_addr),stats->pending);
    client->client_s_addr = client_addr.sin_addr.s_addr;
}

void usage(void) {
    printf("usage: pylon <options>\n");
    printf("version: %s\n", VERSION);
    printf( "-d            run as a daemon\n"
           "-h            print this message and exit\n"
           "-L <file>     log to <file>\n"
           "-P <file>     save PID in <file>, only used with -d option\n"
           "              default: /var/run/pylon.pid\n"
           "              default: /var/log/pylon.log\n"
           "-v            output version information\n"
        );
    return;
}

static void save_pid(const pid_t pid, const char *pid_file) {
    FILE *fp;
    if (pid_file == NULL)
        return;

    if ((fp = fopen(pid_file, "w")) == NULL) {
        fprintf(stderr, "Could not open the pid file %s for writing\n", pid_file);
        return;
    }

    fprintf(fp,"%ld\n", (long)pid);
    if (fclose(fp) == -1) {
        fprintf(stderr, "Could not close the pid file %s.\n", pid_file);
        return;
    }
}

static void remove_pidfile(const char *pid_file) {
  if (pid_file == NULL)
      return;

  if (unlink(pid_file) != 0) {
      fprintf(stderr, "Could not remove the pid file %s.\n", pid_file);
  }

}

int main(int argc, char **argv) {
    int listen_fd;
    struct sockaddr_in listen_addr;
    int reuseaddr_on = 1;
    int c;
    int i;
    int step;

    srand(time(NULL));
    opts = malloc(sizeof(vlopts_t));
    if (opts == NULL) {
        printf("malloc pylon main opts FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    opts->max_buckets = MAX_BUCKETS;
    opts->bucket_size = BUCKET_SIZE;
    opts->buckets = malloc(sizeof(int) * opts->max_buckets);
    if (opts->buckets == NULL) {
        printf("malloc pylon main opts->buckets FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    opts->bucket_count = 4;
    opts->buckets[0] = 300;
    opts->buckets[1] = 1800;
    opts->buckets[2] = 7200;
    opts->buckets[3] = 86400;

    char *cvalue = NULL;
    bool do_daemonize = false;
    char *pid_file = "/var/run/pylon.pid";
    char *log_file = "/var/log/pylon.log";


    while ((c = getopt(argc, argv, "hdP:s:c:L:F:vD:")) != -1) {
        switch (c) {
            case 'D':
            case 'F':
                break;
            case 'd':
                do_daemonize = true;
                break;
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
                break;
            case 'L':
                log_file = optarg;
                break;
            case 'P':
                pid_file = optarg;
                break;
            case 's':
                if (opts->bucket_count >= opts->max_buckets) {
                    break;
                }
                step = atoi(optarg);
                for (i=opts->bucket_count;i>0;i--) {
                    if (step > opts->buckets[i-1]) {
                        opts->buckets[i] = step;
                        break;
                    }
                    opts->buckets[i] = opts->buckets[i-1];
                }
                if (step < opts->buckets[0]) {
                    opts->buckets[0] = step;
                }
                opts->bucket_count++;
                break;
            case 'v':
                printf("%s\n",VERSION);
                exit(0);
                break;
            default:
                usage();
                //fprintf(stderr, "Illegal argument \"%c\"\n", c);
                exit(EXIT_FAILURE);
        }
    }

    /* daemonize if requested */
    /* if we want to ensure our ability to dump core, don't chdir to / */
    if (do_daemonize) {
        int res;

        // I have no idea how stuff works. If I don't use stdout first, then nothing gets written to the log file.
        printf("daemonizing.\n");
        res = daemonize(0, 0);
        if (res == -1) {
            printf("ERR:failed to daemon() in order to daemonize\n");
            fflush(stdout);
            exit(-1);
        }
        save_pid(getpid(), pid_file);

        printf("log_file:'%s'\n",log_file);
        int fd = open(log_file, O_APPEND | O_WRONLY | O_CREAT, 0755);
        if (fd < 0) {
            printf("ERR:Can't open logfile %s for writing\n", log_file);
            fflush(stdout);
            exit(-1);
        }        
        if(dup2(fd, STDOUT_FILENO) < 0) {
            printf("ERR:failed to dup2 stdout\n");
            fflush(stdout);
            exit(-1);
        }
        if(dup2(fd, STDERR_FILENO) < 0) {
            printf("ERR:failed to dup2 stderr\n");
            fflush(stdout);
            exit(-1);
        }
    }
    printf("initializing.\n");

    /* initialize stats */
    stats = malloc(sizeof(stats_t));
    if (stats == NULL) {
        printf("malloc pylon main stats FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    stats->commands = 0;
    stats->gets = 0;
    stats->adds = 0;
    stats->start_time = time(NULL);

    command_overflow_buffers = malloc(sizeof(overflow_buffer_t));
    if (command_overflow_buffers == NULL) {
        printf("malloc pylon main command_overflow_buffers FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    command_overflow_buffers->next = NULL;
    command_overflow_buffers->prev = NULL;

    event_base = event_init();

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) printf("ERR:listen failed\n");
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on)) == -1) printf("ERR:setsockopt failed\n");
    
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(SERVER_PORT);
    if (bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) printf("ERR:bind failed\n");

    if (listen(listen_fd, 5) < 0) printf("ERR:listen failed\n");

    if (setnonblock(listen_fd) < 0) printf("ERR:failed to set server socket to non-blocking\n");

    struct event ev_accept;
    event_set(&ev_accept, listen_fd, EV_READ|EV_PERSIST, on_accept, NULL);
    event_add(&ev_accept, NULL);

    event_dispatch();

    if (do_daemonize) remove_pidfile(pid_file);

    return 0;
}
