#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>

#define MAX_CHAR_PEEK 1024

//Just some example syscalls
//http://blog.rchapman.org/posts/Linux_System_Call_Table_for_x86_64/
enum syscalls {SYS_READ = 0, SYS_WRITE, SYS_OPEN, SYS_CLOSE, SYS_STAT,
                        SYS_FSTAT, SYS_LSTAT, SYS_POLL, SYS_LSEEK, SYS_MMAP,
                        SYS_MPROTECT, SYS_MUNMAP, SYS_BRK, SYS_IOCTL = 16,
                        SYS_ACCESS = 21,
                        SYS_DUP2 = 33,
                        SYS_FCNTL = 72,
                        SYS_CREAT = 85, SYS_OPENAT = 257};


int print_syscall_info(pid_t pid, struct user_regs_struct *regs);
int peek_str(pid_t pid, unsigned long long addr);

int PRINT_BUFFER = 0; //print read/written data

int main(int argc, char **argv)
{
        char **cmd = argv + 1;
        int status = 0;
        int follow_up = 0; //follow up to print result

        pid_t pid = fork();
        switch (pid) {
        case -1:
                perror("fork() error.\n");
                return -1;
        case 0: //child proc
                if (ptrace(PTRACE_TRACEME, pid, 0, 0) < 0) perror("FATAL: ptrace from child"); //this will pause child
                if (execvp(cmd[0], cmd) < 0) perror("exec error");
                break;
        default: //parent
                waitpid(pid, 0, 0); //sync with TRACEME from child
                ptrace(PTRACE_SETOPTIONS, pid, 0, PTRACE_O_EXITKILL); //child process should terminate with parent

                for (;;) {
                        if (ptrace(PTRACE_SYSCALL, pid, 0, 0) < 0) { //continue child, stop right before syscall entry
                                perror("ptrace_syscall from parent"); 
                                break;
                        }
                        waitpid(pid, &status, 0); //wait until child process is ready
                        //[WE CAN INTERCEPT ARGUMENT VALUES HERE]

                        struct user_regs_struct regs;

                        ptrace(PTRACE_GETREGS, pid, 0, &regs);
                        follow_up = print_syscall_info(pid, &regs);

                        /*Execute syscall and stop right before return*/
                        ptrace(PTRACE_SYSCALL, pid, 0, 0);
                        waitpid(pid, &status, 0);

                        ptrace(PTRACE_GETREGS, pid, 0, &regs);
                        if (follow_up) printf("Returned: %d\n", regs.rax);
                        //[WE CAN INTERCEPT RETURN VALUE HERE]
                        
                        if (WIFEXITED(status)) break; //child exited
                }
        }

        return 0;
}


int print_syscall_info(pid_t pid, struct user_regs_struct *regs)
{
        int follow_up = 0; //follow up to print result later, usually used when fd is returned
        switch (regs->orig_rax) {
        case SYS_READ:
                printf("READ %d bytes on fd:%d", regs->rdx, regs->rdi);
                if (PRINT_BUFFER) peek_str(pid, regs->rsi);
                break;
        case SYS_WRITE:
                printf("WRITE %d bytes on fd:%d", regs->rdx, regs->rdi);
                if (PRINT_BUFFER) peek_str(pid, regs->rsi);
                break;
        case SYS_OPEN:
                printf("OPEN syscall on file:");
                peek_str(pid, regs->rdi);
                follow_up = 1;
                break;
        case SYS_CLOSE:
                printf("CLOSE syscall on fd:%d", regs->rdi);
                break;
        case SYS_STAT:
                printf("STAT syscall on file:");
                peek_str(pid, regs->rdi);
                break;
        case SYS_FSTAT:
                printf("FSTAT syscall on fd:%d", regs->rdi);
                break;
        case SYS_LSEEK:
                printf("LSEEK syscall on fd:%d", regs->rdi);
                break;
        case SYS_MMAP:
                printf("MMAP syscall at %ld", regs->rdi);
                break;
        case SYS_MUNMAP:
                printf("MUNMAP syscall at %ld", regs->rdi);
                break;
        case SYS_BRK:
                printf("BRK syscall");
                break;
        case SYS_IOCTL:
                printf("IOCTL syscall on fd:%d, cmd: %d, arg: %ld", regs->rdi, regs->rsi, regs->rdx);
                break;
        case SYS_ACCESS: //21
                printf("ACCESS syscall on file:");
                peek_str(pid, regs->rdi);
                break;
        case SYS_DUP2: //33
                printf("DUP2 syscall, old fd:%d, new fd:%d", regs->rdi, regs->rsi);
                break;
        case SYS_FCNTL: //72
                printf("FCNTL syscall on fd:%d, cmd: %d, arg: %ld", regs->rdi, regs->rsi, regs->rdx);
                break;
        case SYS_CREAT:
                printf("Trying to create file:");
                peek_str(pid, regs->rdi);
                break;
        case SYS_OPENAT:
                printf("OPENAT syscall on file:");
                peek_str(pid, regs->rsi);
                follow_up = 1;
                break;
        default:
                printf("Unhandled syscall: %d", regs->orig_rax);
                break;
        }
        printf("\n");

        return follow_up;
}


/*Print out string at addr in child process*/
int peek_str(pid_t pid, unsigned long long addr)
{
        union u {
                long val;
                char c[sizeof(long)];
        } word;
        int i = 0;
        
        while (i < MAX_CHAR_PEEK) {
                word.val = ptrace(PTRACE_PEEKDATA, pid, addr + i * 4, 0);
                i++;
                for (int j = 0; j < 4; j++) {
                        if (word.c[j] == '\0') return 0;
                        printf("%c", word.c[j]);
                }
        }

        return 0;
}