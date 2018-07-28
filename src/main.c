/* 
 * Copyright (c) 2015 Scott Vokes <vokes.s@gmail.com>
 *  
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *  
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <err.h>
#include <errno.h>
#include <fcntl.h>
#include <getopt.h>
#include <limits.h>
#include <poll.h>
#include <time.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <libgen.h>
#include <ctype.h>

/* Version 0.1.1 */
#define AUTOCLAVE_VERSION_MAJOR 0
#define AUTOCLAVE_VERSION_MINOR 1
#define AUTOCLAVE_VERSION_PATCH 1
#define AUTOCLAVE_AUTHOR "Scott Vokes <vokes.s@gmail.com>"

#include "types.h"

static const char REASON_UNDEF[] = "undef";
static const char REASON_TIMEOUT[] = "timeout";
static const char REASON_EXIT[] = "exit";
static const char REASON_TERM[] = "term";
static const char REASON_STOP[] = "stop";

static long arg_max_size;
static char *output_prefix_buf;

static const struct config * cfg;

static struct state state;

static void usage(const char *msg) {
    if (msg) { fprintf(stderr, "%s\n\n", msg); }
    fprintf(stderr, "autoclave v. %d.%d.%d by %s\n",
        AUTOCLAVE_VERSION_MAJOR, AUTOCLAVE_VERSION_MINOR,
        AUTOCLAVE_VERSION_PATCH, AUTOCLAVE_AUTHOR);
    fprintf(stderr,
        "Usage: autoclave [-h] [-c <count>] [-l] [-e] [-f <max_failures>]\n"
        "                 [-i <id_str>] [-k <signal>] [-m <min_duration_msec>]\n"
        "                 [-o <output_prefix>] [-r <max_runs>] [-s]\n"
        "                 [-t <timeout_sec>] [-v] [-x <cmd>] <command line>\n"
        "\n"
        "    -h:         print this help\n"
        "    -c COUNT:   rotate log files by count\n"
        "    -f COUNT:   max failures (def. 1)\n"
        "    -i STR:     replace STR in args with run_id\n"
        "    -k INT:     signal to send process on timeout\n"
        "    -l:         log stdout\n"
        "    -e:         log stderr\n"
        "    -m MSEC:    min duration per run, will delay to pad (def. 50 msec)\n"
        "    -o PATH:    log output prefix (default: program's $0)\n"
        "    -r COUNT:   max runs (def. no limit)\n"
        "    -t SEC:     timeout for watched program (in seconds)\n"
        "    -s:         supervise (abbreviation for `-l -e -v`)\n"
        "    -v:         increase verbosity\n"
        "    -x CMD:     execute command on error/timeout\n"
        );
    
    exit(1);
}

static int signal_id_from_str(const char *name) {
    if (isdigit(name[0])) {
        int res = strtoll(optarg, NULL, 10);
        if (res <= 0 || res > SIGRTMAX) { return -1; }
        return res;
    }
    /* Name-based lookup is not currently supported. */
    return -1;
}

static void handle_args(struct config *cfg, int argc, char **argv) {
    int fl = 0;
    while ((fl = getopt(argc, argv, "hc:ef:i:k:lm:o:r:st:vx:")) != -1) {
        switch (fl) {
        case 'h':               /* help */
            usage(NULL);
            break;
        case 'c':               /* rotation count */
            cfg->rot.type = ROT_COUNT;
            cfg->rot.u.count.count = (size_t)strtoll(optarg, NULL, 10);
            break;
        case 'e':               /* log stderr */
            cfg->log_stderr = true;
            break;
        case 'f':               /* max failures */
            cfg->max_failures = (size_t)strtoll(optarg, NULL, 10);
            break;
        case 'i':               /* run_id string */
            cfg->run_id_str = optarg;
            break;
        case 'k':               /* timeout kill signal */
            cfg->timeout_kill_signal = signal_id_from_str(optarg);
            if (cfg->timeout_kill_signal == -1) {
                fprintf(stderr, "Invalid signal: %s\n", optarg);
                usage(NULL);
            }
            break;
        case 'l':               /* log stdout */
            cfg->log_stdout = true;
            break;
        case 'm':               /* min_duration (in msec.) */
            cfg->min_duration_msec = (size_t)strtoll(optarg, NULL, 10);
            break;
        case 'o':               /* output prefix */
            cfg->output_prefix = optarg;
            break;
        case 'r':               /* max runs */
            cfg->max_runs = (size_t)strtoll(optarg, NULL, 10);
            break;
        case 's':               /* supervise: abbreviation for -l -e -v */
            cfg->log_stdout = true;
            cfg->log_stderr = true;
            cfg->verbosity++;
            break;
        case 't':               /* timeout (in sec) */
            cfg->timeout_sec = (size_t)strtoll(optarg, NULL, 10);
            break;
        case 'v':               /* verbosity */
            cfg->verbosity++;
            break;
        case 'x':               /* execute error handler */
            cfg->error_handler = optarg;
            break;
        case '?':
        default:
            usage(NULL);
        }
    }

    argc -= (optind - 1);
    argv += (optind - 1);
    cfg->argc = argc - 1;
    cfg->argv = argv + 1;

    if (cfg->argc < 1) { usage(NULL); }

    if (cfg->log_stdout || cfg->log_stderr) {
        if (cfg->output_prefix == NULL) {
            /* Construct a default prefix for the logs. */
            char argv_cp[arg_max_size + 1];
            memset(argv_cp, 0x00, arg_max_size + 1);
            strncpy(argv_cp, cfg->argv[0], arg_max_size);

            output_prefix_buf = calloc(1, arg_max_size);
            if (output_prefix_buf == NULL) { err(1, "calloc"); }
            (void)snprintf(output_prefix_buf, arg_max_size,
                "autoclave_%s", basename(argv_cp));
            cfg->output_prefix = output_prefix_buf;
        } else {
            /* If output prefix contain a sub-directories, then attempt
             * to create it, if not already present. */
            char prefix_cp[arg_max_size + 1];
            memset(prefix_cp, 0x00, arg_max_size + 1);

            strncpy(prefix_cp, cfg->output_prefix, arg_max_size);
            char *dir = dirname(prefix_cp);
            if (-1 == mkdir(dir, 0700)) {
                if (errno == EEXIST) {
                    errno = 0;      /* already exists, ignore */
                } else {
                    err(1, "mkdir: %s", dir);
                }
            }
        }
    }
}    

/* Pipes used for handling SIGCHLD notifications. */
static int alert_wr_pipe;
static int alert_rd_pipe;

/* Send a notification when the child process terminates.
 * The content of the write isn't actually important,
 * it's just used to interrupt the poll and wake the
 * supervisor process up immediately .*/
static void sigchild_handler(int sig) {
    assert(sig == SIGCHLD);
#define RETRIES 100             /* arbitrary */
    for (int retries = 0; retries < RETRIES; retries++) {
        /* POSIX.1-2004 requires calling write(2) in a
         * signal handler to be safe. */
        ssize_t res = write(alert_wr_pipe, "!", 1);
        if (res == 1) { return; }
        if (res == -1) {
            if (errno == EINTR) {
                errno = 0;
            } else {
                err(1, "write");
            }
        }
    }
    assert(false);
}

static void sigint_handler(int sig) {
    assert(sig == SIGINT);
    print_stats();
    free(output_prefix_buf);
    exit(state.failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS);
}

static int log_path(char *buf, size_t buf_size,
    size_t id, const char *fdname,
    enum log_status status) {

    char *status_suffix;
    switch (status) {
    case LOG_RUNNING:
        status_suffix = "";
        break;
    case LOG_PASS:
        status_suffix = ".pass";
        break;
    case LOG_FAIL:
        status_suffix = ".FAIL";
        break;
    default:
        fprintf(stderr, "(MATCH FAIL)\n");
        assert(false);
    }

    int res = snprintf(buf, buf_size, "%s%s.%zd.%s.log",
        cfg->output_prefix, status_suffix, id, fdname);

    if ((int)buf_size < res) {
        fprintf(stderr, "snprintf: path too long\n");
        exit(1);
    }
    return res;
}

static bool try_exec(size_t id, struct child_status *status) {
    memset(status, 0, sizeof(*status));
    char outlogbuf[PATH_MAX];
    int outlog = -1;

    const char *tag_stdout = "stdout";
    const char *tag_stderr = "stderr";

    if (cfg->log_stdout) {
        log_path(outlogbuf, PATH_MAX, id, tag_stdout, LOG_RUNNING);
        outlog = open(outlogbuf, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (outlog == -1) { err(1, "open"); }
    }

    char errlogbuf[PATH_MAX];
    int errlog = -1;
    if (cfg->log_stderr) {
        log_path(errlogbuf, PATH_MAX, id, tag_stderr, LOG_RUNNING);
        errlog = open(errlogbuf, O_WRONLY | O_TRUNC | O_CREAT, 0644);
        if (errlog == -1) { err(1, "open"); }
    }

    bool failed = false;
    int kid = fork();
    if (kid == -1) {
        err(1, "fork");
    } else if (kid == 0) {      /* child */
        if (outlog != -1) {
            if (-1 == dup2(outlog, STDOUT_FILENO)) { err(1, "dup2"); }
        }
        if (errlog != -1) {
            if (-1 == dup2(errlog, STDERR_FILENO)) { err(1, "dup2"); }
        }
        
        /* If run_id_str is used (e.g. `-i %`) then replace every
         * argument matching its string with the run_id */
        char run_id_buf[16];
        if (cfg->run_id_str != NULL) {
            (void)snprintf(run_id_buf, sizeof(run_id_buf),
                "%zu", state.run_id);
            for (int i = 1; i < cfg->argc; i++) {
                if (0 == strcmp(cfg->argv[i], cfg->run_id_str)) {
                    cfg->argv[i] = run_id_buf;
                }
            }
        }

        int res = execvp(cfg->argv[0], &cfg->argv[0]);
        if (res == -1) { err(1, "execvp"); }
    } else {                   /* parent */
        status->pid = kid;
        status->run_id = id;
        bool timed_out = false;
        int stat_loc = supervise_process(status, &timed_out);
        status->reason = REASON_UNDEF;
#ifdef WCOREDUMP
        status->dumped_core = WCOREDUMP(stat_loc);
#endif

        if (timed_out) {
            status->reason = REASON_TIMEOUT;
            failed = true;
        } else if (WIFEXITED(stat_loc)) {
            status->reason = REASON_EXIT;
            status->exit_status = WEXITSTATUS(stat_loc);
            failed = (status->exit_status != EXIT_SUCCESS);
        } else if (WIFSIGNALED(stat_loc)) {
            status->reason = REASON_TERM;
            status->term_signal = WTERMSIG(stat_loc);
            failed = true;
        } else if (WIFSTOPPED(stat_loc)) {
            status->reason = REASON_STOP;
            status->stop_signal = WSTOPSIG(stat_loc);
            failed = true;
        }

        if (cfg->verbosity > 1) {
            printf(" -- type: %s, core? %d, exit: %d, term: %d, stop: %d\n",
                status->reason, status->dumped_core,
                status->exit_status, status->term_signal,
                status->stop_signal);
        }

        if (failed && cfg->error_handler != NULL) {
            setenv_and_call_handler(status,
                outlog != -1 ? outlogbuf : NULL,
                errlog != -1 ? errlogbuf : NULL);
        } else if (status->reason == REASON_TIMEOUT) {
            int res = kill(kid, cfg->timeout_kill_signal);
            if (res == -1) {
                if (errno == ESRCH) {
                    /* Race: child terminated on its own, as we
                     * timed it out. Consider it a timeout. */
                    errno = 0;
                } else {
                    err(1, "kill");
                }
            }
        }
    }

    if (outlog != -1) {
        close_log(outlog);
        rename_log(tag_stdout, id, failed);
        rotate_log(tag_stdout, id);
    }
    if (errlog != -1) {
        close_log(errlog);
        rename_log(tag_stderr, id, failed);
        rotate_log(tag_stderr, id);
    }
    return failed;
}

static void close_log(int fd) {
    if (-1 == close(fd)) { err(1, "close"); }
}

static void rename_log(const char *tag, size_t id, bool failed) {
    char oldlogbuf[PATH_MAX];
    char newlogbuf[PATH_MAX];
    log_path(oldlogbuf, PATH_MAX, id, tag, LOG_RUNNING);
    log_path(newlogbuf, PATH_MAX, id, tag,
        failed ? LOG_FAIL : LOG_PASS);

    errno = 0;
    const int res = rename(oldlogbuf, newlogbuf);
    if (res == -1) { err(1, "rename"); }
}

static void rotate_log(const char *tag, size_t id) {
    if (cfg->rot.type == ROT_COUNT) {
        const size_t count = cfg->rot.u.count.count;
        char oldlogbuf[PATH_MAX];
        /* Only rotate logs from passing runs */
        log_path(oldlogbuf, PATH_MAX, id - count, tag, LOG_PASS);

        if (id >= count) {
            int res = unlink(oldlogbuf);
            if (res == -1) {
                if (errno == ENOENT) {
                    /* Couldn't find file -- may not exist, or may
                     * be a failure, which should probably be kept. */
                    errno = 0;
                } else {
                    err(1, "unlink");
                }
            }
        }
    }
}

static int supervise_process(struct child_status *status, bool *timed_out) {
    int stat_loc = 0;
    size_t ticks = 0;
    const int sleep_msec = 100;
    const size_t max_ticks = (size_t)cfg->timeout_sec * (1000 / sleep_msec);
    
    struct pollfd fds[] = {
        {
            .fd = alert_rd_pipe,
            .events = POLLIN,
        },
    };
    
    while (ticks < max_ticks) {
        /* Poll for child process termination. */
        const pid_t res = waitpid(status->pid, &stat_loc, WNOHANG);
        if (res == -1) {
            if (errno == EINTR) {
                errno = 0;
            } else {
                err(1, "wait");
            }
        } else if (res == 0) {
            /* Sleep 100 msec, unless a SIGCHLD wakes it up. */
            const int poll_res = poll(fds, 1, sleep_msec);
            if (poll_res == 1) {
                char buf[8];
                ssize_t rd = read(alert_rd_pipe, buf, sizeof(buf));
                if (rd >= 0) {
                    /* No-op -- the read is ignored. */
                } else if (rd == -1) {
                    if (errno == EINTR) {
                        errno = 0;
                    } else {
                        err(1, "read");
                    }
                }
            }
            if (cfg->timeout_sec != NO_TIMEOUT) { ticks++; }
        } else {
            assert(res == status->pid);
            break;
        }
    }

    if (ticks == max_ticks) { *timed_out = true; }
    return stat_loc;
}

static void setenv_and_call_handler(struct child_status *status,
    char *stdout_log_path, char *stderr_log_path) {
    setenv("AUTOCLAVE_DUMPED_CORE",
        status->dumped_core ? "1" : "0", 1);
    setenv("AUTOCLAVE_FAIL_TYPE", status->reason, 1);

    const int sz = 32;
    char exit_buf[sz];
    if (sz >= snprintf(exit_buf, sz, "%d", (int)status->exit_status)) {
        setenv("AUTOCLAVE_EXIT_STATUS", exit_buf, 1);
    }
    char term_buf[sz];
    if (sz >= snprintf(term_buf, sz, "%d", (int)status->term_signal)) {
        setenv("AUTOCLAVE_TERM_SIGNAL", term_buf, 1);
    }
    char stop_buf[sz];
    if (sz >= snprintf(stop_buf, sz, "%d", (int)status->stop_signal)) {
        setenv("AUTOCLAVE_STOP_SIGNAL", stop_buf, 1);
    }
    char child_pid_buf[sz];
    if (sz >= snprintf(child_pid_buf, sz, "%d", (int)status->pid)) {
        setenv("AUTOCLAVE_CHILD_PID", child_pid_buf, 1);
    }    
    char id_buf[sz];
    if (sz >= snprintf(id_buf, sz, "%d", (int)status->run_id)) {
        setenv("AUTOCLAVE_RUN_ID", id_buf, 1);
    }
    setenv("AUTOCLAVE_CMD", cfg->argv[0], 1);

    if (stdout_log_path) {
        setenv("AUTOCLAVE_STDOUT_LOG", stdout_log_path, 1);
    }
    if (stderr_log_path) {
        setenv("AUTOCLAVE_STDERR_LOG", stderr_log_path, 1);
    }

    if (-1 == system(cfg->error_handler)) {
        err(1, "system");
    }
}

static void cur_time(struct timeval *tv) {
    assert(tv != NULL);
#if  _POSIX_C_SOURCE >= 199309L
    struct timespec ts;
    if (-1 == clock_gettime(CLOCK_MONOTONIC, &ts)) {
        err(1, "clock_gettime");
    }
    tv->tv_sec = ts.tv_sec;
    tv->tv_usec = ts.tv_nsec/1000; /* note: nsec -> usec */
#else
    if (-1 != gettimeofday(tv, NULL)) { err(1, "gettimeofday"); }
#endif
}

static double calc_duration(const struct timeval *pre,
    const struct timeval *post) {
    return (post->tv_sec < pre->tv_sec || (post->tv_sec == pre->tv_sec
            && post->tv_usec < pre->tv_usec))
      ? 0      /* non-monotonic clock, ignore negative time delta */
      : (1000.0 * (post->tv_sec - pre->tv_sec)
          + (post->tv_usec - pre->tv_usec)/1000.0);
}

static int mainloop(void) {
    cur_time(&state.start_time);

    while (state.run_id < cfg->max_runs) {
        struct timeval pre, post;
        struct child_status s;
        cur_time(&pre);
        bool failed = try_exec(state.run_id, &s);
        cur_time(&post);
        if (failed) { state.failures++; }
        if (state.failures >= cfg->max_failures) { break; }
        state.run_id++;

        const double duration_msec = calc_duration(&pre, &post);

        if (cfg->verbosity > 0) {
            printf("%08lld.%06lld -- %zd run%s, %zd failure%s, %g msec\n",
                (long long)post.tv_sec, (long long)post.tv_usec,
                state.run_id, state.run_id == 1 ? "" : "s",
                state.failures, state.failures == 1 ? "" : "s",
                duration_msec);
        }

        /* If the run did not take at least the minimum duration, then
         * wait, to pad the overall run time to the minimum. */
        if (duration_msec < cfg->min_duration_msec) {
            const size_t rem = cfg->min_duration_msec - duration_msec;
            poll(NULL, 0, (int)rem);
        }
    }
    print_stats();
    return state.failures > 0 ? EXIT_FAILURE : EXIT_SUCCESS;
}

static void init_sigchild_alert(void) {
    int pipes[2];
    if (0 != pipe(pipes)) { err(1, "pipe"); }
    alert_rd_pipe = pipes[0];
    alert_wr_pipe = pipes[1];

    struct sigaction sa = {
        .sa_handler = sigchild_handler,
    };
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        err(1, "sigaction");
    }
}

static void init_sigint_handler(void) {
    struct sigaction sa = {
        .sa_handler = sigint_handler,
    };
    if (sigaction(SIGINT, &sa, NULL) == -1) {
        err(1, "sigaction");
    }
}

static void print_stats(void) {
    const size_t passes = state.run_id - state.failures;
    struct timeval post;
    cur_time(&post);
    double duration = calc_duration(&state.start_time, &post);

    printf("-- %zu run%s, %zu pass%s, %zu failure%s, %g msec\n",
        state.run_id, state.run_id == 1 ? "" : "s",
        passes, passes == 1 ? "" : "es",
        state.failures, state.failures == 1 ? "" : "s",
        duration);
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    arg_max_size = sysconf(_SC_ARG_MAX);

    struct config config = {
        .max_failures = DEF_MAX_FAILURES,
        .max_runs = NO_LIMIT,
        .min_duration_msec = DEF_MIN_DURATION_MSEC,
        .timeout_sec = NO_TIMEOUT,
        .timeout_kill_signal = SIGTERM,
    };
    handle_args(&config, argc, argv);
    cfg = &config;              /* After this point, cfg is const */

    init_sigchild_alert();
    init_sigint_handler();

    return mainloop();
}
