#ifndef TYPES_H
#define TYPES_H

typedef enum {
    ROT_NONE,
    ROT_COUNT,
} rot_t;

#define TIMED_OUT 126
#define NO_TIMEOUT (-1)

typedef struct {
    rot_t type;
    union {
        struct {
            size_t count;
        } count;
    } u;
} rotation;

typedef struct {
    size_t max_failures;
    size_t max_runs;
    bool log_stdout;
    bool log_stderr;
    char *out_path;
    rotation rot;
    int wait;
    int timeout;
    int verbosity;
    char *error_handler;

    /* TODO */
    //char *out_pipe;

    int argc;
    char **argv;
} config;

typedef struct {
    pid_t pid;
    int run_id;
    const char *reason;
    bool dumped_core;
    uint8_t exit_status;
    uint8_t term_signal;
    uint8_t stop_signal;
} child_status;

static void handle_args(config *cfg, int argc, char **argv);
static bool try_exec(config *cfg, size_t id, child_status *status);
static int mainloop(config *cfg);
static void setenv_and_call_handler(config *cfg, child_status *status);
//static void process_log(config *cfg, char *path);

#endif
