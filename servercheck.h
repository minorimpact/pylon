#include "valuelist.h"

#define LIST_SIZE 575
#define LIST_STEP 10
/*
#define LIST_SIZE 5
#define LIST_STEP 5
*/

typedef struct check {
   char *name;
   struct check *next;
   valueList_t *vl;
} check_t; 

typedef struct server {
    char *name;
    struct server *next;
    struct check *check;
} server_t;

typedef struct vlopts {
    int max_buckets;
    int bucket_size;
    int bucket_count;
    int *buckets;
    int cleanup;
} vlopts_t;

int getServerCount(server_t *server_index);
int getCheckCount(server_t *server_index);
long int serverIndexSize(server_t *server_index);
char *getCheckList(server_t *server_index, char *server_id);
char *getServerList(server_t *server_index);
valueList_t *getValueList(server_t *server_index, char *server_id, char *check_id, time_t now, int range, vlopts_t *opts, int force);
void deleteServerByName(server_t *server_index, char *server_id);
server_t *newServerIndex();
valueList_t *loadData(server_t *server_index, char *server_id, char *check_id, time_t first, int size, int step, double* data, time_t now, vlopts_t *opts);
void cleanupServerIndex(server_t *server_index, time_t now, int cleanup);
void deleteServer(server_t *server);
void deleteCheck(check_t *check);
server_t *getServerByName(server_t *server_index, char *server_id, int force);
check_t *getServerCheckByName(server_t *server_index, char *server_id, char *check_id, int force);
void dumpServer(server_t *server, time_t now, char *output_buf);
void dumpCheck(char *servername, check_t *check, time_t now, char *output_buf);

