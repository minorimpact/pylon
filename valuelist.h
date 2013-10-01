#include <time.h> 
#include <malloc.h>

typedef struct valueList {
    int step;
    int size;
    time_t first;
    double *data;
    double *head;
    time_t update_time;
    double update_value;
    double update_counter_value;
    struct valueList *next;
} valueList_t;

valueList_t *newValueList(int size, int step, time_t now);
void addValue(valueList_t *vl, double value, time_t now, int type);
void loadValueList(valueList_t *vl, time_t first, int size, int step, double *data);
void getValueListData(valueList_t *vl, double *data);
time_t makeValueListCurrent(valueList_t *vl, time_t now);
void dumpValueList(char *check, char *server, valueList_t *vl, time_t now, char *output_buf);

