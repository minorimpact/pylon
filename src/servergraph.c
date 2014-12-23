#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include "servergraph.h"

int cleanupServerIndex(server_t *server_index, time_t now, int cleanup) {
    server_t *last_server = server_index;
    server_t *server = server_index->next;
    time_t cutoff = now - cleanup;
    int change = 0;

    outlog(7, "servergraph.cleanupServerIndex: start\n");
    while(server != NULL) {
        graph_t *last_graph = server->graph;
        graph_t *graph = server->graph->next;
        while(graph != NULL) {
            valueList_t *vl = graph->vl->next;
            if (vl->update_time < cutoff) {
                last_graph->next = graph->next;
                deleteGraph(graph);
                graph = last_graph->next;
                change++;
            } else {
                graph = graph->next;
            }
        }

        if (server->graph->next == NULL) {
            last_server->next = server->next;
            deleteServer(server);
            server = last_server->next;
            change++;
        } else {
            server = server->next;
        }
    }
    return change;
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
        server = malloc(sizeof(server_t));
        if (server == NULL) {
            outlog(1, "servergraph.getServerByName: malloc server FAILED\n");
            exit(-1);
        }
        server->name = malloc((strlen(server_id) + 1) * sizeof(char));
        if (server->name == NULL) {
            outlog(1, "servergraph.getServerByName: malloc server->name FAILED\n");
            exit(-1);
        }
        strcpy(server->name, server_id);
        server->graph = malloc(sizeof(graph_t));
        if (server->graph == NULL) {
            outlog(1, "servergraph.getServerByName: malloc server->graph FAILED\n");
            exit(-1);
        }
        server->graph->next = NULL;

        server->next = server_index->next;
        server_index->next = server;
    }

    return server;
}

graph_t *getServerGraphByName(server_t *server_index, char *server_id, char *graph_id, int force) {
    outlog(7, "servergraph.getServerGraphByName: start\n");
    server_t *server = getServerByName(server_index, server_id, force);
    if (server == NULL) {
        return NULL;
    }

    graph_t *graph = server->graph->next;
    while(graph != NULL) {
        if (strcmp(graph->name, graph_id) == 0) {
            return graph;
        }
        graph = graph->next;
    }

    if (force > 0 && strlen(server_id) <=52) {
        graph = malloc(sizeof(graph_t));
        if (graph == NULL) {
            outlog(1, "servergraph.getServerGraphByName: malloc graph FAILED\n");
            exit(-1);
        }
        graph->name = malloc((strlen(graph_id)+1) * sizeof(char));
        if (graph->name == NULL) {
            outlog(1, "servergraph.getServerGraphByName: malloc graph->name FAILED\n");
            exit(-1);
        }
        strcpy(graph->name, graph_id);
        graph->vl = malloc(sizeof(valueList_t));
        if (graph->vl == NULL) {
            outlog(1, "servergraph.getServerGraphByName: malloc graph->vl FAILED\n");
            exit(-1);
        }
        graph->vl->next = NULL;
        graph->next = server->graph->next;
        server->graph->next = graph;
    }

    return graph;
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

int getGraphCount(server_t *server_index) {
    int count = 0;
    server_t *server = server_index->next;

    while(server != NULL) {
        graph_t *graph = server->graph->next;
        while(graph != NULL) {
            count++;
            graph = graph->next;
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
        size += sizeof(graph_t);
        graph_t *graph = server->graph->next;
        while(graph != NULL) {
            size += sizeof(graph_t);
            size += strlen(graph->name);
            valueList_t *vl = graph->vl->next;
            while (vl != NULL) {
                size += (sizeof(double) * vl->size);
                vl = vl->next;
            }
            graph = graph->next;
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
        outlog(1, "servergraph.getServerList: malloc tmp_str FAILED\n");
        exit(-1);
    }
    outlog(10, "servergraph.getServerList: malloc tmp_str %p\n", tmp_str);
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

char *getGraphList(server_t *server_index, char *server_id) {
     server_t *server = getServerByName(server_index, server_id, 0);

    if (server == NULL) {
        return NULL;
    }
   
    graph_t *graph = server->graph->next;
    if (graph == NULL) {
        return NULL;
    }

    int graph_size = 0;
    while(graph != NULL) {
        graph_size += (strlen(graph->name) + 1);
        graph = graph->next;
    }

    if (graph_size > 0) {
        char *tmp_str = malloc(sizeof(char) * (graph_size+1));
        if (tmp_str == NULL) {
            outlog(1, "servergraph.getServerGraphList: malloc tmp_str FAILED\n");
            exit(-1);
        }
        outlog(10, "servergraph.getServerGraphList: malloc tmp_str %p\n", tmp_str);
        tmp_str[0] = 0;
        graph = server->graph->next;
        while(graph != NULL) {
            strcat(tmp_str, graph->name);
            strcat(tmp_str, "|");
            graph = graph->next;
        }
        // Cut the trailing pipe.
        if (strlen(tmp_str) > 0) {
            tmp_str[strlen(tmp_str) - 1] = 0;
        }
        outlog(10, "servergraph.getServerGraphList: graph_size = %d, strlen(tmp_str) = %d\n", graph_size, strlen(tmp_str));
        return tmp_str;
    }
    return NULL;
}

valueList_t *getValueList(server_t *server_index, char *server_id, char *graph_id, time_t now, int range, int force, int bucket_count, int size, int *steps) {
    graph_t *graph = getServerGraphByName(server_index, server_id, graph_id, force);

    if (graph == NULL) {
        return NULL;
    }

    valueList_t *vl = graph->vl->next;
    if (vl == NULL && force > 0) {
        valueList_t *last_vl = graph->vl;
        int i;
        for (i=0; i < bucket_count; i++) {
            valueList_t *vl2 = newValueList(size, steps[i], now);
            last_vl->next = vl2;
            last_vl = vl2;
        }
        vl = graph->vl->next;
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
    outlog(7, "servergraph.deleteServer: start %s\n", server->name);
    if (server->graph->next != NULL) {
        graph_t *graph = server->graph->next;
        while(graph != NULL) {
            graph_t *next = graph->next;
            deleteGraph(graph);
            graph = next;
        }
    }
    free(server->graph);
    free(server->name);
    free(server);
}

void deleteGraph(graph_t *graph) {
    outlog(7, "servergraph.deleteGraph: start %s\n", graph->name);
    valueList_t *vl = graph->vl->next;
    while (vl != NULL) {
        valueList_t *next = vl->next; 
        deleteValueList(vl);
        vl = next;
    }
    free(graph->name);
    free(graph->vl);
    free(graph);
}

void deleteServerByName(server_t *server_index, char *server_id) {
    outlog(7, "servergraph.deleteServerByName: start\n");
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

    graph_t *graph = server->graph->next;

    while(graph != NULL) {
        graph_t *next = graph->next;
        deleteGraph(graph);
    }

    if (prev) {
        prev->next = server->next;
    }

    deleteServer(server);
}

server_t *newServerIndex() {
    outlog(7, "servergraph.newServerIndex: start\n");
    server_t *server_index = malloc(sizeof(server_t));
    if (server_index == NULL) {
        outlog(1, "servergraph.newServerIndex: malloc server_index FAILED\n");
        exit(-1);
    }
    server_index->name = NULL;
    server_index->next = NULL;
    return server_index;
}

valueList_t *loadData(server_t *server_index, char *server_id, char *graph_id, time_t first, int size, int step, double* data, time_t now, int bucket_count, int *steps ) {
    // Get the first valuelist for this server/graph.  If none exist, create a default.
    outlog(7, "servergraph.loadData: start server_id=%s,graph_id=%s,size=%d,step=%d\n",server_id, graph_id, size, step);
    graph_t *graph = getServerGraphByName(server_index, server_id, graph_id, 1);
    valueList_t *vl = graph->vl->next;
    if (vl == NULL) {
        valueList_t *last_vl = graph->vl;
        int i;
        for (i=0; i < bucket_count; i++) {
            valueList_t *vl2 = newValueList(size, steps[i], now);
            last_vl->next = vl2;
            last_vl = vl2;
        }
        vl = graph->vl->next;

        graph->vl->next = vl;
    }

    valueList_t *last_vl = graph->vl;

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

void dumpGraph(graph_t *graph, char *servername, time_t now, char *output_buf) {
    valueList_t *vl = graph->vl->next;
    while (vl != NULL) {
        outlog(8, "servergraph.dumpGraph: strlen (%s/%s) output_buf %d\n", graph->name, servername, strlen(output_buf));
        dumpValueList(graph->name, servername, vl, now, output_buf);
        vl = vl->next;
    }
}

void dumpServer(server_t *server, time_t now, char *output_buf) {
    outlog(7, "servergraph.dumpServer: start\n");
    graph_t *graph = server->graph->next;
    while (graph != NULL) {
        dumpGraph(graph, server->name, now, output_buf);
        graph = graph->next;
    }
}

