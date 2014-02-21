#include <stdio.h> 
#include <stdlib.h>
#include <string.h>
#include "servercheck.h"


void cleanupServerIndex(server_t *server_index, time_t now, int cleanup) {
    server_t *last_server = server_index;
    server_t *server = server_index->next;
    time_t cutoff = now - cleanup;

    printf("serverchec.cleanupServerIndex: now=%d, cleanup=%d, cutoff=%d\n", now, cleanup, cutoff);
    while(server != NULL) {
        check_t *last_check = server->check;
        check_t *check = server->check->next;
        while(check != NULL) {
            valueList_t *vl = check->vl->next;
            if (vl->update_time < cutoff) {
                last_check->next = check->next;
                deleteCheck(check);
                check = last_check->next;
            } else {
                check = check->next;
            }
        }

        if (server->check->next == NULL) {
            last_server->next = server->next;
            deleteServer(server);
            server = last_server->next;
        } else {
            server = server->next;
        }
    }
}

server_t *getServerByName(server_t *server_index, char *server_id, int force) {
    server_t *server = server_index->next;

    // Scan the list for the server;
    while(server != NULL) {
        if (strcmp(server->name, server_id) == 0) {
            return server;
        }
        server = server->next;
    }

    if (force > 0 && strlen(server_id) < 52) {
        //printf("servercheck.getServerByName:creating server_id=%s\n", server_id);
        server = malloc(sizeof(server_t));
        if (server == NULL) {
            printf("malloc servercheck getServerByName server FAILED\n");
            exit(-1);
        }
        server->name = malloc((strlen(server_id) + 1) * sizeof(char));
        if (server->name == NULL) {
            printf("malloc servercheck getServerByName server->name FAILED\n");
            exit(-1);
        }
        strcpy(server->name, server_id);
        server->check = malloc(sizeof(check_t));
        if (server->check == NULL) {
            printf("malloc servercheck getServerByName server->check FAILED\n");
            exit(-1);
        }
        server->check->next = NULL;

        server->next = server_index->next;
        server_index->next = server;
    }

    return server;
}

check_t *getServerCheckByName(server_t *server_index, char *server_id, char *check_id, int force) {
    server_t *server = getServerByName(server_index, server_id, force);
    if (server == NULL) {
        return NULL;
    }

    check_t *check = server->check->next;
    while(check != NULL) {
        if (strcmp(check->name, check_id) == 0) {
            return check;
        }
        check = check->next;
    }

    if (force > 0 && strlen(server_id) <=52) {
        printf("servercheck.getServerCheckByName:creating check_id=%s for server_id=%s\n", check_id, server_id);
        check = malloc(sizeof(check_t));
        if (check == NULL) {
            printf("malloc servercheck getSererCheckByName check FAILED\n");
            exit(-1);
        }
        check->name = malloc((strlen(check_id)+1) * sizeof(char));
        if (check->name == NULL) {
            printf("malloc servercheck getSererCheckByName check->name FAILED\n");
            exit(-1);
        }
        strcpy(check->name, check_id);
        check->vl = malloc(sizeof(valueList_t));
        if (check->vl == NULL) {
            printf("malloc servercheck getSererCheckByName check->vl FAILED\n");
            exit(-1);
        }
        check->vl->next = NULL;
        check->next = server->check->next;
        server->check->next = check;
    }

    return check;
}

int getServerCount(server_t *server_index) {
    int count = 0;
    server_t *server = server_index->next;

    while(server != NULL) {
        count++;
        server = server->next;
    }
    return count;
}

int getCheckCount(server_t *server_index) {
    int count = 0;
    server_t *server = server_index->next;

    while(server != NULL) {
        check_t *check = server->check->next;
        while(check != NULL) {
            count++;
            check = check->next;
        }
        server = server->next;
    }
    return count;
}

long int serverIndexSize(server_t *server_index) {
    long int size = sizeof(server_t);
    server_t *server = server_index->next;

    while(server != NULL) {
        size += sizeof(server_t);
        size += strlen(server->name);
        size += sizeof(check_t);
        check_t *check = server->check->next;
        while(check != NULL) {
            size += sizeof(check_t);
            size += strlen(check->name);
            valueList_t *vl = check->vl->next;
            while (vl != NULL) {
                size += (sizeof(double) * vl->size);
                vl = vl->next;
            }
            check = check->next;
        }
        server = server->next;
    }
    return size;
}

char *getServerList(server_t *server_index) {
    server_t *server = server_index->next;
    if (server == NULL) {
        return NULL;
    }

    int server_size = 0;
    while(server != NULL) {
        server_size += (strlen(server->name) + 1);
        server = server->next;
    }

    char *tmp_str = malloc((server_size + 1) * sizeof(char));
    if (tmp_str == NULL) {
        printf("malloc servercheck getServerList tmp_str FAILED\n");
        exit(-1);
    }
    printf("malloc servercheck getServerList tmp_str %p\n", tmp_str);
    tmp_str[0] = 0;

    server = server_index->next;
    while(server != NULL) {
        strcat(tmp_str, server->name);
        strcat(tmp_str, "|");
        server = server->next;
    }
    if (strlen(tmp_str) > 0) {
        tmp_str[strlen(tmp_str) - 1] = 0;
    }

    return tmp_str;
}

char *getCheckList(server_t *server_index, char *server_id) {
     server_t *server = getServerByName(server_index, server_id, 0);

    if (server == NULL) {
        return NULL;
    }
   
    check_t *check = server->check->next;
    if (check == NULL) {
        return NULL;
    }

    int check_size = 0;
    while(check != NULL) {
        check_size += (strlen(check->name) + 1);
        check = check->next;
    }

    if (check_size > 0) {
        //printf("servercheck.getCheckList:server_id=%s,check_size=%d\n", server_id, check_size);
        char *tmp_str = malloc(sizeof(char) * (check_size+1));
        if (tmp_str == NULL) {
            printf("malloc servercheck getServerCheckList tmp_str FAILED\n");
            exit(-1);
        }
        printf("malloc servercheck getServerCheckList tmp_str %p\n", tmp_str);
        tmp_str[0] = 0;
        check = server->check->next;
        while(check != NULL) {
            strcat(tmp_str, check->name);
            strcat(tmp_str, "|");
            check = check->next;
        }
        // Cut the trailing pipe.
        if (strlen(tmp_str) > 0) {
            tmp_str[strlen(tmp_str) - 1] = 0;
        }
        return tmp_str;
    }
    return NULL;
}

valueList_t *getValueList(server_t *server_index, char *server_id, char *check_id, time_t now, int range, vlopts_t *opts,int force) {
    check_t *check = getServerCheckByName(server_index, server_id, check_id, force);

    if (check == NULL) {
        return NULL;
    }

    valueList_t *vl = check->vl->next;
    if (vl == NULL && force > 0) {
        valueList_t *last_vl = check->vl;
        int i;
        for (i=0;i<opts->bucket_count;i++) {
            valueList_t *vl2 = newValueList(opts->bucket_size, opts->buckets[i], now);
            last_vl->next = vl2;
            last_vl = vl2;
        }
        vl = check->vl->next;
    } else if (vl != NULL) {
        makeValueListCurrent(vl, now);
    }

    if (range == 0) {
        return vl;
    }

    while (vl != NULL) {
        if  ((vl->first - vl->step) <= range || vl->next == NULL) {
            return vl;
        }

        vl = vl->next;
        makeValueListCurrent(vl, now);
    }
    return vl;
}

void deleteServer(server_t *server) {
    printf("servercheck.deleteServer: deleting %s\n", server->name);
    if (server->check->next != NULL) {
        check_t *check = server->check->next;
        while(check != NULL) {
            check_t *next = check->next;
            deleteCheck(check);
            check = next;
        }
    }
    free(server->check);
    free(server->name);
    free(server);
}

void deleteCheck(check_t *check) {
    printf("servercheck.deleteCheck: deleting %s\n", check->name);
    valueList_t *vl = check->vl->next;
    while (vl != NULL) {
        valueList_t *next = vl->next; 
        deleteValueList(vl);
        vl = next;
    }
    free(check->name);
    free(check->vl);
    free(check);
}

void deleteServerByName(server_t *server_index, char *server_id) {
    server_t *server = server_index->next;
    server_t *prev = server_index;

    // Scan the list for the server;
    while(server != NULL) {
        if (strcmp(server->name, server_id) == 0) {
            break;
        }
        prev = server;
        server = server->next;
    }

    if (server == NULL) {
        return;
    }

    check_t *check = server->check->next;

    while(check != NULL) {
        check_t *next = check->next;
        deleteCheck(check);
    }

    if (prev) {
        prev->next = server->next;
    }

    deleteServer(server);
}

server_t *newServerIndex() {
    server_t *server_index = malloc(sizeof(server_t));
    if (server_index == NULL) {
        printf("malloc servercheck newServerIndex server_index FAILED\n");
        exit(-1);
    }
    server_index->name = NULL;
    server_index->next = NULL;
    return server_index;
}

valueList_t *loadData(server_t *server_index, char *server_id, char *check_id, time_t first, int size, int step, double* data, time_t now, vlopts_t *opts) {
    // Get the first valuelist for this server/check.  If none exist, create a default.
    printf("servercheck.loadData: server_id=%s,check_id=%s,size=%d,step=%d\n",server_id, check_id, size, step);
    check_t *check = getServerCheckByName(server_index, server_id, check_id, 1);
    valueList_t *vl = check->vl->next;
    if (vl == NULL) {
        valueList_t *last_vl = check->vl;
        int i;
        for (i=0;i<opts->bucket_count;i++) {
            valueList_t *vl2 = newValueList(opts->bucket_size, opts->buckets[i], now);
            last_vl->next = vl2;
            last_vl = vl2;
        }
        vl = check->vl->next;

        check->vl->next = vl;
    }

    valueList_t *last_vl = check->vl;

    while (vl != NULL) {
        if (step == vl->step) {
            break;
        }
        if (step < vl->step) {
            if (last_vl == NULL) {
                // Loading data that's smaller than the smallest valuelist, so insert a new one at the beginning.
                valueList_t *new_vl = newValueList(size, step, now);
                new_vl->next = vl;
                vl = new_vl;
                last_vl->next = vl;
            } else {
                valueList_t *new_vl = newValueList(size, step, now);
                new_vl->next = vl;
                last_vl->next = new_vl;
                vl = new_vl;
            }
            break;
        }

        last_vl = vl;
        vl = vl->next;
    }
    if (vl == NULL) {
        vl = newValueList(size, step, now);
        last_vl->next = vl;
    }
    loadValueList(vl, first, size, step, data);
    return vl;
}

void dumpCheck(check_t *check, char *servername, time_t now, char *output_buf) {
    valueList_t *vl = check->vl->next;
    while (vl != NULL) {
        printf("strlen servercheck dumpCheck (%s/%s) output_buf %d\n", check->name, servername, strlen(output_buf));
        dumpValueList(check->name, servername, vl, now, output_buf);
        vl = vl->next;
    }
}

void dumpServer(server_t *server, time_t now, char *output_buf) {
    check_t *check = server->check->next;
    while (check != NULL) {
        dumpCheck(check, server->name, now, output_buf);
        check = check->next;
    }
}

