# shell-lite
A lightweight shell programm for linux.

Features included:
- Handle error situation correctly, e.g., incorrect filename, incorrect path, not a binary file, etc.
- Allow user execute multiple commands (with arguments) one at a time (using &&).
- Handle zombie processes.
- Support | piping
- Support & background execution.
- Handle SIGINT, SIGCHLD, SIGUSR1 with predefined response.
- Support time macro.
- Detection of uncorrect syntax.

# How to compile:
```
gcc main.c -o myshell
```
