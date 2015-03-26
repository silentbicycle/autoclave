#ifndef TYPES_H
#define TYPES_H

typedef enum {
    ROT_NONE,
    ROT_COUNT,
} rot_t;

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
    size_t wait;
    int timeout;
    int verbosity;
    char *error_handler;

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
static void sigchild_handler(int sig);
static int log_path(char *buf, size_t buf_size,
    config *cfg, size_t id, const char *fdname);
static bool try_exec(config *cfg, size_t id, child_status *status);
static int supervise_process(config *cfg, child_status *status,
    bool *timed_out);
static void setenv_and_call_handler(config *cfg, child_status *status,
    char *stdout_log_path, char *stderr_log_path);
static int mainloop(config *cfg);
static void init_sigchild_alert(void);
static void close_log(int fd, bool failed);
static void rotate_log(config *cfg, const char *tag, int id);

#endif
