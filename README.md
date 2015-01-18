autoclave: repeatedly run programs until they break
===================================================

## Summary

autoclave repeatedly executes a command line until its process exits
with a non-zero status, is stopped/terminated by a signal, a
user-specified timeout (-t) runs out, or a user-specified number of runs
(-r) have passed without any failures.

It can log the contents of stdout (-l) and/or stderr (-e), and can
rotate the log files so a fixed number of logs are kept (-c).

On failure, a handler program (-x) can be called with information about
the child process. That way, a debugger can be opened if the stressed
program times out or dumps core.

For more detailed usage info, see the examples below and the man page.


## Examples

Repeatedly run buggy_program until it crashes:

    $ autoclave buggy_program

Same, but print run and failure counts:

    $ autoclave -v buggy_program

Same, but if it succeeds 10 times, return EXIT_SUCCESS:

    $ autoclave -v -r 10 buggy_program

Repeatedly run buggy_program, logging stdout to
`buggy_program.1.stdout.log`, `buggy_program.2.stdout.log`, and so on:

    $ autoclave -l buggy_program

Same, but log to `/tmp/buggy.ID.stdout.log` instead:

    $ autoclave -l -o /tmp/buggy buggy_program

Log, but only keep the 5 most recent log files:

    $ autoclave -l -c 5 buggy_program

Same, but log stderr as well as stdout (keeping 10 files):

    $ autoclave -l -e -c 5 buggy_program

Repeatedly run buggy_program until it has failed 10 times:

    $ autoclave -f 10 buggy_program

Run a program that occasionally deadlocks, halting it and counting it as a
failure if it takes more than 10 seconds to complete:

    $ autoclave -t 10  examples/deadlock_example

Same, but run the script `examples/gdb_it` when it times out, so gdb can
attach to the stopped process and investigate what is deadlocking.
(Note: the process is not halted after the -x command returns.)

    $ autoclave -t 10 -x examples/gdb_it examples/deadlock_example

Run `examples/crash_example`, calling `examples/gdb_it` if it fails:

    $ autoclave -x examples/gdb_it examples/crash_example
