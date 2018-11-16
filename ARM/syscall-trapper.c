#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/ptrace.h>
#include <sys/user.h>
//#include </usr/include/arm-linux-gnueabihf/sys/user.h>


#define MAX_CHAR_PEEK 1024

//Just some example syscalls
//https://w3challs.com/syscalls/?arch=arm_strong
//r7 for syscall, r0 for 1st arg, r0 for retval
enum syscalls {SYS_READ = 3, SYS_WRITE, SYS_OPEN, SYS_CLOSE, SYS_CREAT = 8,
			SYS_LINK, SYS_UNLINK,
			SYS_ACCESS = 33,
			SYS_BRK = 45,
			SYS_IOCTL = 54, SYS_FCNTL,
			SYS_DUP2 = 63,
			SYS_STAT = 106,
                        SYS_OPENAT = 322};


int print_syscall_info(pid_t pid, struct user_regs *regs);
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

                        struct user_regs regs;

                        ptrace(PTRACE_GETREGS, pid, 0, &regs);
                        follow_up = print_syscall_info(pid, &regs);

                        /*Execute syscall and stop right before return*/
                        ptrace(PTRACE_SYSCALL, pid, 0, 0);
                        waitpid(pid, &status, 0);

                        ptrace(PTRACE_GETREGS, pid, 0, &regs);
                        if (follow_up) printf("Returned: %d\n", *(&regs));
                        //[WE CAN INTERCEPT RETURN VALUE HERE]

                        if (WIFEXITED(status)) break; //child exited
                }
        }

        return 0;
}


int print_syscall_info(pid_t pid, struct user_regs *regs)
{
        int follow_up = 0; //follow up to print result later, usually used when fd is returned
	/*each register holds a value of size long, we have 18 registers*/	
	unsigned long r[18] = {0};
	memcpy(r, regs, sizeof(r));
	unsigned long syscall = r[7];

        switch (syscall) {
        case SYS_READ:
                printf("READ %d bytes on fd:%d", r[2], r[0]);
                if (PRINT_BUFFER) peek_str(pid, r[1]);
                break;
        case SYS_WRITE:
                printf("WRITE %d bytes on fd:%d", r[2], r[0]);
                if (PRINT_BUFFER) peek_str(pid, r[1]);
                break;
        case SYS_OPEN:
                printf("OPEN syscall on file:");
                peek_str(pid, r[0]);
                follow_up = 1;
                break;
        case SYS_CLOSE:
                printf("CLOSE syscall on fd:%d", r[0]);
                break;
        case SYS_STAT:
                printf("STAT syscall on file:");
                peek_str(pid, r[0]);
                break;
	case SYS_ACCESS:
		printf("ACCESS syscall on file:");
		peek_str(pid, r[0]);
		break;
	case SYS_DUP2:
		printf("DUP2 syscall, old fd:%d, new fd:%d", r[0], r[1]);
		break;
/*        case SYS_FSTAT:
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
                break;*/
        default:
                printf("Unhandled syscall: %d", syscall);
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
