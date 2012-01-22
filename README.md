# Measure is a program to measure programs... really!

So there's the time command, which gets you basic runtime and CPU, but:

* its output isn't easily parsed
* only measures time (not say memory usage)
* oh and its resolution sucks: "time ls" ... it took _zero_seconds_? really?

Oh and if you're running a non-trivial program, you might need to warm
it up by running it once... or is it twice? How do you know?

There's rumors of some old "timex" thing, but well that seems to have
fallen out of remembering for the most part.

Right so there's this (seemingly) little-used wait4(2) function which
returns _resource_usage_ of a child process.

What measure does is take a command, runs it, and collects:

* high resolution (nanoseconds on Linux) wallclock runtime
* returncode (in case your program is less than deterministic, you just might care)
* resource usage, including
** user and system CPU time
** memory usage (maxrss)
** page faults
** and more! (see getrusage(2) for details)
* stashes the command's stdout and stderr into temporary files

Right so then there's sample, which is just a symlink to measure, which
_keeps_running_ the command until something kills it (like say SIGPIPE
from a |head).


# State of the code

This project is me dusting off my C skills; there're likely bugs and
things that could be done better.

I have yet to dust off my autotools skills, so there's just a dead-stupid
Makefile at this point; it compiles for me on the latest Arch Linux, YMMV.
