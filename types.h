#ifndef TYPES_H
#define TYPES_H

enum rot_t {
    ROT_NONE,
    ROT_COUNT,
};

#define NO_TIMEOUT (-1)

struct rotation {
    enum rot_t type;
    union {
        struct {
            size_t count;
        } count;
    } u;
};

struct config {
    size_t max_failures;
    size_t max_runs;
    bool log_stdout;
    bool log_stderr;
    char *out_path;
    struct rotation rot;
    size_t wait;
    int timeout;
    int verbosity;
    char *error_handler;

    int argc;
    char **argv;

    size_t run_id;
    size_t failures;
};

struct child_status {
    pid_t pid;
    size_t run_id;
    const char *reason;
    bool dumped_core;
    uint8_t exit_status;
    uint8_t term_signal;
    uint8_t stop_signal;
};

enum log_status {
    LOG_RUNNING,
    LOG_PASS,
    LOG_FAIL,
};

static void handle_args(int argc, char **argv);
static void sigchild_handler(int sig);
static int log_path(char *buf, size_t buf_size,
    size_t id, const char *fdname,
    enum log_status status);
static bool try_exec(size_t id, struct child_status *status);
static int supervise_process(struct child_status *status,
    bool *timed_out);
static void setenv_and_call_handler(struct child_status *status,
    char *stdout_log_path, char *stderr_log_path);
static int mainloop(void);
static void init_sigchild_alert(void);
static void init_sigint_handler(void);
static void print_stats(void);

static void close_log(int fd);
static void rename_log(const char *tag, size_t id, bool failed);
static void rotate_log(const char *tag, size_t id);

#endif
