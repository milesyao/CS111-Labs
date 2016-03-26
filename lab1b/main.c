//
//  main.c
//  simpsh
//
//  Created by cnyao on 1/9/16.
//  Copyright © 2016 cnyao. All rights reserved.
//

//
//  simpsh.c
//  simpsh
//
//  Created by cnyao on 1/9/16.
//
//
#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>

#define MAXFD 1024 //subject to change
#define MAXJOB 100 //subject to change
#define MAXPARA 100 //max parameters for a job

#define RDONLY 'a'
#define WRONLY 'b'
#define COMMAND 'c'
#define CREAT 'd'
#define TRUNC 'e'
#define APPEND 'f'
#define CLOXEXEC 'g'
#define DIRECTORY 'h'
#define DSYNC 'i'
#define EXCL 'j'
#define NOFOLLOW 'k'
#define NONBLOCK 'l'
#define RSYNC 'm'
#define SYNC 'n'
#define PIPE 'o'
#define WAIT 'p'
#define ABORT 'q'
#define CLOSE 'r'
#define CATCH 's'
#define IGNORE 't'
#define DEFAULT 'u'
#define PAUSE 'v'


static int verbose_flag;
int nextfd = 0; //next file descriptor to allocate
int nextjob = 0;
int maxfd = MAXFD;
int maxjob = MAXJOB;
int maxpara = MAXPARA;
struct file_d {              /* The internal file descriptor */
    int fd;                 /* system level file descriptor */
    int peer;
};

struct job_d {
    int pid;
    int input;
    int output;
    int errorput;
    char** pro_info;
    int paranum;
};

struct file_d *fd_list; /* The file descriptor list */
struct job_d *job_list;

int addfd(struct file_d FileDescriptor);
int addjob(struct job_d newjob);
int judgenumber(const char* str);
void simpsh_handler(int signum);


int main (int argc, char **argv)
{
    int c;
    mode_t mode = O_RDONLY;
    int curpid;
    int fd;
    int k;
    int flag;
    
    fd_list = (struct file_d*)malloc(sizeof(struct file_d) * maxfd);
    job_list = (struct job_d*)malloc(sizeof(struct job_d) * maxjob);
    while (1)
    {
        static struct option long_options[] =
        {
            /* These options set a flag. */
            {"verbose", no_argument, &verbose_flag, 1},
            {"rdonly",  optional_argument, 0, 'a'},
            {"wronly",  optional_argument, 0, 'b'},
            {"command", optional_argument, 0, 'c'},
            {"creat", no_argument, 0, 'd'},
            {"trunc", no_argument, 0, 'e'},
            {"append",no_argument, 0, 'f'},
            {"cloexec",no_argument, 0, 'g'},
            {"directory",no_argument, 0, 'h'},
            {"dsync",no_argument, 0, 'i'},
            {"excl",no_argument, 0, 'j'},
            {"nofollow",no_argument, 0, 'k'},
            {"nonblock",no_argument, 0, 'l'},
            {"rsync",no_argument, 0, 'm'},
            {"sync",no_argument, 0, 'n'},
            {"pipe",no_argument, 0, 'o'},
            {"wait", no_argument, 0, 'p'},
            {"abort", no_argument, 0, 'q'},
            {"close", optional_argument, 0, 'r'},
            {"catch", optional_argument, 0, 's'},
            {"ignore", optional_argument, 0, 't'},
            {"default", optional_argument, 0, 'u'},
            {"pause", no_argument, 0, 'v'},
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        
        c = getopt_long (argc, argv, "",
                         long_options, &option_index);
    
        /* Detect the end of the options. */
        if (c == -1)
            break;
        struct file_d newfd;
        
        switch (c)
        {
            case 0:
                break;
            case WRONLY:
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=2) fprintf(stderr, "Warnning: --wronly: Too many arguments. Use the first one.\n");
                if(k==0) {
                    fprintf(stderr, "--wronly: Require an argument\n");
                    break;
                }
                if(verbose_flag) {
                    printf("--wronly %s\n", argv[optind-1]);
                }
                mode |= O_WRONLY;
                fd = open(argv[optind-1], mode, 0644);
                newfd.fd = fd;
                newfd.peer = -1;
                addfd(newfd);
                mode = O_RDONLY;
                break;
            case RDONLY:
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=2) fprintf(stderr, "Warnning: --rdonly: Too many arguments. Use the first one.\n");
                if(k==0) {
                    fprintf(stderr, "--rdonly: Require an argument\n");
                    break;
                }
                if(verbose_flag) {
                    printf("--rdonly %s\n", argv[optind-1]);
                }
                fd = open(argv[optind-1], mode);
                newfd.fd = fd;
                newfd.peer = -1;
                addfd(newfd);
                if(fd<0) {fprintf(stderr,"--rdonly: %s: No such file\n", argv[optind-1]);}
                mode = O_RDONLY;
                break;
            case COMMAND: //command
                k=0;
                flag = 0;
                struct job_d new_job_info;
                new_job_info.input = 0;
                new_job_info.output = 0;
                new_job_info.errorput = 0;
                new_job_info.pro_info = (char**)malloc(sizeof(char**)*maxpara);
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) {                    if(k==0) {
                        if(judgenumber(argv[optind]) < 0) {
                            fprintf(stderr, "--command: %s: Argument invalid\n", argv[optind]);
                            flag = 1;
                            break;
                        }
                        new_job_info.input = atoi(argv[optind]);
                        if(new_job_info.input<0 || new_job_info.input >=nextfd) {
                            fprintf(stderr, "--command: File descriptor %s out of range\n", argv[optind]);
                            flag = 1;
                            break;
                        }
			if(fd_list[new_job_info.input].fd == -1) {
			    fprintf(stderr, "--command: File %s closed \n", argv[optind]);
			    flag = 1;
			    break;  
			}
                    }
                    if(k==1) {
                        if(judgenumber(argv[optind]) < 0) {
                            fprintf(stderr, "--command: %s: Argument invalid\n", argv[optind]);
                            flag = 1;
                            break;
                        }
                        new_job_info.output = atoi(argv[optind]);
                        if(new_job_info.output<0 || new_job_info.output >=nextfd) {
                            fprintf(stderr, "--command: File descriptor %s out of range\n", argv[optind]);
                            flag = 1;
                            break;
                        }
			if(fd_list[new_job_info.output].fd == -1) {
			    fprintf(stderr, "--command: File %s closed \n", argv[optind]);
			    flag = 1;
			    break;  
			}
                    }
                    if(k==2) {
                        if(judgenumber(argv[optind]) < 0) {
                            fprintf(stderr, "--command: %s: Argument invalid\n", argv[optind]);
                            flag = 1;
                            break;
                        }
                        new_job_info.errorput = atoi(argv[optind]);
                        if(new_job_info.errorput<0 || new_job_info.errorput >=nextfd) {
                            fprintf(stderr, "--command: File descriptor %s out of range\n", argv[optind]);
                            flag = 1;
                            break;
                        }
			if(fd_list[new_job_info.errorput].fd == -1) {
			    fprintf(stderr, "--command: File %s closed \n", argv[optind]);
			    flag = 1;
			    break;  
			}
                    }
                    if(k>=3) {
                        if(k-3>=maxpara-1) {
                            new_job_info.pro_info = (char**)realloc(new_job_info.pro_info, sizeof(char**)*2*maxpara);
                            maxpara = maxpara*2;
                        }
                        new_job_info.pro_info[k-3] = argv[optind];
                        new_job_info.pro_info[k-2] = NULL;
                    }
                    k++;
                }
                if(flag==1) break;
                new_job_info.paranum = k-3;
                if(k<=3) {fprintf(stderr, "--command: Too few arguments\n");break;}
                addjob(new_job_info);
                if(verbose_flag) {
                    printf("--command");
                    optind = optind-k;
                    for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) {
                        printf(" %s", argv[optind]);
                    }
                    printf("\n");
                }
                curpid = fork();
                if(curpid == 0) { //child process
                    dup2(fd_list[new_job_info.input].fd, 0);
                    dup2(fd_list[new_job_info.output].fd, 1);
                    dup2(fd_list[new_job_info.errorput].fd, 2);
                    
                    //we've already redirected fd to 0, 1, 2. So just close all fds other than 0 1 2
                    //to ensure the correctness of pipe
                    for(k=0; k < nextfd; k++) close(fd_list[k].fd);
                    
                    execvp(new_job_info.pro_info[0],new_job_info.pro_info);
                    perror("--command: Execvp failed");
                    exit(-1);
                    break;
                } else { //parent process
                    job_list[nextjob-1].pid = curpid;
                }
                break;
            case WAIT:{
//Wait for all commands to finish. As each finishes, output its exit status, and a copy of the command (with spaces separating arguments) to standard output.
                for(k = 0; k < nextfd; k++) {
                    close(fd_list[k].fd);
                }
                int status;
                int returnpid;
                int j, m;
                if(verbose_flag) printf("--wait\n");
                for(k = 0; k < nextjob; k++) {
                    returnpid =  waitpid(-1, &status, 0);
                    if(WIFEXITED(status))
                        printf("Process exit: %d \n", WEXITSTATUS(status));
                    else
                        printf("Process doesn't terminate normally!\n");
                    for(j=0; j < nextjob; j++) {
                        if(job_list[j].pid == returnpid) {
                            printf("--command %d %d %d ", job_list[j].input, job_list[j].output, job_list[j].errorput);
                            for(m=0; m < job_list[j].paranum; m++) {
                                printf("%s ", job_list[j].pro_info[m]);
                            }
                            printf("\n");
                            break;
                        }
                    }
                }
                break;
            }
            case CREAT: //creat
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --creat: Too many arguments.\n");
                if(verbose_flag) printf("--creat ");
                mode |= O_CREAT;
                break;
            case TRUNC: //trunc
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --trunc: Too many arguments.\n");
                if(verbose_flag) printf("--trunc ");
                mode |= O_TRUNC;
                break;
            case APPEND: //append
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --append: Too many arguments.\n");
                if(verbose_flag) printf("--append ");
                mode |= O_APPEND;
                break;
            case CLOXEXEC: //cloxexec
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --cloxexec: Too many arguments.\n");
                if(verbose_flag) printf("--cloxexec\n");
                mode |= O_CLOEXEC;
                break;
            case DIRECTORY: //directory
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --directory: Too many arguments.\n");
                if(verbose_flag) printf("--directory\n");
                mode |= O_DIRECTORY;
                break;
            case DSYNC: //dsync
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --dsync: Too many arguments.\n");
                if(verbose_flag) printf("--dsync\n");
                mode |= O_DSYNC;
                break;
            case EXCL: //excl
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --excl: Too many arguments.\n");
                if(verbose_flag) printf("--excl\n");
                mode |= O_EXCL;
                break;
            case NOFOLLOW: //nofollow
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --nofollow: Too many arguments.\n");
                if(verbose_flag) printf("--nofollow\n");
                mode |= O_NOFOLLOW;
                break;
            case NONBLOCK: //nonblock
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --nonblock: Too many arguments.\n");
                if(verbose_flag) printf("--nonblock\n");
                mode |= O_NONBLOCK;
                break;
            case RSYNC: //rsync
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --rsync: Too many arguments.\n");
                if(verbose_flag) printf("--rsync\n");
                  mode |= O_RSYNC;
//                    mode = 0;
                break;
            case SYNC: //sync
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --sync: Too many arguments.\n");
                if(verbose_flag) printf("--sync\n");
                mode |= O_SYNC;
                break;
            case PIPE:{
                if(verbose_flag) printf("--pipe\n");
                int fd[2];
                pipe(fd);
                newfd.fd = fd[0];
                newfd.peer = fd[1];
//                printf("pipe is %d, %d \n", fd[1], fd[0]);
                addfd(newfd);
                newfd.fd = fd[1];
                newfd.peer = fd[0];
                addfd(newfd);
                break;
            }
            case ABORT: {
                if(verbose_flag) printf("--abort\n");
                raise(11);
                break;
//                int *a = NULL;
//                int b = *a;
            }
            case CLOSE: {
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=2) fprintf(stderr, "Warnning: --close: Too many arguments. Use the first one.\n");
                if(k==0) {
                    fprintf(stderr, "--close: Require an argument\n");
                    break;
                }
                if(judgenumber(argv[optind-1]) < 0) {
                    fprintf(stderr, "--close: %s: Argument invalid\n", argv[optind-1]);
                    break;
                }
                if(verbose_flag) {
                    printf("--close %s\n", argv[optind-1]);
                }
		int file_number = atoi(argv[optind-1]);
                close(fd_list[file_number].fd);
                fd_list[file_number].fd = -1;
		break;
            }
            
            case CATCH: {
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=2) fprintf(stderr, "Warnning: --catch: Too many arguments. Use the first one.\n");
                if(k==0) {
                    fprintf(stderr, "--catch: Require an argument\n");
                    break;
                }
                if(judgenumber(argv[optind-1]) < 0) {
                    fprintf(stderr, "--catch: %s: Argument invalid\n", argv[optind-1]);
                    break;
                }
                if(verbose_flag) {
                    printf("--catch %s\n", argv[optind-1]);
                }
                signal(atoi(argv[optind-1]), simpsh_handler);
                break;
            }
            case IGNORE: {
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=2) fprintf(stderr, "Warnning: --ignore: Too many arguments. Use the first one.\n");
                if(k==0) {
                    fprintf(stderr, "--ignore: Require an argument\n");
                    break;
                }
                if(judgenumber(argv[optind-1]) < 0) {
                    fprintf(stderr, "--ignore: %s: Argument invalid\n", argv[optind-1]);
                    break;
                }
                if(verbose_flag) {
                    printf("--ignore %s\n", argv[optind-1]);
                }
                signal(atoi(argv[optind-1]), SIG_IGN);
                break;
            }
            case DEFAULT: {
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=2) fprintf(stderr, "Warnning: --default: Too many arguments. Use the first one.\n");
                if(k==0) {
                    fprintf(stderr, "--default: Require an argument\n");
                    break;
                }
                if(judgenumber(argv[optind-1]) < 0) {
                    fprintf(stderr, "--default: %s: Argument invalid\n", argv[optind-1]);
                    break;
                }
                if(verbose_flag) {
                    printf("--default %s\n", argv[optind-1]);
                }
                signal(atoi(argv[optind-1]), SIG_DFL);
                break;
            }
            case PAUSE: {
                if(verbose_flag) printf("--pause\n");
                pause();
                break;
            }
            case '?':
                if (optopt == 'c' || optopt == 'r' || optopt == 'w')
                    fprintf (stderr, "Option -%c requires an argument.\n", optopt);
                else {
                    fprintf(stderr, "Invalid option.\n");
                }
                break;
            default:
                abort ();
        }
    }
    
    
    exit (0);
}

int addfd(struct file_d FileDescripter) {
    if(FileDescripter.fd<0) return 0;
    if(nextfd>=maxfd) {
        fd_list = (struct file_d*)realloc(fd_list, maxfd*2*sizeof(struct job_d));
        maxfd = maxfd*2;
    }
    fd_list[nextfd++] = FileDescripter;
    return 1;
}

int addjob(struct job_d newjob) {
    if(nextjob>maxjob) {
        job_list = (struct job_d*)realloc(job_list, maxjob*2*sizeof(struct job_d));
        maxjob = maxjob*2;
    }
    job_list[nextjob++] = newjob;
    return 1;
}

int judgenumber(const char* str) {
    while(*str!='\0') {
        if(!(*str>='0' && *str<='9')) return -1;
        str++;
    }
    return 0;
}

void simpsh_handler(int signum) {
    fprintf(stderr, "%d caught\n", signum);
    exit(signum);
}
