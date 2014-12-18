#include "valuelist.h"

typedef struct graph {
   char *name;
   struct graph *next;
   valueList_t *vl;
} graph_t; 

typedef struct server {
    char *name;
    struct server *next;
    struct graph *graph;
} server_t;

int getServerCount(server_t *server_index);
int getGraphCount(server_t *server_index);
long int serverIndexSize(server_t *server_index);
char *getGraphList(server_t *server_index, char *server_id);
char *getServerList(server_t *server_index);
valueList_t *getValueList(server_t *server_index, char *server_id, char *graph_id, time_t now, int range, int force, int bucket_count, int size, int *steps);
void deleteServerByName(server_t *server_index, char *server_id);
server_t *newServerIndex();
valueList_t *loadData(server_t *server_index, char *server_id, char *graph_id, time_t first, int size, int step, double* data, time_t now, int bucket_count, int *steps );
int cleanupServerIndex(server_t *server_index, time_t now, int cleanup);
void deleteServer(server_t *server);
void deleteGraph(graph_t *graph);
server_t *getServerByName(server_t *server_index, char *server_id, int force);
graph_t *getServerGraphByName(server_t *server_index, char *server_id, char *graph_id, int force);
void dumpServer(server_t *server, time_t now, char *output_buf);
void dumpGraph(graph_t *graph, char *servername, time_t now, char *output_buf);

