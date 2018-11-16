# syscall-trapper
Monitor system calls of an executable program on Linux.

Only support x86-64 and Arm64 so far, for more info on other architectures, see "man syscall"

- Compile
```
make
```

- Run
```
./syscall-trapper <program_name> [program_params]
```
