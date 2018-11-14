# syscall-trapper
Monitor system calls of an executable program on Linux.

Only support x86 so far, for more info on other architectures, see "man syscall"

- Compile
```
gcc syscall-trapper.c -o syscall-trapper
```

- Run
```
./syscall-trapper <program_name> [program_params]
```
