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

/* Length of each buffer in the buffer queue.  Also becomes the amount
 * of data we try to read per call to read(2). */
#define BUFLEN 10240 * MAX_BUCKETS


typedef struct stats {
    int connections;
    int commands;
    int adds;
    int gets;
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

overflow_buffer_t *command_overflow_buffers;
server_t *server_index;
vlopts_t *opts;
stats_t *stats;
time_t now;


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


char* parseCommand(char *buf, int len, unsigned long s_addr) {
    int i;
    char *tmp;
    u_char *output_buf;
    char *command;
    now = time(NULL);

    stats->commands++;
    tmp = NULL;

    if (!(rand() % 1000)) {
        cleanupServerIndex(server_index, now, opts->cleanup);
    }

    buf[len]=0;
    output_buf = malloc(BUFLEN*sizeof(u_char));
    output_buf[0] = 0;

    if (command_overflow_buffers->next != NULL) {
        overflow_buffer_t *ob;
        ob = command_overflow_buffers->next;
        while (ob != NULL) {
            if (ob->s_addr == s_addr) {
                tmp = malloc((len + strlen(ob->command_overflow_buffer) + 1) * sizeof(char));
                strcpy(tmp, ob->command_overflow_buffer);
                strcat(tmp, buf);
                if (ob->next != NULL) {
                    ob->next->prev = ob->prev;
                }
                if (ob->prev != NULL) {
                    ob->prev->next = ob->next;
                }
                free(ob->command_overflow_buffer);
                free(ob);

                break;
            }
            ob = ob->next;
        }
    }

    if (tmp == NULL) {
        tmp = malloc((len+1) * sizeof(char));
        strcpy(tmp, buf);
    }

    if (strcmp(tmp + (strlen(tmp) - 4), "EOF\n") != 0) {

        overflow_buffer_t *ob;
        ob = malloc(sizeof(overflow_buffer_t));
        ob->s_addr = s_addr;
        ob->command_overflow_buffer = malloc((strlen(tmp) + 1) * sizeof(char));
        strcpy(ob->command_overflow_buffer, tmp);
        ob->next = command_overflow_buffers->next;
        ob->prev = command_overflow_buffers;
        if (ob->next != NULL) {
            ob->next->prev = ob;
        }
        command_overflow_buffers->next = ob;
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

        printf("parseCommand: add|%s|%s|%s|%s\n", check_id, server_id, value, type_s);

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
        //printf("malloc data(%p)\n", data);
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
        char tmp_str[100];

        printf("parseCommand: dump|%s|%s\n", check_id, server_id);

        valueList_t *vl = getValueList(server_index, server_id, check_id, now, 0, opts, 0);
        while (vl != NULL) {
            //printf("dumping %s, %s, %d\n", server_id, check_id, vl->step);
            double data[vl->size];
            getValueListData(vl, data);
            sprintf(tmp_str, "%s|", check_id);
            strcat(output_buf, tmp_str);
            sprintf(tmp_str, "%s|", server_id);
            strcat(output_buf, tmp_str);
            sprintf(tmp_str, "%d|", vl->first);
            strcat(output_buf, tmp_str);
            sprintf(tmp_str, "%d|", vl->size);
            strcat(output_buf, tmp_str);
            sprintf(tmp_str, "%d|", vl->step);
            strcat(output_buf, tmp_str);

            for (i=0; i<vl->size; i++) {
                sprintf(tmp_str,"%f|",data[i]);
                strcat(output_buf,tmp_str); 
            }

            vl = vl->next;
            if (vl != NULL) {
                makeValueListCurrent(vl, now);
            }
            output_buf[strlen(output_buf) -1] = '\n';
        }
        strcat(output_buf,"\n");
    } else if (strcmp(command, "deleteserver") == 0) {
        char *server_id = strtok(NULL, "|\n\r");
        printf("parseCommand: delete|%s\n", server_id);
        deleteServerByName(server_index, server_id);
    } else if (strcmp(command, "get") == 0 || strcmp(command, "avg") == 0) {
        char *check_id = strtok(NULL, "|\n\r");
        char *range_s = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");
        char tmp_str[50];
        char *tmp_output_buf = calloc(BUFLEN, sizeof(char));
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
            printf("pylon.parseCommand.%s:No data.\n",command);
            strcpy(output_buf, "0|0|0\n");
        }
        free(tmp_output_buf);
        stats->gets++;
    } else if (strcmp(command, "reset") == 0) {
        server_t *server = server_index->next;
        printf("deleting all data\n");

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
        int size = serverIndexSize(server_index);
        int overflow_buffer_count = 0;
        overflow_buffer_t *ob = command_overflow_buffers->next;

        while (ob != NULL) {
            overflow_buffer_count++;
            ob = ob->next;
        }
        sprintf(tmp_str, "servers=%d checks=%d size=%d uptime=%d connections=%d commands=%d adds=%d gets=%d overflow_buffer_count=%d\n", server_count, check_count, size, (now - stats->start_time), stats->connections, stats->commands, stats->adds, stats->gets, overflow_buffer_count);
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
                //printf("pylon.parseCommand.%s:pre free(tmp_str)\n", command);
                free(tmp_str);
                //printf("pylon.parseCommand.%s:post free(tmp_str)\n", command);
            }
            server_id = strtok(NULL, "|\n\r");
            //printf("pylon.parseCommand.%s:server_id=%s\n",command,server_id);
        }
        //printf("pylon.parseCommand.%s:strlen(output_buf)=%d\n", command, strlen(output_buf));
        output_buf[strlen(output_buf) - 1] = 0;
        strcat(output_buf,"\n");
    } else if (strcmp(command, "servers") == 0) {
        char *tmp_str;

        printf("parseCommand: servers\n");
        tmp_str = getServerList(server_index);
        if (tmp_str != NULL) {
            strcpy(output_buf, tmp_str);
            free(tmp_str);
        }
        strcat(output_buf, "\n");
    } else {
        printf("parseCommand: unknown command|%s\n", command);
        strcpy(output_buf, "unknown command\n");
    }

    // Blank line signals the end of output.
    strcat(output_buf, "\n");
    
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
        err(1, "malloc failed");
    }

    len = read(fd, buf, BUFLEN);
    if (len == 0) {
        printf("Client disconnected.\n");
        close(fd);
        event_del(&client->ev_read);
        free(client);
        free(buf);
        return;
    } else if (len < 0) {
        printf("Socket failure, disconnecting client: %s",
        strerror(errno));
        close(fd);
        event_del(&client->ev_read);
        free(client);
        free(buf);
        return;
    }

    char *output_buf = parseCommand(buf, len, client->client_s_addr);

    if (output_buf != NULL && strlen(output_buf) > 0) {
        bufferq = malloc(sizeof(struct bufferq));
        if (bufferq == NULL) {
            err(1, "malloc failed");
        }
        bufferq->buf = output_buf;
        bufferq->len = strlen(output_buf);
        bufferq->offset = 0;
        TAILQ_INSERT_TAIL(&client->writeq, bufferq, entries);

        event_add(&client->ev_write, NULL);
    } else if (output_buf != NULL) {
        free(output_buf);
    }

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
    free(bufferq->buf);
    free(bufferq);
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
    if (client == NULL) err(1, "malloc failed");

    event_set(&client->ev_read, client_fd, EV_READ|EV_PERSIST, on_read, client);

    event_add(&client->ev_read, NULL);

    event_set(&client->ev_write, client_fd, EV_WRITE, on_write, client);

    TAILQ_INIT(&client->writeq);

    printf("Accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));
    client->client_s_addr = client_addr.sin_addr.s_addr;
}

void usage(void) {
    printf("usage: pylon\n");
    printf("-h            print this message and exit\n"
           "-d            run as a daemon\n"
           "-P <file>     save PID in <file>, only used with -d option\n"
           "              default: /var/run/pylon.pid\n"
           "-L <file>     log to <file>\n"
           "              default: /var/log/pylon.log\n"
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
    opts->max_buckets = MAX_BUCKETS;
    opts->bucket_size = BUCKET_SIZE;
    opts->buckets = malloc(sizeof(int) * opts->max_buckets);
    opts->bucket_count = 4;
    opts->cleanup =  86400;
    opts->buckets[0] = 300;
    opts->buckets[1] = 1800;
    opts->buckets[2] = 7200;
    opts->buckets[3] = 86400;

    char *cvalue = NULL;
    bool do_daemonize = false;
    char *pid_file = "/var/run/pylon.pid";
    char *log_file = "/var/log/pylon.log";

    struct event ev_accept;

    while ((c = getopt(argc, argv, "hdP:s:c:L:")) != -1) {
        switch (c) {
            case 'h':
                usage();
                exit(EXIT_SUCCESS);
            case 'd':
                do_daemonize = true;
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
            case 'c':
                cleanup = atoi(optarg);
                opts->cleanup = cleanup;
                break;
            case 'L':
                log_file = optarg;
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
    stats->commands = 0;
    stats->gets = 0;
    stats->adds = 0;
    stats->start_time = time(NULL);

    command_overflow_buffers = malloc(sizeof(overflow_buffer_t));
    command_overflow_buffers->next = NULL;
    command_overflow_buffers->prev = NULL;

    event_init();

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

    event_set(&ev_accept, listen_fd, EV_READ|EV_PERSIST, on_accept, NULL);
    event_add(&ev_accept, NULL);

    event_dispatch();

    if (do_daemonize) remove_pidfile(pid_file);

    return 0;
}
