#include <stdio.h> 
#include <malloc.h>
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
                printf("serverchec.cleanupServerIndex: deleting %s->%s (%d < %d)\n", server->name, check->name, vl->update_time, cutoff);
                last_check->next = check->next;
                deleteCheck(check);
                check = last_check->next;
            } else {
                check = check->next;
            }
        }

        if (server->check->next == NULL) {
            printf("serverchec.cleanupServerIndex: deleting %s (no checks)\n", server->name);
            last_server->next = server->next;
            deleteServer(server);
            server = last_server->next;
        } else {
            server = server->next;
        }
    }
}

server_t *getServerByName(server_t *server_index, char *server_id) {
    server_t *server = server_index->next;

    // Scan the list for the server;
    while(server != NULL) {
        if (strcmp(server->name, server_id) == 0) {
            return server;
        }
        server = server->next;
    }

    server = malloc(sizeof(server_t));
    //printf("malloc server(%p)\n", server);
    server->name = malloc((strlen(server_id) + 1) * sizeof(char));
    strcpy(server->name, server_id);
    server->check = malloc(sizeof(check_t));
    //printf("malloc server->checks(%p)\n", server->checks);
    server->check->next = NULL;

    server->next = server_index->next;
    server_index->next = server;

    return server;
}

check_t *getServerCheckByName(server_t *server_index, char *server_id, char *check_id) {
    server_t *server;
    printf("servercheck.getServerCheckByName: looking for server %s\n", server_id);
    server = getServerByName(server_index, server_id);
    printf("servercheck.getServerCheckByName: got server %s\n", server->name);
    if (server == NULL) {
        return NULL;
    }

    printf("servercheck.getServerCheckByName: looking for check %s\n", check_id);
    check_t *check = server->check->next;
    while(check != NULL) {
        if (strcmp(check->name, check_id) == 0) {
            return check;
        }
        check = check->next;
    }

    check = malloc(sizeof(check_t));
    check->name = malloc((strlen(check_id)+1) * sizeof(char));
    strcpy(check->name, check_id);
    check->vl = malloc(sizeof(valueList_t));
    check->vl->next = NULL;
    check->next = server->check->next;
    server->check->next = check;

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

int serverIndexSize(server_t *server_index) {
    int size = sizeof(server_t);
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
    char *tmp_str = malloc(30720 * sizeof(char));
    //printf("malloc tmp_str(%p)\n", tmp_str);
    tmp_str[0] = 0;

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

char *getCheckList(server_t *server_index, char *server_id, char *tmp_str) {
    server_t *server = server_index->next;
    //char *tmp_str = malloc(30720 * sizeof(char));
    //printf("malloc tmp_str(%p)\n", tmp_str);
    tmp_str[0] = 0;

    while(server != NULL) {
        check_t *check = server->check->next;
        if (strcmp(server->name, server_id) == 0) {
            break;
        }
        server = server->next;
    }

    if (server == NULL) {
        return tmp_str;
    }
   
    check_t *check = server->check->next;
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

valueList_t *getValueList(server_t *server_index, char *server_id, char *check_id, time_t now, int range, vlopts_t *opts) {
    check_t *check = getServerCheckByName(server_index, server_id, check_id);

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
    } else {
        makeValueListCurrent(vl, now);
    }

    if (range == 0) {
        return vl;
    }
    valueList_t *last_vl = NULL;
    while (vl != NULL) {
        printf("servercheck.getValueList: comparing %d,%d (vl->first: %d) to %d\n", vl->size, vl->step, vl->first, range);
        if  (vl->first <= range) {
            return vl;
        }
        if (vl->next == NULL) {
            return vl;
        }

        last_vl = vl;
        vl = vl->next;
        if (vl != NULL) {
            makeValueListCurrent(vl, now);
        }
    }
    return last_vl;
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
    //printf("free server->checks(%p)\n",server->checks);
    free(server->check);
    //printf("free server->name(%p)\n",server->name);
    free(server->name);
    //printf("free server(%p)\n",server);
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
    //printf("malloc server_index(%p)\n", server_index);
    server_index->name = NULL;
    server_index->next = NULL;
    return server_index;
}

valueList_t *loadData(server_t *server_index, char *server_id, char *check_id, time_t first, int size, int step, double* data, time_t now, vlopts_t *opts) {
    // Get the first valuelist for this server/check.  If none exist, create a default.
    check_t *check = getServerCheckByName(server_index, server_id, check_id);
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
        printf("servercheck.loadData: checking vl->size=%d, vl->step=%d\n", vl->size, vl->step);
        if (step == vl->step) {
            printf("servercheck.loadData: found a match\n");
            break;
        }
        if (step < vl->step) {
            if (last_vl == NULL) {
                // Loading data that's smaller than the smallest valuelist, so insert a new one at the beginning.
                printf("servercheck.loadData: creating %d,%d before %d,%d\n", size, step, vl->size, vl->step);
                valueList_t *new_vl = newValueList(size, step, now);
                new_vl->next = vl;
                vl = new_vl;
                last_vl->next = vl;
            } else {
                printf("servercheck.loadData: creating %d,%d between %d,%d and %d,%d\n", size, step, last_vl->size, last_vl->step, vl->size, vl->step);
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
        printf("servercheck.loadData: adding %d,%d to end of list\n", size, step);
        vl = newValueList(size, step, now);
        last_vl->next = vl;
    }
    printf("servercheck.loadData: using %d,%d\n", size, step);
    loadValueList(vl, first, size, step, data);
    return vl;
}

