/*
 * libevent echo server example.
 *
 * yum install gcc libevent-devel
 * gcc -levent pylon.c servercheck.c valuelist.c daemon.c -o pylon
 */

#include "pylon.h"
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

#include "servercheck.h"


/*
#define EVENT_DBG_NONE 0
#define EVENT_DBG_ALL 0xffffffffu
#
#void event_enable_debug_logging(ev_uint32_t which);
*/

/* Port to listen on. */
#define SERVER_PORT 5555
#define MAX_BUCKETS 6
#define BUCKET_SIZE 575
#define CLEANUP_TIMEOUT 3600
#define VERSION "0.0.0-16"

/* Length of each buffer in the buffer queue.  Also becomes the amount
 * of data we try to read per call to read(2). */
#define BUFLEN (20 * BUCKET_SIZE * MAX_BUCKETS) + 100

typedef struct stats {
    int connections;
    int commands;
    int adds;
    int gets;
    int pending;
    int dumps;
    time_t start_time;
} stats_t;

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

typedef struct dump_config {
    char *dump_file;
    char *dump_file_tmp;
    char *checkdump;
    struct event ev_dump;   
    struct timeval tv;
    int dump_fd;
    int frequency;
    server_t *server;
    check_t *check;
} dump_config_t;

overflow_buffer_t *command_overflow_buffers;
server_t *server_index;
vlopts_t *opts;
stats_t *stats;
dump_config_t *dump_config;
time_t now;
struct event_base *event_base;

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


char* parseCommand(char *buf, unsigned long s_addr) {
    int i;
    char *tmp = NULL;
    u_char *output_buf;
    char *command;
    now = time(NULL);
    int len = strlen(buf);

    stats->commands++;

    output_buf = malloc(BUFLEN*sizeof(u_char));
    if (output_buf == NULL) {
        printf("malloc pylon command output_buf FAILED\n");
        exit(-1);
    }
    printf("malloc pylon command output_buf %p\n", output_buf);
    output_buf[0] = 0;

    if (command_overflow_buffers->next != NULL) {
        overflow_buffer_t *ob;
        ob = command_overflow_buffers->next;
        while (ob != NULL) {
            if (ob->s_addr == s_addr) {
                tmp = malloc((len + strlen(ob->command_overflow_buffer) + 1) * sizeof(char));
                if (tmp == NULL) {
                    printf("malloc pylon parseCommand tmp-1 FAILED\n");
                    exit(-1);
                }
                printf("malloc pylon parseCommand tmp-1 %p\n", tmp);
                strcpy(tmp, ob->command_overflow_buffer);
                strcat(tmp, buf);
                if (ob->next != NULL) {
                    ob->next->prev = ob->prev;
                }
                if (ob->prev != NULL) {
                    ob->prev->next = ob->next;
                }
                printf("free pylon parseCommand ob->command_overflow_buffer %p\n", ob->command_overflow_buffer);
                free(ob->command_overflow_buffer);
                printf("free pylon parseCommand ob %p\n", ob);
                free(ob);

                break;
            }
            ob = ob->next;
        }
    }

    if (tmp == NULL) {
        tmp = malloc((len+1) * sizeof(char));
        if (tmp == NULL) {
            printf("malloc pylon parseCommand tmp-2 FAILED\n");
            exit(-1);
        }
        printf("malloc pylon parseCommand tmp-2 %p\n", tmp);
        strcpy(tmp, buf);
    }

    if (strcmp(tmp + (strlen(tmp) - 4), "EOF\n") != 0) {
        overflow_buffer_t *ob;
        ob = malloc(sizeof(overflow_buffer_t));
        if (ob == NULL) {
            printf("malloc pylon parseCommand ob FAILED\n");
            exit(-1);
        }
        printf("malloc pylon parseCommand ob %p\n", ob);
        ob->s_addr = s_addr;
        ob->command_overflow_buffer = malloc((strlen(tmp) + 1) * sizeof(char));
        if (ob->command_overflow_buffer == NULL) {
            printf("malloc pylon parseCommand ob->command_overflow_buffer FAILED\n");
            exit(-1);
        }
        printf("malloc pylon parseCommand ob->command_overflow_buffer %p\n", ob->command_overflow_buffer);
        strcpy(ob->command_overflow_buffer, tmp);
        ob->next = command_overflow_buffers->next;
        ob->prev = command_overflow_buffers;
        if (ob->next != NULL) {
            ob->next->prev = ob;
        }
        command_overflow_buffers->next = ob;
        printf("free pylon parseCommand tmp %p\n", tmp);
        free(tmp);
        return output_buf;
    }

    command = strtok(tmp, "|\n\r");
    if (strcmp(command, "add") == 0) {
        char *check_id = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");
        char *value = strtok(NULL, "|\n\r");
        char *type_s = strtok(NULL, "|\n\r");
        valueList_t *vl = NULL;
        int type = 0;

        //printf("parseCommand: add|%s|%s|%s|%s\n", check_id, server_id, value, type_s);

        if (server_id != NULL && value != NULL && strcmp(server_id,"EOF") !=0 && strcmp(value,"EOF") != 0) {
            if (strcmp(type_s, "counter") == 0) {
                type = 1;
            }

            vl = getValueList(server_index, server_id, check_id, now, 0, opts, 1);
            if (vl != NULL) {
                addValue(vl, atof(value), now, type);
                strcpy(output_buf, "OK\n");
            } else { 
                strcpy(output_buf, "FAIL\n");
            }
        } else { 
            strcpy(output_buf, "INVALID\n");
        }
        stats->adds++;
    } else if (strcmp(command, "load") == 0) {
        char *check_id = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");
        char *first_s = strtok(NULL, "|\n\r");
        char *size_s = strtok(NULL, "|\n\r");
        char *step_s = strtok(NULL, "|\n\r");


        time_t first = atoi(first_s);
        int size = atoi(size_s);
        int step = atoi(step_s);
        printf("parseCommand: load|%s|%s|%d|%d|%d\n", check_id, server_id, first, size, step);

        double *data = malloc(size*sizeof(double));
        if (data == NULL) {
            printf("malloc pylon parseCommand data FAILED\n");
            exit(-1);
        }
        printf("malloc pylon parseCommand data %p\n", data);
        char *d = strtok(NULL, "|\n\r");
        for (i=0;i<size;i++) {
            if (d != NULL) {
                data[i] = atof(d);
                d = strtok(NULL, "|\n\r");
            } else {
                data[i] = 0.0/0.0;
            }
        }

        valueList_t *vl = loadData(server_index, server_id, check_id, first, size, step, data, now, opts);
        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "dump") == 0) {
        char *check_id = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");

        printf("parseCommand: dump|%s|%s\n", check_id, server_id);

        if (server_id != NULL && check_id != NULL) {
            valueList_t *vl = getValueList(server_index, server_id, check_id, now, 0, opts, 0);
            while (vl != NULL) {
                //printf("dumping %s, %s, %d\n", server_id, check_id, vl->step);
                dumpValueList(check_id, server_id, vl, now, output_buf);
                vl = vl->next;
            }
        } else {
            strcpy(output_buf, "INVALID\n");
        }
    } else if (strcmp(command, "deleteserver") == 0) {
        char *server_id = strtok(NULL, "|\n\r");
        printf("parseCommand: deleteserver|%s\n", server_id);
        deleteServerByName(server_index, server_id);
    } else if (strcmp(command, "get") == 0 || strcmp(command, "avg") == 0) {
        char *check_id = strtok(NULL, "|\n\r");
        char *range_s = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");
        char tmp_str[50];
        char *tmp_output_buf = calloc(BUFLEN, sizeof(char));
        if (tmp_output_buf == NULL) {
            printf("malloc pylon parseCommand tmp_output_buf FAILED\n");
            exit(-1);
        }
        printf("malloc pylon parseCommand tmp_output_buf %p\n", tmp_output_buf);
        time_t first = 0;
        int range = 0;
        int size = 0;
        int server_count = 0;

        printf("parseCommand: get|%s|%s|%s\n", check_id, range_s, server_id);
        range = atoi(range_s);

        valueList_t *vl = getValueList(server_index, server_id, check_id, now, range, opts,0);

        if (vl == NULL) {
            server_id = strtok(NULL, "|\n\r");
            while (vl==NULL && strcmp(server_id,"EOF") != 0 && server_id != NULL) {
                //printf("pylon.parseCommand.%s:looking for first server that has a valuelist for %s\n",command, check_id);
                vl = getValueList(server_index, server_id, check_id, now, range, opts,0);
                server_id = strtok(NULL, "|\n\r");
            }
        }

        if (vl != NULL) {
            // Initialize the data set to return.
            double data[vl->size];
            for (i=0; i<vl->size; i++) {
                data[i] = 0.0/0.0;
            }

            // Use the first server as a model for the data set.
            if (strcmp(server_id,"EOF") != 0 && vl != NULL && server_id != NULL) {
                // Walk through the list of requested servers and build our data list.
                while (strcmp(server_id,"EOF") != 0 && server_id != NULL) {
                    //printf("pylon.parseCommand.%s:looking for more sevrers that have a valuelist for %s\n",command, check_id);
                    valueList_t *vl2 = getValueList(server_index, server_id, check_id, now, range, opts,0);
                    if (vl2 != NULL && vl2->size == vl->size && vl2->step == vl->step) {
                        //printf("pylon.parseCommand.%s:found %s\n",command, server_id);
                        addValueListToData(vl2, data);
                        server_count++;
                    }
                    server_id = strtok(NULL, "|\n\r");
                }
                
                if (range == 0) {
                    range = vl->first;
                }

                for (i=0; i<vl->size; i++) {
                    if ((vl->first + (vl->step * i) > range)) {
                        if (!first) {
                            first = vl->first + (vl->step * i);
                        }
                        size++;
                        if (strcmp(command, "avg") == 0) {
                            data[i] = data[i]/server_count;
                        }
                        sprintf(tmp_str,"%f",data[i]);
                        strcat(tmp_output_buf,tmp_str); 
                        if (i<(vl->size - 1)) {
                            strcat(tmp_output_buf,"|");
                        }
                    }
                }

                sprintf(tmp_str, "%d|", first);
                strcpy(output_buf, tmp_str);
                sprintf(tmp_str, "%d|", size);
                strcat(output_buf, tmp_str);
                sprintf(tmp_str, "%d|", vl->step);
                strcat(output_buf, tmp_str);
                strcat(output_buf, tmp_output_buf);
                strcat(output_buf,"\n");
            }
        } else {
            // No data to return.
            //printf("pylon.parseCommand.%s:No data.\n",command);
            strcpy(output_buf, "0|0|0\n");
        }
        printf("free pylon parseCommand tmp_output_buf %p\n", tmp_output_buf);
        free(tmp_output_buf);
        stats->gets++;
    } else if (strcmp(command, "reset") == 0) {
        printf("parseCommand: reset\n");
        server_t *server = server_index->next;

        while(server != NULL) {
            server_index->next = server->next;
            deleteServer(server);
            server = server_index->next;
        }
        server_index->next = NULL;
        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "status") == 0) {
        printf("parseCommand: status\n");
        char tmp_str[255];
        int server_count = getServerCount(server_index);
        int check_count = getCheckCount(server_index);
        long int size = serverIndexSize(server_index);
        int overflow_buffer_count = 0;
        overflow_buffer_t *ob = command_overflow_buffers->next;

        while (ob != NULL) {
            overflow_buffer_count++;
            ob = ob->next;
        }
        sprintf(tmp_str, "servers=%d checks=%d size=%ld uptime=%d connections=%d commands=%d adds=%d gets=%d overflow_buffer_count=%d dumps=%d\n", server_count, check_count, size, (now - stats->start_time), stats->connections, stats->commands, stats->adds, stats->gets, overflow_buffer_count, stats->dumps);
        printf("%s\n", tmp_str);

        strcpy(output_buf, tmp_str);

    } else if (strcmp(command, "checks") == 0) {
        char *server_id = strtok(NULL, "|\n\r");
        char tmp2[1024];

        printf("parseCommand: checks|%s\n", server_id);

        while (server_id != NULL && strcmp(server_id,"EOF") != 0) {
            int i;
            char *tmp_str = getCheckList(server_index, server_id);
            if (tmp_str != NULL) {
                int strpos = 0;
                //printf("pylon.parseCommand.%s:strlen(tmp_str)=%d\n",command,strlen(tmp_str));
                for (i=0; i <= strlen(tmp_str); i++) {
                    if (*(tmp_str+i) == '|' || i == strlen(tmp_str)) {
                        int newlen = i - strpos;
                        strncpy(tmp2,tmp_str + strpos,newlen);
                        tmp2[newlen] = '|';
                        tmp2[newlen+1] = 0;
                        if (strstr(output_buf, tmp2) == NULL) {
                            strcat(output_buf, tmp2);
                        }
                        //printf("pylon.parseCommand.%s:tmp2=%s,i=%d,strpos=%d,strlen(tmp_str)=%d,strlen(output_buf)=%d,tmp_str=%s\n", command, tmp2, i, strpos, strlen(tmp_str), strlen(output_buf), tmp_str + strpos);
                        i++;
                        strpos = i;
                    }
                }
                printf("free pylon parseCommand tmp_str-1 %p\n", tmp_str);
                free(tmp_str);
                printf("command pylon parseCommand %s output_buf %d(%d)\n", command, strlen(output_buf), BUFLEN*sizeof(u_char));
            }
            server_id = strtok(NULL, "|\n\r");
        }
        output_buf[strlen(output_buf) - 1] = 0;
        strcat(output_buf,"\n");
    } else if (strcmp(command, "version") == 0) {
        char *tmp_str;

        printf("parseCommand:version\n");
        strcpy(output_buf, VERSION);
        strcat(output_buf, "\n");
    } else if (strcmp(command, "servers") == 0) {
        char *tmp_str;

        printf("parseCommand:servers\n");
        tmp_str = getServerList(server_index);
        if (tmp_str != NULL) {
            strcpy(output_buf, tmp_str);
            printf("free pylon parseCommand tmp_str-2 %p\n", tmp_str);
            free(tmp_str);
        }
        strcat(output_buf, "\n");
    } else {
        printf("parseCommand: unknown command|%s\n", command);
        strcpy(output_buf, "unknown command\n");
    }

    // Blank line signals the end of output.
    strcat(output_buf, "\n");
    
    printf("free pylon parseCommand tmp %p\n", tmp);
    free(tmp);
    return output_buf;
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
        printf("Socket failure, disconnecting client: %s", strerror(errno));
        close(fd);
        event_del(&client->ev_read);
        printf("free pylon on_read client-2 %p\n", client);
        free(client);
        printf("free pylon on_read buf-2 %p\n", buf);
        free(buf);
        return;
    }
    stats->pending++;

    buf[len] = 0;
    char *output_buf = parseCommand(buf, client->client_s_addr);

    if (output_buf != NULL && strlen(output_buf) > 0) {
        bufferq = malloc(sizeof(struct bufferq));
        if (bufferq == NULL) {
            printf("malloc pylon on_read bufferq FAILED\n");
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
            err(1, "write");
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

void cleanup_data (int fd, short ev, void *arg) {
    cleanupServerIndex(server_index, now, opts->cleanup);
    struct timeval tv;
    tv.tv_sec = CLEANUP_TIMEOUT;
    tv.tv_usec = 0;
    event_add((struct event *)arg, &tv);
}

void dump_data(int fd, short ev, void *arg) {
    dump_config_t *dump_config = arg;

    // Advance the current dump pointer.
    if (dump_config->check != NULL) {
        dump_config->check = dump_config->check->next;
    }
    if (dump_config->check == NULL) {
        if (dump_config->server == NULL) {
            dump_config->server = server_index->next;
        } else {
            dump_config->server = dump_config->server->next;
        }
        if (dump_config->server != NULL) {
            dump_config->check = dump_config->server->check->next;
        }
    } 

    // Process the current pointer.
    if (dump_config->check == NULL && dump_config->server == NULL && dump_config->dump_fd > 0) {
        // Reached the end of the set.  Close the temp file, and swap it to live.
        close(dump_config->dump_fd);
        dump_config->dump_fd = 0;
        rename(dump_config->dump_file_tmp, dump_config->dump_file);
    } else if (dump_config->check != NULL && dump_config->server != NULL) {
        // Sitting on a valid entry.  
        if (dump_config->dump_fd < 1) {
            // Temp file hasn't been created.  Do so.
            unlink(dump_config->dump_file_tmp);
            dump_config->dump_fd = open(dump_config->dump_file_tmp, O_WRONLY|O_CREAT, 0755);
            if (dump_config->dump_fd < 0) {
                printf("pylon.dump_data:Can't open %s for writing\n", dump_config->dump_file_tmp);
                exit(-1);
            }
        }
        // Dump the current check to the temp file.
        dump_config->checkdump[0] = 0;

        printf("dumping %s. buffer size: %d\n", dump_config->check->name, (BUFLEN*sizeof(u_char)));
        dumpCheck(dump_config->check, dump_config->server->name, now, dump_config->checkdump);
        write(dump_config->dump_fd, dump_config->checkdump, strlen(dump_config->checkdump));
        stats->dumps++;
    }

    // Re-add the event so it fires again.
    dump_config->tv.tv_sec = 0;
    dump_config->tv.tv_usec = (int) 1000000/dump_config->frequency;
    event_add(&dump_config->ev_dump, &dump_config->tv);
}

void on_accept(int fd, short ev, void *arg) {
    int client_fd;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct client *client;

    client_fd = accept(fd, (struct sockaddr *)&client_addr, &client_len);
    if (client_fd == -1) {
        warn("accept failed");
        return;
    }

    if (setnonblock(client_fd) < 0) warn("failed to set client socket non-blocking");

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
           "-D <checks/s> dump frequency; will write X checks per second.\n"
           "-F <file>     dump data to <file>\n"
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
    int cleanup;
    int step;

    srand(time(NULL));
    opts = malloc(sizeof(vlopts_t));
    if (opts == NULL) {
        printf("malloc pylon main opts FAILED\n");
        exit(-1);
    }
    opts->max_buckets = MAX_BUCKETS;
    opts->bucket_size = BUCKET_SIZE;
    opts->buckets = malloc(sizeof(int) * opts->max_buckets);
    if (opts->buckets == NULL) {
        printf("malloc pylon main opts->buckets FAILED\n");
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
        printf("malloc pylon main dump_config FAILED\n");
        exit(-1);
    }
    dump_config->dump_fd = 0;
    dump_config->frequency = 25;

    char *cvalue = NULL;
    bool do_daemonize = false;
    char *pid_file = "/var/run/pylon.pid";
    char *log_file = "/var/log/pylon.log";
    char *dump_file = NULL;


    while ((c = getopt(argc, argv, "hdP:s:c:L:F:vD:")) != -1) {
        switch (c) {
            case 'c':
                cleanup = atoi(optarg);
                opts->cleanup = cleanup;
                break;
            case 'd':
                do_daemonize = true;
                break;
            case 'D':
                dump_config->frequency = atoi(optarg);
                if (dump_config->frequency < 1) {
                    dump_config->frequency = 1;
                }
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
            fprintf(stderr, "failed to daemon() in order to daemonize\n");
            return 1;
        }
        save_pid(getpid(), pid_file);

        printf("log_file:'%s'\n",log_file);
        int fd = open(log_file, O_APPEND | O_WRONLY | O_CREAT, 0755);
        if (fd < 0) {
            perror("open logfile");
            exit(-1);
        }        
        if(dup2(fd, STDOUT_FILENO) < 0) {
            perror("dup2 stdout");
            exit(-1);
        }
    }
    printf("initializing.\n");

    /* initialize stats */
    stats = malloc(sizeof(stats_t));
    if (stats == NULL) {
        printf("malloc pylon main stats FAILED\n");
        exit(-1);
    }
    stats->commands = 0;
    stats->gets = 0;
    stats->adds = 0;
    stats->start_time = time(NULL);

    command_overflow_buffers = malloc(sizeof(overflow_buffer_t));
    if (command_overflow_buffers == NULL) {
        printf("malloc pylon main command_overflow_buffers FAILED\n");
        exit(-1);
    }
    command_overflow_buffers->next = NULL;
    command_overflow_buffers->prev = NULL;

    event_base = event_init();

    server_index = newServerIndex();

    listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_fd < 0) err(1, "listen failed");
    if (setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuseaddr_on, sizeof(reuseaddr_on)) == -1) err(1, "setsockopt failed");
    
    memset(&listen_addr, 0, sizeof(listen_addr));
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(SERVER_PORT);
    if (bind(listen_fd, (struct sockaddr *)&listen_addr, sizeof(listen_addr)) < 0) err(1, "bind failed");

    if (listen(listen_fd, 5) < 0) err(1, "listen failed");

    if (setnonblock(listen_fd) < 0) err(1, "failed to set server socket to non-blocking");

    struct event ev_accept;
    event_set(&ev_accept, listen_fd, EV_READ|EV_PERSIST, on_accept, NULL);
    event_add(&ev_accept, NULL);

    // Add cleanup event
    struct event ev_cleanup;
    event_set(&ev_cleanup, -1, EV_TIMEOUT|EV_PERSIST, cleanup_data, &ev_cleanup);
    struct timeval tv;
    tv.tv_sec = CLEANUP_TIMEOUT;
    tv.tv_usec = 0;
    event_add(&ev_cleanup, &tv);

    // Add dumper, if enabled.
    if (dump_config->dump_file != NULL) {
        dump_config->dump_file_tmp = malloc(sizeof(char) * (strlen(dump_config->dump_file) + 5));
        dump_config->checkdump = malloc(BUFLEN*sizeof(u_char));
        if (dump_config->checkdump == NULL) {
            printf("malloc pylon main dump_config->checkdump FAILED\n");
            exit(-1);
        }
        strcpy(dump_config->dump_file_tmp, dump_config->dump_file);
        strcat(dump_config->dump_file_tmp, ".tmp");

        // Load data from existing dump file
        int dump_fd = open(dump_config->dump_file, O_RDONLY);
        if ( dump_fd > 0) {
            time_t load_start = time(NULL);
            int load_count = 0;
            printf("loading data from %s: %d\n", dump_config->dump_file, load_start);
            char read_buf[1024];
            char *read_buf_tmp;
            char *output_buf = malloc(BUFLEN*sizeof(u_char));
            if (output_buf == NULL) {
                printf("malloc pylon main output_buf FAILED\n");
                exit(-1);
            }
            printf("malloc pylon main output_buf %p\n", output_buf);
            output_buf[0] = 0;
            char *pos;
            int read_size;
            while (read_size = read(dump_fd, read_buf, 1024)){
                read_buf[read_size] = 0;
                read_buf_tmp = read_buf;
                // Each line is a separate record.
                while (pos = strchr(read_buf_tmp, '\n')) {
                    pos[0] = 0;
                    strcat(output_buf, read_buf_tmp);
                    //printf("output='%s'\n",output_buf);

                    char *check_id = strtok(output_buf, "|\n\r");
                    char *server_id = strtok(NULL, "|\n\r");
                    char *first_s = strtok(NULL, "|\n\r");
                    char *size_s = strtok(NULL, "|\n\r");
                    char *step_s = strtok(NULL, "|\n\r");
                    time_t first = atoi(first_s);
                    int size = atoi(size_s);
                    int step = atoi(step_s);
                    //printf("parseCommand: load|%s|%s|%d|%d|%d\n", check_id, server_id, first, size, step);

                    double *data = malloc(size*sizeof(double));
                    if (data == NULL) {
                        printf("malloc pylon main data FAILED\n");
                        exit(-1);
                    }

                    char *d = strtok(NULL, "|\n\r");
                    for (i=0;i<size;i++) {
                        if (d != NULL) {
                            data[i] = atof(d);
                            d = strtok(NULL, "|\n\r");
                        } else {
                            data[i] = 0.0/0.0;
                        }
                    }

                    if (server_id != NULL && check_id != NULL && size > 0 && step > 0) {
                        loadData(server_index, server_id, check_id, first, size, step, data, now, opts);
                        load_count++;
                    } else {
                        //printf("free pylon main data %p\n", data);
                        free(data);
                    }

                    output_buf[0] = 0;
                    read_buf_tmp = pos + 1;
                }
                strcat(output_buf, read_buf_tmp);
                printf("strlen pylon main output_buf %d (%d)\n", strlen(output_buf), BUFLEN*sizeof(u_char));
            }
            time_t load_end = time(NULL);
            printf("free pylon main output_buf %p\n", output_buf);
            free(output_buf);
            printf("finished loading data from %s: %d\n", dump_config->dump_file, load_end);
            if (load_end > load_start) {
                printf("load time: %d seconds, %d checks, %.2f records/sec\n", (load_end - load_start), (load_count/4), ((load_count/4)/(load_end-load_start)));
            }
        }
        close(dump_fd);

        event_set(&dump_config->ev_dump, -1, EV_TIMEOUT|EV_PERSIST, dump_data, dump_config);
        dump_config->tv.tv_sec = 1;
        dump_config->tv.tv_usec = 0;
        event_add(&dump_config->ev_dump, &dump_config->tv);
    }
    //exit(0);

    event_dispatch();

    if (do_daemonize) remove_pidfile(pid_file);

    return 0;
}
