#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <fcntl.h>
#include <event.h>
#include "pylon.h"

char* parseCommand(char *buf, time_t now, server_t *server_index, vlopts_t *opts, stats_t *stats, dump_config_t *dump_config) {
    int i;
    char *tmp = NULL;
    u_char *output_buf;
    char *command;
    now = time(NULL);
    int len = strlen(buf);

    outlog(7, "pylon.parseCommand: start\n");

    stats->commands++;

    output_buf = calloc(BUFLEN, sizeof(u_char));
    if (output_buf == NULL) {
        outlog(1, "pylon.parseCommand: malloc output_buf FAILED\n");
        fflush(stdout);
        exit(-1);
    }

    outlog(10, "pylon.parseCommand: malloc output_buf %p\n", output_buf);

    if (tmp == NULL) {
        tmp = calloc((len+1), sizeof(u_char));
        if (tmp == NULL) {
            outlog(1, "pylon.parseCommand: malloc tmp FAILED\n");
            fflush(stdout);
            exit(-1);
        }
        outlog(10, "pylon.parseCommand: malloc tmp %p\n", tmp);
        strcpy(tmp, buf);
    }

    command = strtok(tmp, "|\n\r");
    if (strcmp(command, "add") == 0) {
        char *graph_id = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");
        char *value = strtok(NULL, "|\n\r");
        char *type_s = strtok(NULL, "|\n\r");
        valueList_t *vl = NULL;
        int type = 0;

        outlog(5, "pylon.parseCommand: add|%s|%s|%s|%s\n", graph_id, server_id, value, type_s);

        if (server_id != NULL && value != NULL && strcmp(server_id,"EOF") !=0 && strcmp(value,"EOF") != 0) {
            if (strcmp(type_s, "counter") == 0) {
                type = 1;
            }

            vl = getValueList(server_index, server_id, graph_id, now, 0, 1, opts->bucket_count, opts->bucket_size, opts->buckets);
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
    } else if (strcmp(command, "cleanup") == 0) {
        char *cleanup_s = strtok(NULL, "|\n\r");

        outlog(5, "pylon.parseCommand: cleanup|%s\n", cleanup_s);
        if (cleanup_s == NULL || strcmp(cleanup_s, "EOF") == 0) {
            outlog(5, "pylon.parseCommand: INVALID\n");
            strcpy(output_buf, "INVALID");
            return;
        }

        int cleanup = atoi(cleanup_s);
        if (cleanup > 0) 
            opts->cleanup = cleanup;
        else if (cleanup == 0) {
            if (cleanupServerIndex(server_index, now, opts->cleanup) > 0) {
                dump_config->abort = 1;
            }
        }

        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "deleteserver") == 0) {
        char *server_id = strtok(NULL, "|\n\r");
        outlog(5, "pylon.parseCommand: deleteserver|%s\n", server_id);
        deleteServerByName(server_index, server_id);
        dump_config->abort = 1;
    } else if (strcmp(command, "dump") == 0) {
        char *graph_id = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");

        outlog(5, "pylon.parseCommand: dump|%s|%s\n", graph_id, server_id);

        if (server_id == NULL || strcmp(server_id, "EOF") == 0 || graph_id == NULL || strcmp(graph_id,"EOF") == 0) {
            strcpy(output_buf, "INVALID\n");
            return;
        }

        valueList_t *vl = getValueList(server_index, server_id, graph_id, now, 0, 0, opts->bucket_count, opts->bucket_size, opts->buckets);
        while (vl != NULL) {
            dumpValueList(graph_id, server_id, vl, now, output_buf);
            vl = vl->next;
        }
    } else if (strcmp(command, "dumpoff") == 0) {
        outlog(5, "pylon.parseCommand: dumpoff\n");
        dump_config->enabled = 0;
        dump_config->abort = 1;
        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "dumpon") == 0) {
        outlog(5, "pylon.parseCommand: dumpon\n");
        if (dump_config->dump_file != NULL)
            dump_config->enabled = 1;
        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "get") == 0 || strcmp(command, "avg") == 0) {
        char *graph_id = strtok(NULL, "|\n\r");
        char *range_s = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");
        outlog(5, "pylon.parseCommand: get|%s|%s|%s\n", graph_id, range_s, server_id);
        if (graph_id == NULL || strcmp(graph_id, "EOF") == 0 ||
            range_s == NULL || strcmp(range_s, "EOF") == 0 ||
            server_id == NULL || strcmp(server_id, "EOF") == 0) {
            strcpy(output_buf, "INVALID\n");
            return;
        }
        char *tmp_output_buf = calloc(BUFLEN, sizeof(u_char));
        if (tmp_output_buf == NULL) {
            outlog(1, "pylon.parseCommand: malloc tmp_output_buf FAILED\n");
            exit(-1);
        }
        outlog(10, "pylon.parseCommand: malloc tmp_output_buf %p\n", tmp_output_buf);
        time_t first = 0;
        int range = 0;
        int size = 0;
        int server_count = 0;

        range = atoi(range_s);

        valueList_t *vl = getValueList(server_index, server_id, graph_id, now, range, 0, opts->bucket_count, opts->bucket_size, opts->buckets);

        if (vl == NULL) {
            server_id = strtok(NULL, "|\n\r");
            while (vl==NULL && strcmp(server_id,"EOF") != 0 && server_id != NULL) {
                vl = getValueList(server_index, server_id, graph_id, now, range, 0, opts->bucket_count, opts->bucket_size, opts->buckets);
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
                char tmp_str[50];
                // Walk through the list of requested servers and build our data list.
                while (strcmp(server_id,"EOF") != 0 && server_id != NULL) {
                    valueList_t *vl2 = getValueList(server_index, server_id, graph_id, now, range, 0, opts->bucket_count, opts->bucket_size, opts->buckets);
                    if (vl2 != NULL && vl2->size == vl->size && vl2->step == vl->step) {
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
            strcpy(output_buf, "0|0|0\n");
        }
        outlog(10, "pylon.parseCommand: free tmp_output_buf %p\n", tmp_output_buf);
        free(tmp_output_buf);
        stats->gets++;
    } else if (strcmp(command, "graphs") == 0 || strcmp(command, "checks") == 0) {
        char *server_id = strtok(NULL, "|\n\r");
        char tmp2[1024];

        outlog(5, "pylon.parseCommand: graphs|%s\n", server_id);

        while (server_id != NULL && strcmp(server_id,"EOF") != 0) {
            int i;
            char *tmp_str = getGraphList(server_index, server_id);
            if (tmp_str != NULL) {
                int strpos = 0;
                for (i=0; i <= strlen(tmp_str); i++) {
                    if (*(tmp_str+i) == '|' || i == strlen(tmp_str)) {
                        int newlen = i - strpos;
                        strncpy(tmp2,tmp_str + strpos,newlen);
                        tmp2[newlen] = '|';
                        tmp2[newlen+1] = 0;
                        if (strstr(output_buf, tmp2) == NULL) {
                            strcat(output_buf, tmp2);
                        }
                        i++;
                        strpos = i;

                        if (strlen(output_buf) > ((BUFLEN) * 0.95)) {
                            break;
                        }
                    }
                }
                outlog(10, "pylon.parseCommand: free tmp_str %p\n", tmp_str);
                free(tmp_str);
                if (strlen(output_buf) > ((BUFLEN) * 0.95)) {
                    outlog(2, "pylon.parseCommand: graph list too large\n");
                    break;
                }
            }
            server_id = strtok(NULL, "|\n\r");
        }
        if (strlen(output_buf) > 0) {
            output_buf[strlen(output_buf) - 1] = 0;
        }
        strcat(output_buf,"\n");
    } else if (strcmp(command, "load") == 0) {
        char *graph_id = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");
        char *first_s = strtok(NULL, "|\n\r");
        char *size_s = strtok(NULL, "|\n\r");
        char *step_s = strtok(NULL, "|\n\r");

        if (graph_id == NULL || strcmp(graph_id, "EOF") == 0 || 
            server_id == NULL || strcmp(server_id, "EOF") == 0 || 
            first_s == NULL || strcmp(first_s, "EOF") == 0 || 
            size_s == NULL || strcmp(size_s, "EOF") == 0 || 
            step_s == NULL || strcmp(step_s, "EOF") == 0) {
            strcpy(output_buf, "INVALID");
            return;
        }

        time_t first = atoi(first_s);
        int size = atoi(size_s);
        int step = atoi(step_s);

        outlog(5, "pylon.parseCommand: load|%s|%s|%d|%d|%d\n", graph_id, server_id, first, size, step);
        double *data = malloc(size*sizeof(double));
        if (data == NULL) {
            outlog(1, "pylon.parseCommand: malloc data FAILED\n");
            exit(-1);
        }
        outlog(10, "pylon.parseCommand: malloc data %p\n", data);
        char *d = strtok(NULL, "|\n\r");
        for (i=0;i<size;i++) {
            if (d != NULL) {
                data[i] = atof(d);
                d = strtok(NULL, "|\n\r");
            } else {
                data[i] = 0.0/0.0;
            }
        }

        valueList_t *vl = loadData(server_index, server_id, graph_id, first, size, step, data, now, opts->bucket_count, opts->buckets);
        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "loglevel") == 0) {
        char *loglevel_s = strtok(NULL, "|\n\r");

        outlog(5, "pylon.parseCommand: loglevel|%s\n", loglevel_s);
        if (loglevel_s == NULL || strcmp(loglevel_s, "EOF") == 0) {
            outlog(5, "pylon.parseCommand: INVALID\n");
            strcpy(output_buf, "INVALID");
            return;
        }

        opts->loglevel = atoi(loglevel_s);
        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "options") == 0) {
        outlog(5, "pylon.parseCommand: options\n");
        char tmp_str[512];

        sprintf(tmp_str, "cleanup=%d max_buckets=%d bucket_size=%d bucket_count=%d loglevel=%d dump_interval=%d\n", opts->cleanup, opts->max_buckets, opts->bucket_size, opts->bucket_count, opts->loglevel, dump_config->dump_interval);

        strcpy(output_buf, tmp_str);
    } else if (strcmp(command, "placeholder") == 0) {
        outlog(5, "pylon.parseCommand: placeholder\n");
        char tmp_str[255];
        sprintf(tmp_str, "PLACEHOLDER uptime=%d connections=%d commands=%d\n", (now - stats->start_time), stats->connections, stats->commands);

        strcpy(output_buf, tmp_str);
    } else if (strcmp(command, "reset") == 0) {
        outlog(5, "pylon.parseCommand: reset\n");
        dump_config->abort = 1;
        server_t *server = server_index->next;

        while(server != NULL) {
            server_index->next = server->next;
            deleteServer(server);
            server = server_index->next;
        }
        server_index->next = NULL;
        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "servers") == 0) {
        outlog(5, "pylon.parseCommand:servers\n");
        char *tmp_str;

        tmp_str = getServerList(server_index);
        if (tmp_str != NULL) {
            strcpy(output_buf, tmp_str);
            outlog(10, "pylon.parseCommand: free tmp_str-2 %p\n", tmp_str);
            free(tmp_str);
        }
        strcat(output_buf, "\n");
    } else if (strcmp(command, "status") == 0) {
        outlog(5, "pylon.parseCommand: status\n");
        char tmp_str[512];
        int server_count = getServerCount(server_index);
        int graph_count = getGraphCount(server_index);
        long int size = serverIndexSize(server_index);
        sprintf(tmp_str, "servers=%d graphs=%d size=%ld uptime=%d connections=%d commands=%d adds=%d gets=%d dumps=%d\n", server_count, graph_count, size, (now - stats->start_time), stats->connections, stats->commands, stats->adds, stats->gets, stats->dumps);

        strcpy(output_buf, tmp_str);
    } else if (strcmp(command, "version") == 0) {
        char *tmp_str;

        outlog(5, "parseCommand:version\n");
        strcpy(output_buf, VERSION);
        strcat(output_buf, "\n");
    } else {
        strcpy(output_buf, "unknown command\n");
    }

    // Blank line signals the end of output.
    strcat(output_buf, "\n");
    
    outlog(10, "pylon.parseCommand: free tmp %p\n", tmp);
    free(tmp);
    outlog(5, output_buf);
    return output_buf;
}

void dump_data(dump_config_t *dump_config) {
    if (dump_config->completed > (time(NULL) - dump_config->dump_interval)) 
        return;

    if (dump_config->abort == 1) {
        outlog(10, "pylon.dump_data: aborting dump\n");
        close(dump_config->dump_fd);
        dump_config->dump_fd = 0;
        unlink(dump_config->dump_file_tmp);
        dump_config->completed = time(NULL);
        dump_config->abort = 0;
        dump_config->graph = NULL;
        dump_config->server = NULL;
        return;
    }
    if (dump_config->enabled == 0) 
        return;

    outlog(10, "pylon.dump_data: start\n");
    fflush(stdout);

    if (dump_config->loading == 1) {
        // Haven't implemented this yet, but I eventually want to move the data loading
        // to a staggered one-graph-at-a-time method, rather than loading everything
        // at once at startup.
    } else {
        // Advance the current dump pointer.
        if (dump_config->graph != NULL) {
            // We're sitting on a valid graph, so move to the next one.
            outlog(10, "pylon.dump_data: increment graph pointer\n");
            fflush(stdout);
            dump_config->graph = dump_config->graph->next;
        }

        if (dump_config->graph == NULL) {
            // Graph is null, so we're either before the first one or after
            // the last one.
            outlog(10, "pylon.dump_data: graph pointer is null\n");
            fflush(stdout);
            if (dump_config->server == NULL) {
                // Server is null, so this must be our first time through.
                // Set the server to the first one on the list.
                outlog(10, "pylon.dump_data: first server\n");
                fflush(stdout);
                dump_config->server = dump_config->server_index->next;
            } else {
                // Otherwise, move us to the next server.
                outlog(10, "pylon.dump_data: increment server pointer\n");
                fflush(stdout);
                dump_config->server = dump_config->server->next;
            }

            if (dump_config->server != NULL) {
                // Assuming we have a server, start with the first graph.
                outlog(10, "pylon.dump_data: server is not null, start with first graph\n");
                fflush(stdout);
                dump_config->graph = dump_config->server->graph->next;
            }
        } 

        // Process the current pointer.
        outlog(10, "pylon.dump_data: dump_config->dump_fd=%d\n", dump_config->dump_fd);
        if (dump_config->graph == NULL && dump_config->server == NULL && dump_config->dump_fd > 0) {
            // Reached the end of the set.  Close the temp file, and swap it to live.
            outlog(10, "pylon.dump_data: reached the end of the set\n");
            fflush(stdout);
            outlog(10, "pylon.dump_data: closing %s\n", dump_config->dump_file_tmp);
            fflush(stdout);
            close(dump_config->dump_fd);
            dump_config->dump_fd = 0;
            outlog(10, "pylon.dump_data: renaming %s to %s\n", dump_config->dump_file_tmp, dump_config->dump_file);
            fflush(stdout);
            rename(dump_config->dump_file_tmp, dump_config->dump_file);
            dump_config->completed = time(NULL);
            return;
        } else if (dump_config->graph != NULL && dump_config->server != NULL) {
            outlog(10, "pylon.dump_data: sitting on a valid entry\n");
            fflush(stdout);
            if (dump_config->dump_fd < 1) {
                // Temp file hasn't been created.  Do so.
                outlog(10, "pylon.dump_data: temp file hasn't been created\n");
                fflush(stdout);
                outlog(10, "pylon.dump_data: removing %s\n", dump_config->dump_file_tmp);
                fflush(stdout);
                unlink(dump_config->dump_file_tmp);
                outlog(10, "pylon.dump_data: opening %s\n", dump_config->dump_file_tmp);
                fflush(stdout);
                dump_config->dump_fd = open(dump_config->dump_file_tmp, O_WRONLY|O_CREAT, 0755);
                if (dump_config->dump_fd < 1) {
                    outlog(10, "pylon.dump_data: can't open %s for writing (dump_fd=%d)\n", dump_config->dump_file_tmp, dump_config->dump_fd);
                    fflush(stdout);
                    outlog(10, "pylon.dump_data: trying again\n");
                    fflush(stdout);
                    dump_config->dump_fd = open(dump_config->dump_file_tmp, O_WRONLY|O_CREAT, 0755);
                    if (dump_config->dump_fd < 1) {
                        outlog(10, "pylon.dump_data: still can't open %s for writing (dump_fd=%d)\n", dump_config->dump_file_tmp, dump_config->dump_fd);
                        fflush(stdout);
                        exit(-1);
                    }
                }
            }
            // Dump the current graph to the temp file.
            dump_config->graphdump[0] = 0;

            outlog(10, "pylon.dump_data: collecting %s/%s. buffer size: %d\n", dump_config->server->name, dump_config->graph->name, (BUFLEN*sizeof(u_char)));
            fflush(stdout);
            dumpGraph(dump_config->graph, dump_config->server->name, dump_config->now, dump_config->graphdump);
            fflush(stdout);
            outlog(10, "pylon.dump_data: writing to dump file %s\n", dump_config->dump_file_tmp);
            fflush(stdout);
            int ret;
            ret = write(dump_config->dump_fd, dump_config->graphdump, strlen(dump_config->graphdump));
            if (ret < 0) {
                outlog(10, "pylon.dump_data: error writing to %s: %d\n", dump_config->dump_file_tmp, ret);
                fflush(stdout);
                exit(-1);
            } else if ( ret < strlen(dump_config->graphdump)) {
                outlog(10, "pylon.dump_data: only wrote %d bytes to %s, should have written %d\n", ret, dump_config->dump_file_tmp, strlen(dump_config->graphdump));
                fflush(stdout);
                exit(-1);
            }
            fflush(stdout);
            outlog(10, "pylon.dump_data: dump_config->dump_fd=%d\n", dump_config->dump_fd);
        } else {
            outlog(10, "pylon.dump_data: nothing to do\n");
            fflush(stdout);
            dump_config->completed = time(NULL);
        }
        outlog(10, "pylon.dump_data: finished dump run\n");
        fflush(stdout);
    }
}

void load_data(dump_config_t *dump_config, time_t now, vlopts_t *opts) {
    int i;
    dump_config->dump_file_tmp = malloc(sizeof(u_char) * (strlen(dump_config->dump_file) + 5));
    dump_config->graphdump = malloc(BUFLEN*sizeof(u_char));
    if (dump_config->graphdump == NULL) {
        outlog(1, "pylon.load_data: malloc dump_config->graphdump FAILED\n");
        exit(-1);
    }
    outlog(10, "pylon.load_data: malloc dump_config->graphdump %p\n", dump_config->graphdump);
    strcpy(dump_config->dump_file_tmp, dump_config->dump_file);
    strcat(dump_config->dump_file_tmp, ".tmp");

    // Load data from existing dump file
    int dump_fd = open(dump_config->dump_file, O_RDONLY);
    if ( dump_fd > 0) {
        dump_config->loading = 1;
        time_t load_start = time(NULL);
        int load_count = 0;
        outlog(10, "loading data from %s: %d\n", dump_config->dump_file, load_start);
        char read_buf[1024];
        char *read_buf_tmp;
        char *output_buf = malloc(BUFLEN*sizeof(u_char));
        if (output_buf == NULL) {
            outlog(1, "pylon.load_data: malloc output_buf FAILED\n");
            exit(-1);
        }
        outlog(10, "pylon.load_data: malloc output_buf %p\n", output_buf);
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

                char *graph_id = strtok(output_buf, "|\n\r");
                char *server_id = strtok(NULL, "|\n\r");
                char *first_s = strtok(NULL, "|\n\r");
                char *size_s = strtok(NULL, "|\n\r");
                char *step_s = strtok(NULL, "|\n\r");
                time_t first = atoi(first_s);
                int size = atoi(size_s);
                int step = atoi(step_s);

                double *data = malloc(size*sizeof(double));
                if (data == NULL) {
                    outlog(1, "pylon.load_data: malloc data FAILED\n");
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

                if (server_id != NULL && graph_id != NULL && size > 0 && step > 0) {
                    loadData(dump_config->server_index, server_id, graph_id, first, size, step, data, now, opts->bucket_count, opts->buckets);
                    load_count++;
                } else {
                    free(data);
                }

                output_buf[0] = 0;
                read_buf_tmp = pos + 1;
            }
            strcat(output_buf, read_buf_tmp);
        }
        time_t load_end = time(NULL);
        outlog(10, "pylon.load_data: free output_buf %p\n", output_buf);
        free(output_buf);
        outlog(3, "pylon.load_data: finished loading data from %s: %d\n", dump_config->dump_file, load_end);
        if (load_end > load_start) {
            outlog(3, "pylon.load_data: load time: %d seconds, %d graphs, %.2f records/sec\n", (load_end - load_start), (load_count/4), ((load_count/4)/(load_end-load_start)));
        }
        dump_config->loading = 0;
    }
    close(dump_fd);
}

