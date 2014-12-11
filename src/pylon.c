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

    stats->commands++;

    output_buf = calloc(BUFLEN, sizeof(u_char));
    if (output_buf == NULL) {
        printf("malloc pylon parseCommand output_buf FAILED\n");
        fflush(stdout);
        exit(-1);
    }

    printf("malloc pylon parseCommand output_buf %p\n", output_buf);

    if (tmp == NULL) {
        tmp = calloc((len+1), sizeof(u_char));
        if (tmp == NULL) {
            printf("malloc pylon parseCommand tmp-2 FAILED\n");
            fflush(stdout);
            exit(-1);
        }
        printf("malloc pylon parseCommand tmp-2 %p\n", tmp);
        strcpy(tmp, buf);
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

            vl = getValueList(server_index, server_id, check_id, now, 0, 1, opts->bucket_count, opts->bucket_size, opts->buckets);
            if (vl != NULL) {
                addValue(vl, atof(value), now, type);
                strcpy(output_buf, "OK\n");
            } else { 
                printf("FAIL\n");
                strcpy(output_buf, "FAIL\n");
            }
        } else { 
            printf("INVALID\n");
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

        valueList_t *vl = loadData(server_index, server_id, check_id, first, size, step, data, now, opts->bucket_count, opts->buckets);
        strcpy(output_buf, "OK\n");
    } else if (strcmp(command, "dump") == 0) {
        char *check_id = strtok(NULL, "|\n\r");
        char *server_id = strtok(NULL, "|\n\r");

        printf("parseCommand: dump|%s|%s\n", check_id, server_id);

        if (server_id != NULL && check_id != NULL) {
            valueList_t *vl = getValueList(server_index, server_id, check_id, now, 0, 0, opts->bucket_count, opts->bucket_size, opts->buckets);
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
        char *tmp_output_buf = calloc(BUFLEN, sizeof(u_char));
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

        valueList_t *vl = getValueList(server_index, server_id, check_id, now, range, 0, opts->bucket_count, opts->bucket_size, opts->buckets);

        if (vl == NULL) {
            server_id = strtok(NULL, "|\n\r");
            while (vl==NULL && strcmp(server_id,"EOF") != 0 && server_id != NULL) {
                //printf("pylon.parseCommand.%s:looking for first server that has a valuelist for %s\n",command, check_id);
                vl = getValueList(server_index, server_id, check_id, now, range, 0, opts->bucket_count, opts->bucket_size, opts->buckets);
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
                    //printf("pylon.parseCommand.%s:looking for more servers that have a valuelist for %s\n",command, check_id);
                    valueList_t *vl2 = getValueList(server_index, server_id, check_id, now, range, 0, opts->bucket_count, opts->bucket_size, opts->buckets);
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
        dump_config->abort = 1;
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
        //int overflow_buffer_count = 0;
        //overflow_buffer_t *ob = command_overflow_buffers->next;

        //while (ob != NULL) {
        //    overflow_buffer_count++;
        //    ob = ob->next;
        //}
        //sprintf(tmp_str, "servers=%d checks=%d size=%ld uptime=%d connections=%d commands=%d adds=%d gets=%d overflow_buffer_count=%d dumps=%d\n", server_count, check_count, size, (now - stats->start_time), stats->connections, stats->commands, stats->adds, stats->gets, overflow_buffer_count, stats->dumps);
        sprintf(tmp_str, "servers=%d checks=%d size=%ld uptime=%d connections=%d commands=%d adds=%d gets=%d dumps=%d\n", server_count, check_count, size, (now - stats->start_time), stats->connections, stats->commands, stats->adds, stats->gets, stats->dumps);
        printf("%s", tmp_str);

        strcpy(output_buf, tmp_str);

    } else if (strcmp(command, "placeholder") == 0) {
        printf("parseCommand: placeholder\n");
        char tmp_str[255];
        sprintf(tmp_str, "PLACEHOLDER uptime=%d connections=%d commands=%d\n", (now - stats->start_time), stats->connections, stats->commands);
        printf("%s", tmp_str);

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

void dump_data(dump_config_t *dump_config) {
    printf("pylon dump_data\n");
    fflush(stdout);

    if (dump_config->completed > (time(NULL) - dump_config->dump_interval)) 
        return;

    if (dump_config->abort == 1) {
        printf("aborting dump\n");
        close(dump_config->dump_fd);
        dump_config->dump_fd = 0;
        unlink(dump_config->dump_file_tmp);
        dump_config->completed = time(NULL);
        dump_config->abort = 0;
        return;
    }

    if (dump_config->loading == 1) {
        // Haven't implemented this yet, but I eventually want to move the data loading
        // to a staggered one-check-at-a-time method, rather than loading everything
        // at once at startup.
    } else {
        // Advance the current dump pointer.
        if (dump_config->check != NULL) {
            // We're sitting on a valid check, so move to the next one.
            printf("dump_data.one\n");
            dump_config->check = dump_config->check->next;
        }

        if (dump_config->check == NULL) {
            // Check is null, so we're either before the first one or after
            // the last one.
            printf("dump_data.two\n");
            if (dump_config->server == NULL) {
                // Server is null, so this must be our first time through.
                // Set the server to the first one on the list.
                printf("dump_data.three\n");
                dump_config->server = dump_config->server_index->next;
                printf("dump_data.eight\n");
            } else {
                // Otherwise, move us to the next server.
                printf("dump_data.four\n");
                dump_config->server = dump_config->server->next;
            }
            // Assuming we have a server, start with the first check.
            printf("dump_data.nine\n");
            if (dump_config->server != NULL) {
                printf("dump_data.five\n");
                dump_config->check = dump_config->server->check->next;
            }
        } 

        // Process the current pointer.
        if (dump_config->check == NULL && dump_config->server == NULL && dump_config->dump_fd > 0) {
            // Reached the end of the set.  Close the temp file, and swap it to live.
            printf("closing %s\n", dump_config->dump_file_tmp);
            fflush(stdout);
            close(dump_config->dump_fd);
            dump_config->dump_fd = 0;
            printf("renaming %s to %s\n", dump_config->dump_file_tmp, dump_config->dump_file);
            fflush(stdout);
            rename(dump_config->dump_file_tmp, dump_config->dump_file);
            dump_config->completed = time(NULL);
            return;
        } else if (dump_config->check != NULL && dump_config->server != NULL) {
            // Sitting on a valid entry.  
            printf("dump_data.six\n");
            if (dump_config->dump_fd < 1) {
                // Temp file hasn't been created.  Do so.
                printf("removing %s\n", dump_config->dump_file_tmp);
                fflush(stdout);
                unlink(dump_config->dump_file_tmp);
                printf("opening %s\n", dump_config->dump_file_tmp);
                dump_config->dump_fd = open(dump_config->dump_file_tmp, O_WRONLY|O_CREAT, 0755);
                fflush(stdout);
                if (dump_config->dump_fd < 0) {
                    printf("pylon.dump_data:Can't open %s for writing\n", dump_config->dump_file_tmp);
                    fflush(stdout);
                    exit(-1);
                }
            }
            // Dump the current check to the temp file.
            dump_config->checkdump[0] = 0;

            printf("collecting %s/%s. buffer size: %d\n", dump_config->server->name, dump_config->check->name, (BUFLEN*sizeof(u_char)));
            fflush(stdout);
            dumpCheck(dump_config->check, dump_config->server->name, dump_config->now, dump_config->checkdump);
            fflush(stdout);
            printf("writing to dump file %s\n", dump_config->dump_file_tmp);
            fflush(stdout);
            int ret;
            ret = write(dump_config->dump_fd, dump_config->checkdump, strlen(dump_config->checkdump));
            if (ret < 0) {
                printf("error writing to %s: %d\n", dump_config->dump_file_tmp, ret);
                fflush(stdout);
                exit(-1);
            } else if ( ret < strlen(dump_config->checkdump)) {
                printf("only wrote %d bytes to %s, should have written %d\n", ret, dump_config->dump_file_tmp, strlen(dump_config->checkdump));
                fflush(stdout);
                exit(-1);
            }
            fflush(stdout);
        } else {
            printf("dump_data: nothing to do\n");
            fflush(stdout);
        }
        printf("dump_data.seven\n");
        fflush(stdout);
    }
}

void load_data(dump_config_t *dump_config, time_t now, vlopts_t *opts) {
    int i;
    dump_config->dump_file_tmp = malloc(sizeof(u_char) * (strlen(dump_config->dump_file) + 5));
    dump_config->checkdump = malloc(BUFLEN*sizeof(u_char));
    if (dump_config->checkdump == NULL) {
        printf("malloc pylon main dump_config->checkdump FAILED\n");
        fflush(stdout);
        exit(-1);
    }
    strcpy(dump_config->dump_file_tmp, dump_config->dump_file);
    strcat(dump_config->dump_file_tmp, ".tmp");

    // Load data from existing dump file
    int dump_fd = open(dump_config->dump_file, O_RDONLY);
    if ( dump_fd > 0) {
        dump_config->loading = 1;
        time_t load_start = time(NULL);
        int load_count = 0;
        printf("loading data from %s: %d\n", dump_config->dump_file, load_start);
        char read_buf[1024];
        char *read_buf_tmp;
        char *output_buf = malloc(BUFLEN*sizeof(u_char));
        if (output_buf == NULL) {
            printf("malloc pylon main output_buf FAILED\n");
            fflush(stdout);
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

                double *data = malloc(size*sizeof(double));
                if (data == NULL) {
                    printf("malloc pylon main data FAILED\n");
                    fflush(stdout);
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
                    loadData(dump_config->server_index, server_id, check_id, first, size, step, data, now, opts->bucket_count, opts->buckets);
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
        dump_config->loading = 0;
    }
    close(dump_fd);

}

