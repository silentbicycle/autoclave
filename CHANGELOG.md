# autoclave Changes By Release

## v0.2.0 - 2018-xx-xx

### API Changes

After each run, logs are now renamed to include a `pass` or `FAIL` tag.

Added `-i <run_id_str>` option: if any of the arguments to the program
being run match this string, then it will be replaced with the run ID.

Changed the `-w wait_msec` option to `-m min_runtime_msec`, so that
autoclave can delay to pad run time for very short-lived programs,
but will not slow things down otherwise. (Thanks to Josh Triplett
for the suggestion.)

Added `-k <int>` to send a custom signal on timeout, rather than
`SIGINT`. Also, the default signal was changed to `SIGTERM`.

`run_id` now starts counting at 1 rather than 0.

Added `-I <ints>` to ignore specific non-zero exit statuses.


### Bug Fixes

Fixed the timestamp decimal format width, so it wasn't always preceded
by two extra zeroes.

Fixed some warnings from clang's `scan-build` and building with `-Wextra
-Weverything`.

Use `clock_gettime` with `CLOCK_MONOTONIC` when available.


### Other Improvements

Added this CHANGELOG.

Included run and overall duration in the printouts.

Added printing to `examples/crash_example`, for demonstrating logging.

Print overall counts at exit, even when verbosity is 0 and autoclave
is terminated with `SIGINT`.

Now everything builds into `build/`, except for the generated
documentation formats in `man/` (which are checked in to avoid a
build-time dependency on `ronn`).

Made time domains more explicit.

Several improvements to the documentation, including a new section about
logging and log file paths. (Thanks to Josh Triplett for feedback.)

Fixed typo in the README. (Thanks hugovk.)

Some internal cleanup, code style changes, etc.


## v0.1.1 - 2015-03-26

### API Changes

Added `-s` ("supervise"), an alias for the common case of `-l -e -v`.

Changed wait (`-w`) unit to msec, and changed default to 100 msec.

Set `AUTOCLAVE_STDOUT_LOG` and `AUTOCLAVE_STDERR_LOG` environment
variables for the log paths when in use.


### Bug Fixes

Switched from `execv` to `execvp`, to search the path.

Fixed an incorrect offset into `ARGV`.


### Other Improvements

Simplified log retation.



## v0.1.0 - 2015-01-15

Initial public release.
