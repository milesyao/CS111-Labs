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


static int verbose_flag;
int nextfd = 0; //next file descriptor to allocate
int nextjob = 0;
int maxfd = MAXFD;
int maxjob = MAXJOB;
int maxpara = MAXPARA;
struct file_d {              /* The internal file descriptor */
    int fd;                 /* system level file descriptor */
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

int addfd(int fd);
int addjob(struct job_d newjob);
int judgenumber(const char* str);


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
            {0, 0, 0, 0}
        };
        /* getopt_long stores the option index here. */
        int option_index = 0;
        
        c = getopt_long (argc, argv, "",
                         long_options, &option_index);
    
        /* Detect the end of the options. */
        if (c == -1)
            break;
        
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
                addfd(fd);
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
                if(fd<0) {fprintf(stderr,"--rdonly: %s: No such file\n", argv[optind-1]);}
                addfd(fd);
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
                    execvp(new_job_info.pro_info[0],new_job_info.pro_info);
                    perror("--command: Execvp failed");
                    exit(-1);
                    break;
                } else { //parent process
                    job_list[nextjob-1].pid = curpid;
                }
                break;
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
                break;
            case SYNC: //sync
                k=0;
                for( ; optind<argc && !(*argv[optind] == '-' && *(argv[optind]+1) == '-'); optind++) k++;
                if(k>=1) fprintf(stderr, "Warnning: --sync: Too many arguments.\n");
                if(verbose_flag) printf("--sync\n");
                mode |= O_SYNC;
                break;
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

int addfd(int fd) {
    if(fd<0) return 0;
    if(nextfd>=maxfd) {
        fd_list = (struct file_d*)realloc(fd_list, maxfd*2*sizeof(struct job_d));
        maxfd = maxfd*2;
    }
    struct file_d new_file_descriptor = {fd};
    fd_list[nextfd++] = new_file_descriptor;
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
