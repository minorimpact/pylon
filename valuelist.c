#include <malloc.h>
#include <time.h> 
#include <stdio.h>
#include <string.h>
#include "valuelist.h"

/*
 * General Notes:
 *  - These functions all take "now" as an option so when multiple functions are called together for multiple objects, they're all working off same value of
 *    "current".  Otherwise, if two operations cross from one second to another, you could end up with different, unmatched sets of data.
 */

/* 
 * Returns a pointer to a new valuelist object.
 */
valueList_t *newValueList(int size, int step, time_t now) {
    valueList_t *vl = malloc(sizeof(valueList_t));
    //printf("valuelist.newValueList:size=%d,step=%d,now=%d\n", size, step, now); 

    vl->first = now - (now % step) - (step * (size - 1));
    vl->step = step;
    vl->size = size;
    vl->update_time = 0;
    vl->update_value = 0.0/0.0;
    vl->update_counter_value = 0.0/0.0;

    vl->data = malloc(size*sizeof(double));
    //printf("malloc vl->data(%p)\n", vl->data);
    int i;
    for (i=0;i<size;i++) {
        vl->data[i] = 0.0/0.0;
    }
    vl->head = vl->data;
    vl->next = NULL;

    return vl;
}

/*
 * Adds a new value to a given valuelist.
 *
 * NOTE: What does type mean?
 * type: 0 = gauge
 *       1 = counter
 */
void addValue(valueList_t *vl, double value, time_t now, int type) {
    if (vl == NULL) {
        return;
    }
    makeValueListCurrent(vl,now);
    int last = vl->first + (vl->step * (vl->size - 1));
    int i;

    //printf("valuelist.addValue:value=%f,now=%d,step=%d,type=%d,first=%d,last=%d,update_time=%d,update_value=%f,update_counter_value=%f\n", value, now, vl->step,type,vl->first,last,vl->update_time,vl->update_value,vl->update_counter_value);

    // Reject any attempt to add a value before the "last" possible value of the value list.
    if (now <= last) {
       //printf("valuelist.addValue: too soon! %d <= %d\n", now, last);
       return;
    }


    // Never add a raw value to the value list.  The value added will be derived from this value and the next value.
    // If the valuelist has never been updated, or was last updated too long ago, store the new value and graph a nan instead.
    if (vl->update_time == 0 ||  (vl->update_time < (last - vl->step))) {
        //printf("valuelist.addValue:previous update too old, caching update\n");
        if (type == 1) {
            vl->update_value = 0.0/0.0;
            vl->update_counter_value = value;
            type = 0;
        } else {
            vl->update_value = value;
        }
        value = 0.0/0.0;
    } 


    if (type == 1) {
        int diff_time = now - vl->update_time;
        double diff_value = value - vl->update_counter_value;
        vl->update_counter_value = value;
        if (diff_value < 0) {
            // Rollover, start over.
            //printf("valuelist.addValue: counter rollover, resetting (diff_value=%f)\n",diff_value);
            vl->update_value = 0.0/0.0;
            value = 0.0/0.0;
        } else if (isnan(vl->update_value)) {
            // We need to delay collect 'counter' data twice before we can graph it, since we don't have a proper 'value' until the second consecutive entry.  Counters won't actually start
            // to graph until the third consecutive entry.
            vl->update_value = (diff_value / diff_time);
            //printf("valuelist.addValue: second counter update, derived value=%f\n",vl->update_value);
            value = 0.0/0.0;
        } else {
            value = (diff_value/diff_time);
        }
    }

    int mod = now % vl->step;
    double graph_value;
    if (vl->update_value > value) {
        graph_value = (((vl->update_value - value) / (now - vl->update_time)) * (now - last)) + value;
    } else if (vl->update_value < value) {
        graph_value = (((value - vl->update_value) / (now - vl->update_time)) *  (now - last)) + vl->update_value;
    } else {
        graph_value = value;
    }
    //printf("valuelist.addValue:graph_value=%f\n", graph_value);

    vl->head++;
    if (vl->head >= (vl->data + vl->size)) {
        vl->head = vl->data;
    }
    *vl->head = graph_value;
    vl->first += vl->step;
    vl->update_time = now;

    if (!isnan(value)) {
        vl->update_value = value;
    }

    // Figure out the value to add to the next 
    if (vl->next != NULL) {
        valueList_t *next = vl->next;
        int steps = ((int) (next->step/vl->step));
        int nonnansteps = 0;
        double *head = vl->head;
        double total = 0.0/0.0;
        double value;
        for (i=1; i<=steps; i++) {
            value = *head;
            if (!isnan(value)) {
                if (isnan(total)) {
                    total = value;
                } else {
                    total = total + value;
                }
                nonnansteps++;
            }

            head = head--;
            if (head < vl->data) {
                head = vl->data + (vl->size-1);
            }

            if (i >= vl->size) {
                break;
            }
        }

        if (!isnan(total)) {
            if (nonnansteps > 0) {
                double average = total / nonnansteps;
                //printf("valuelist.addValue:total=%f,nonnansteps=%d,average=%f\n",total,nonnansteps,average);
                addValue(next, average, now, 0);
            }
        }
    }
    //printf("valuelist.addValue:done\n");
    return;
}

/* 
 * Bring the value list up to date.  It's important to remember that this doesn't set the
 * value list to the most recent bucket, but one before.  After this function is called, the
 * value list is in a position to be written to.
 */
time_t makeValueListCurrent(valueList_t *vl, time_t now) {
    if (vl == NULL) {
        return 0;
    }
    int last = vl->first + (vl->step * (vl->size - 1));
    int diff = now - last;

    //printf("valuelist.makeValueListCurrent: vl->first=%d,vl->step=%d,vl->size=%d,now=%d\n",vl->first,vl->step,vl->size,now);
    if (diff < vl->step) {
        return vl->first;
    } else {
        //printf("valuelist.makeValueListCurrent: diff=%d,vl->size=%d,vl->step=%d\n", diff, vl->size, vl->step); 
        int missing_steps = ((int) (diff/vl->step)) - 1;
        //printf("valuelist.makeValueListCurrent: missing steps = '%d'\n", missing_steps);
        if (missing_steps > vl->size) {
            missing_steps = vl->size;
            last = now - (now % vl->step) - (vl->step * (vl->size + 1));
        }
        if (missing_steps > 0) {
            //printf("valuelist.makeValueListCurrent: inserting %d missing value(s)\n", missing_steps);

            int j;
            for (j = 0; j < missing_steps; j++) {
                vl->head++;
                if (vl->head >= vl->data + vl->size) {
                    vl->head = vl->data;
                }
                *vl->head = 0.0/0.0;
                vl->first = vl->first + vl->step;
            }
        }
    }
    //printf("valuelist.makeValueListCurrent: vl->first = '%d'\n", vl->first);
    return vl->first;
}

/*
 * Reset a value list with a new complete set of data.
 */
void loadValueList(valueList_t *vl, time_t first, int size, int step, double* data) {
    if (vl == NULL) {
        return;
    }
    vl->update_time = first + (step * (size - 1));
    vl->update_value = data[size - 1];
    //printf("valuelist.loadValueList: setting vl->update_time = %d\n", vl->update_time);
    //printf("free vl->data(%p)\n", vl->data);
    free(vl->data);
    vl->first = first;
    vl->data = data;
    vl->head = data + (vl->size - 1);
    vl->size = size;
    vl->step = step;
}

double dataAverage(int size, double *data) {
    int i;
    int nonnansize = 0;
    double total = 0;
    for (i=0;i<size;i++) {
        if (!isnan(data[i])) {
            total += data[i];
            nonnansize++;
        }
    }
    if (nonnansize > 0) {
       return total/nonnansize;
    }
   return 0.0/0.0;
} 

/*
 * Returns the data set from a valuelist object.
 */
void getValueListData(valueList_t *vl, double *data) {
    int i;
    if (vl == NULL) {
        return;
    }
    double *head = vl->head;
    for (i=0; i<vl->size; i++) {
        head = head++;
        if (head >= (vl->data + vl->size)) {
            head = vl->data;
        }
        data[i]=*head;
    }
}

/*
 * Add the data from valuelist to an existing set of data.
 */
void addValueListToData(valueList_t *vl, double *data) {
    int i;
    if (vl == NULL) {
        return;
    }
    double *head = vl->head;
    for (i=0; i<vl->size; i++) {
        head = head++;
        if (head >= (vl->data + vl->size)) {
            head = vl->data;
        }
        double value = *head;

        if (!isnan(value)) {
            if (isnan(data[i])) {
                data[i] = value;
            } else {
                data[i] = data[i] + value;
            }
        }
    }
}

/*
 * Destroy a valuelist object.
 */
void deleteValueList(valueList_t *vl) {
    if (vl == NULL) {
        return;
    }
    printf("valueList.deleteValueList: step=%d\n", vl->step);
    free(vl->data);
    free(vl);
}

char *dumpValueList(valueList_t *vl, time_t now, char *output_buf) {
    char tmp_str[100];

    printf("valuelist.dumpValueList:dumping %d,%d\n",vl->step, vl->size);
    makeValueListCurrent(vl, now);
    output_buf[0] = 0;
    sprintf(tmp_str, "%d|", vl->first);
    strcat(output_buf, tmp_str);
    sprintf(tmp_str, "%d|", vl->size);
    strcat(output_buf, tmp_str);
    sprintf(tmp_str, "%d|", vl->step);
    strcat(output_buf, tmp_str);

    double data[vl->size];
    getValueListData(vl, data);

    int i;
    for (i=0; i<vl->size; i++) {
        sprintf(tmp_str,"%f|",data[i]);
        strcat(output_buf,tmp_str);
    }
    output_buf[strlen(output_buf) -1] = '\n';
    printf("valuelist.dumpValueList:done\n");
    return output_buf;
}


