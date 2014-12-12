/*
 * yum install gcc libev libev-devel
 * gcc -lev main.c pylon.c servercheck.c valuelist.c daemon.c -o ../pylon
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
#include <stdarg.h>

/* Easy sensible linked lists. */
#include <sys/queue.h>

#include <libev/ev.h>


#include "pylon.h"
/*
#define EVENT_DBG_NONE 0
#define EVENT_DBG_ALL 0xffffffffu
#
#void event_enable_debug_logging(ev_uint32_t which);
*/

extern int daemonize(int nochdir, int noclose);

server_t *server_index;
vlopts_t *opts;
stats_t *stats;
dump_config_t *dump_config;
time_t now;
struct ev_loop *loop;
char* max_memory;
u_char *input_buf;

void outlog(char *str, ... ) {
    va_list args;
    va_start( args, str );
    vprintf(str, args);
    fflush(stdout);
    va_end( args );
}

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

void cleanup(struct ev_loop *loop, ev_timer *ev_cleanup, int revents) {
    outlog("cleanup data\n");
    cleanupServerIndex(server_index, now, opts->cleanup);
    dump_config->abort = 1;
}

void dump(struct ev_loop *loop, ev_idle *ev_idle, int revents) {
    if (dump_config->completed > (time(NULL) - dump_config->dump_interval))
        return;
    outlog("dump\n");
    dump_data(dump_config);
}

void on_read(struct ev_loop *loop, ev_io *ev_read, int revents) {
    outlog("on read\n");

    //int len = read(ev_read->fd, input_buf, BUFLEN);
    int len = 0;
    int read_size;
    while ((read_size = read(ev_read->fd, input_buf + len, 1024)) > 0) {
        len = len + read_size;
        input_buf[len] = 0;
        outlog("read:%d %s\n", read_size, input_buf);
        if (len + 1024 > BUFLEN) {
            outlog("too much data\n");
            break;
        }
    }

    if (len >= 0) {
        input_buf[len] = 0;
        char *output_buf = parseCommand(input_buf, now, server_index, opts, stats, dump_config);

        if (output_buf != NULL && strlen(output_buf) > 0) {
            len = write(ev_read->fd, output_buf, strlen(output_buf));
            if (len == -1) {
                outlog("ERR:write errno:%d\n", errno);
            }
        }

        if (output_buf != NULL) {
            outlog("free pylon on_read output_buf %p\n", output_buf);
            free(output_buf);
        }
    }

    close(ev_read->fd);
    ev_io_stop(loop, ev_read);
    outlog("free pylon on_read ev_read %p\n", ev_read);
    free(ev_read);
    return;
}

void on_accept(struct ev_loop *loop, ev_io *ev_accept, int revents) {
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    stats->connections++;

    int client_fd = accept(ev_accept->fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd == -1) {
        outlog("WARN:accept failed\n");
        return;
    }

    if (setnonblock(client_fd) < 0) {
        outlog("WARN:failed to set client socket non-blocking\n");
    }

    outlog("accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));
    struct ev_io *ev_read = (struct ev_io*) malloc (sizeof(struct ev_io));
    outlog("malloc pylon on_accept ev_read %p\n", ev_read);
    ev_io_init(ev_read, on_read, client_fd, EV_READ);
    ev_io_start(loop, ev_read);
}

void usage(void) {
    printf("usage: pylon <options>\n");
    printf("version: %s-le\n", VERSION);
    printf( "-d            run as a daemon\n"
            "-D <int>      wait <int> seconds after completing a dump to file before starting a new dump\n"
            "-F <file>     dump data to <file>\n"
            "-h            print this message and exit\n"
            "-L <file>     log to <file>\n"
            "              default: /var/log/pylon.log\n"
            "-P <file>     save PID in <file>, only used with -d option\n"
            "              default: /var/run/pylon.pid\n"
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
        outlog("malloc pylon main opts FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    opts->max_buckets = MAX_BUCKETS;
    opts->bucket_size = BUCKET_SIZE;
    opts->buckets = malloc(sizeof(int) * opts->max_buckets);
    if (opts->buckets == NULL) {
        outlog("malloc pylon main opts->buckets FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    opts->bucket_count = 4;
    opts->cleanup =  86400;
    opts->buckets[0] = 300;
    opts->buckets[1] = 1800;
    opts->buckets[2] = 7200;
    opts->buckets[3] = 86400;

    dump_config = malloc(sizeof(dump_config_t));
    if (dump_config == NULL) {
        outlog("malloc pylon main dump_config FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    dump_config->dump_interval = DUMP_INTERVAL;
    dump_config->abort = 0;
    dump_config->dump_fd = 0;

    bool do_daemonize = false;
    char *pid_file = "/var/run/pylon.pid";
    char *log_file = "/var/log/pylon.log";

    while ((c = getopt(argc, argv, "hdP:s:c:L:F:vD:")) != -1) {
        switch (c) {
            case 'c':
                opts->cleanup = atoi(optarg);
                break;
            case 'D':
                dump_config->dump_interval = atoi(optarg);
                break;
            case 'd':
                do_daemonize = true;
                break;
            case 'F':
                dump_config->dump_file = optarg;
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
                outlog("%s\n",VERSION);
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
        outlog("daemonizing\n");
        res = daemonize(0, 0);
        if (res == -1) {
            outlog("ERR:failed to daemon() in order to daemonize\n");
            fflush(stdout);
            exit(-1);
        }
        save_pid(getpid(), pid_file);

        outlog("log_file:'%s'\n",log_file);
        int fd = open(log_file, O_APPEND | O_WRONLY | O_CREAT, 0755);
        if (fd < 0) {
            outlog("ERR:Can't open logfile %s for writing\n", log_file);
            fflush(stdout);
            exit(-1);
        }        
        if(dup2(fd, STDOUT_FILENO) < 0) {
            outlog("ERR:failed to dup2 stdout\n");
            fflush(stdout);
            exit(-1);
        }
        if(dup2(fd, STDERR_FILENO) < 0) {
            outlog("ERR:failed to dup2 stderr\n");
            fflush(stdout);
            exit(-1);
        }
    }
    outlog("initializing.\n");

    input_buf = malloc((BUFLEN * sizeof(u_char)) + 1024);
    if (input_buf == NULL) {
        outlog("malloc pylon main input_buf FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    outlog("malloc pylon main input_buf %p\n", input_buf);

    /* initialize stats */
    stats = malloc(sizeof(stats_t));
    if (stats == NULL) {
        outlog("malloc pylon main stats FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    stats->commands = 0;
    stats->gets = 0;
    stats->adds = 0;
    stats->start_time = time(NULL);

    server_index = newServerIndex();
    dump_config->server_index = server_index;
    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) outlog("ERR:listen failed\n");
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on)) == -1) {
        outlog("ERR:setsockopt failed\n");
        exit(-1);
    }
    
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(SERVER_PORT);
    if (bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) {
        outlog("ERR:bind failed\n");
        exit(-1);
    }

    if (listen(listen_fd, 5) < 0) {
        outlog("ERR:listen failed\n");
        exit(-1);
    }

    if (setnonblock(listen_fd) < 0) {
        outlog("ERR:failed to set server socket to non-blocking\n");
        exit(-1);
    }

    loop = ev_default_loop(0);

    // Add main socket watcher
    ev_io ev_accept;
    ev_io_init(&ev_accept, on_accept, listen_fd, EV_READ);
    ev_io_start(loop, &ev_accept);

    // Add cleanup timer
    ev_timer ev_cleanup;
    ev_timer_init(&ev_cleanup, cleanup, CLEANUP_TIMEOUT, CLEANUP_TIMEOUT );
    ev_timer_start(loop, &ev_cleanup);

    // Add dumper, if enabled.
    if (dump_config->dump_file != NULL) {
        load_data(dump_config, now, opts);
        ev_idle ev_idle;
        ev_idle_init(&ev_idle, dump);
        ev_idle_start(loop, &ev_idle);
    }

    outlog("starting main loop\n");
    ev_run(loop, 0);
    outlog("exited main loop\n");

    if (do_daemonize) remove_pidfile(pid_file);

    return 0;
}
