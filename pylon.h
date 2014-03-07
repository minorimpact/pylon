typedef struct vlopts {
    int max_buckets;
    int bucket_size;
    int bucket_count;
    int *buckets;
    int cleanup;
} vlopts_t;

extern int daemonize(int nochdir, int noclose);

