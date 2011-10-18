/*
 * myftpd.c
 * author: B. Jordan
 *
 * This file implements the myftp daemon. This includes the server implementation
 * of the protocol specified in myftp.pdf
 */

#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h> /* strlen(), strcmp() etc */
#include <errno.h> /* extern int errno, EINTR, perror() */
#include <signal.h> /* SIGCHLD, sigaction() */
#include <sys/types.h> /* pid_t, u_long, u_short */
#include <sys/socket.h> /* struct sockaddr, socket(), etc */
#include <sys/wait.h> /* waitpid(), WNOHAND */
#include <netinet/in.h> /* struct sockaddr_in, htons(), htonl(), and INADDR_ANY */
#include <dirent.h>
#include "stream.h" /* MAX_BLOCK_SIZE, readn(), writen() */
#include "types.h"

#define SERV_TCP_PORT 40903 /* server port no */

void PWD_handler(int sd, char *buf);
void CD_handler(int sd, char *buf);
void DIR_handler(int sd, char *buf);
void GET_handler_1(int sd, char *buf);
void GET_handler_2(int sd, char *buf);
void PUT_handler(int sd, char *buf);

void claim_children() {
    pid_t pid = 1;
    while (pid > 0) { /* claim as many zombies as we can */
        pid = waitpid(0, (int *) 0, WNOHANG);
    }
}

void daemon_init(void) {
    pid_t pid;
    struct sigaction act;

    if ((pid = fork()) < 0) {
        perror("fork");
        exit(1);
    } else if (pid > 0)
        exit(0); /* exit parent */

    /* child continues */
    setsid(); /* become session leader */
    umask(0); /* clear file mode creation mask */

    /* catch SIGCHLD to remove zombies from system */
    act.sa_handler = claim_children; /* use reliable signal */
    sigemptyset(&act.sa_mask); /* not to block other signals */
    act.sa_flags = SA_NOCLDSTOP; /* not catch stopped children */
    sigaction(SIGCHLD, (struct sigaction *) & act, (struct sigaction *) 0);
}

void PWD_handler(int sd, char *buf) {

    int len, nr;
    char status;
    char path[MAX_BLOCK_SIZE];

    buf[0] = PWD_1;

    getcwd(path, MAX_BLOCK_SIZE);
    nr = strlen(path);
    len = htons(nr);
    bcopy(&len, &buf[1], 2);

    if (len > 0)
        status = PWD_STATUS_OKAY;
    else
        status = PWD_STATUS_ERROR;

    writen(sd, &buf[0], 1);
    writen(sd, &buf[1], 2);
    writen(sd, &status, 1);
    writen(sd, path, nr);
}

void CD_handler(int sd, char *buf) {

    int nr;
    char status;
    int chdirstat = 0;

    chdirstat = chdir(buf);

    if (chdirstat == 0)
        status = CD_STATUS_OKAY;
    else
        status = CD_STATUS_ERROR;

    buf[0] = CD_1;

    writen(sd, &buf[0], 1);
    writen(sd, &status, 1);
}

void GET_handler_1(int sd, char *buf) {

    int nr;
    char status;

    status = GET_STATUS_READY;

    buf[0] = GET_1;

    writen(sd, &buf[0], 1);
    writen(sd, &status, 1);
}

void GET_handler_2(int sd, char *buf) {

    int nr, nw, fd, fsize = 0, len, cw = 0;
    char status;
    char fname[MAX_BLOCK_SIZE];

    strcpy(fname, "./");
    strcpy(fname, buf);

    if ((fd = open(fname, O_RDONLY, 0766)) == -1) {

        if ((errno == EACCES) || (errno = EINVAL))
            status = GET_STATUS_PERM_ERROR;
        else if (errno == ENOENT)
            status = GET_STATUS_READ_ERROR;
        else
            status = GET_STATUS_OTHER_ERROR;

    } else {

        status = GET_STATUS_READY;

        /* get file size */
        fsize = lseek(fd, 0, SEEK_END);
        lseek(fd, 0, SEEK_SET);

        len = htonl(fsize);
        bcopy(&len, &buf[1], 4);
    }

    /* write opcode */
    buf[0] = GET_2;
    writen(sd, &buf[0], 1);

    /* write status */
    writen(sd, &status, 1);

    if (status == GET_STATUS_READY) {

        /* write length */
        writen(sd, &buf[1], 4);

        /**
         * this next section writes the file stream to the scoket.
         */
        char chunk[MAX_BLOCK_SIZE];

        /* initialize chunk memory */
        memset(chunk, '\0', MAX_BLOCK_SIZE);

        /* zero offset */
        lseek(fd, 0, SEEK_SET);

        /* read the file into the first block and write it to socket */
        nr = read(fd, chunk, MAX_BLOCK_SIZE);
        nw = writen(sd, chunk, MAX_BLOCK_SIZE);

        /* add bytes written to socket to total bytes written */
        cw += nw;

        while (cw < fsize) {
            /* reinitialize chunk memory to be safe */
            memset(chunk, '\0', MAX_BLOCK_SIZE);

            /* move offset to start of next chunk */
            lseek(fd, cw, SEEK_SET);

            /* read in the next chunk */
            if ((fsize - cw) > MAX_BLOCK_SIZE) {
                nr = read(fd, chunk, MAX_BLOCK_SIZE);
            } else {
                nr = read(fd, chunk, (fsize - cw));
            }

            /* write the current chunk to socket */
            writen(sd, chunk, MAX_BLOCK_SIZE);

            /* aggregate bytes written */
            cw += nr;
        }
    }

    close(fd);
}

void PUT_handler(int sd, char *buf) {

    int fd, nr = 0, len = 0, fsize = 0, cr = 0, nw = 0;
    char status;
    char fname[MAX_BLOCK_SIZE];
    
    memcpy(fname, buf, MAX_BLOCK_SIZE);
    memset(buf, '\0', MAX_BLOCK_SIZE);

    /* check if the file already exists */
    if (access(buf, R_OK) == 0)
        status = PUT_STATUS_WRITE_ERROR;
    else
        status = PUT_STATUS_READY;

    /* write opcode */
    buf[0] = PUT_1;
    writen(sd, &buf[0], 1);

    /* write status */
    writen(sd, &status, 1);

    if (status == PUT_STATUS_READY) {

        memset(buf, '\0', MAX_BLOCK_SIZE);

        /* read opcode */
        readn(sd, buf, MAX_BLOCK_SIZE);

        /* read length */
        readn(sd, &buf[1], MAX_BLOCK_SIZE);
        bcopy(&buf[1], &fsize, 4);
        len = ntohl(fsize);

        /**
         * this next part reads the file in chunks.
         */

        /* create a new file */
        if ((fd = open(fname, O_WRONLY | O_CREAT | O_TRUNC, 0666)) != -1) {

            /* allocate and initialize chunk memory */
            char chunk[MAX_BLOCK_SIZE];
            memset(chunk, '\0', MAX_BLOCK_SIZE);
            lseek(fd, 0, SEEK_SET);

            /* read the first block */
            nr = readn(sd, chunk, MAX_BLOCK_SIZE);

            /* if file size is below max block size */
            if (len < MAX_BLOCK_SIZE)
                nw = write(fd, chunk, len); /* write the used portion of the chunk to file */
            else {
                /* otherwise, write the maximum block size */
                nw = write(fd, chunk, MAX_BLOCK_SIZE);

                /* add the chunks written to file to counter */
                cr += nw;

                /* keep reading chunks until the entire file has been read */
                while (cr < len) {

                    /* reinitialize the chunk memory to be safe */
                    memset(chunk, '\0', MAX_BLOCK_SIZE);

                    /* move the file offset to the start of the next block */
                    lseek(fd, cr, SEEK_SET);

                    /* read the next block */
                    nr = readn(sd, chunk, MAX_BLOCK_SIZE);

                    /* write the block to file */
                    if ((len - cr) < MAX_BLOCK_SIZE)
                        nw = write(fd, chunk, (len - cr));
                    else
                        nw = write(fd, chunk, MAX_BLOCK_SIZE);

                    /* aggregate the chunks written to file */
                    cr += nw;
                }
            }
        }
    }
}

void DIR_handler(int sd, char *buf) {

    int len, nr, j = 0;
    char status;
    char files[MAX_BLOCK_SIZE];
    char tmp[MAX_BLOCK_SIZE];
    DIR *dp;
    struct dirent *dirp;

    bzero(files, sizeof (files));

    buf[0] = DIR_1;

    if ((dp = opendir(".")) == NULL)
        return;

    while ((dirp = readdir(dp)) != NULL) {

        j++;

        strcpy(tmp, dirp->d_name);

        if (tmp[0] != '.') {
            if (j != 1)
                strcat(files, "\n\t");
            else
                strcat(files, "\t");

            strcat(files, dirp->d_name);
        }
    }

    nr = strlen(files);
    len = htons(nr);
    bcopy(&len, &buf[1], 2);

    if (nr != 0)
        status = DIR_STATUS_OKAY;
    else
        status = DIR_STATUS_ERROR;

    writen(sd, &buf[0], 1);
    writen(sd, &buf[1], 2);
    writen(sd, &status, 1);
    writen(sd, files, nr);
}

void serve_a_client(int sd) {
    int nr, nw, len, i;
    char buf[MAX_BLOCK_SIZE];
    char path[MAX_BLOCK_SIZE];
    char filename[MAX_BLOCK_SIZE];

    while (1) {

        bzero(buf, sizeof (buf));
        bzero(path, sizeof (path));
        bzero(filename, sizeof (path));

        /* read data from client */
        if ((nr = readn(sd, buf, sizeof (buf))) <= 0) {
            return; /* connection broken down */
        }

        if (buf[0] == PWD_1)
            PWD_handler(sd, buf);
        else if (buf[0] == DIR_1)
            DIR_handler(sd, buf);
        else if (buf[0] == CD_1) {
            readn(sd, &buf[1], sizeof (buf));
            bcopy(&buf[1], &len, 2);
            len = (int) ntohs(len);

            /* Read pwd argument. */
            readn(sd, &buf[3], sizeof (buf));

            for (i = 0; i < len; i++) {
                path[i] = buf[i + 3];
            }

            CD_handler(sd, path);
        } else if (buf[0] == GET_1) {
            readn(sd, &buf[1], sizeof (buf));
            bcopy(&buf[1], &len, 2);
            len = (int) ntohs(len);

            /* Read the filename. */
            readn(sd, &buf[3], sizeof (buf));

            for (i = 0; i < len; i++) {
                filename[i] = buf[i + 3];
            }

            GET_handler_1(sd, filename);
        } else if (buf[0] == GET_2) {
            readn(sd, &buf[1], sizeof (buf));
            bcopy(&buf[1], &len, 2);
            len = (int) ntohs(len);

            /* Read the filename. */
            readn(sd, &buf[3], sizeof (buf));

            for (i = 0; i < len; i++) {
                filename[i] = buf[i + 3];
            }

            GET_handler_2(sd, filename);
        } else if (buf[0] == PUT_1) {
            readn(sd, &buf[1], sizeof (buf));
            bcopy(&buf[1], &len, 2);
            len = (int) ntohs(len);

            /* Read the filename. */
            readn(sd, &buf[3], sizeof (buf));

            for (i = 0; i < len; i++) {
                filename[i] = buf[i + 3];
            }

            PUT_handler(sd, filename);
        }
    }
}

main(int argc, char *argv[]) {

    int sd, nsd, n, cli_addrlen;
    pid_t pid;
    struct sockaddr_in ser_addr, cli_addr;

    /* handle command line parameters */
    if (argc > 2) {
        printf("usage: myftpd [ initial_current_directory ]\n");
    } else if (argc == 2) {
        if (chdir(argv[1]) != 0)
            exit(1);
    }
    
    /* turn the program into a daemon */
    daemon_init();

    /* set up listening socket sd */
    if ((sd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("server:socket");
        exit(1);
    }

    /* build server Internet socket address */
    bzero((char *) & ser_addr, sizeof (ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(SERV_TCP_PORT);
    ser_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    /**
     * note: accept client request sent to any one of the
     * network interface(s) on this host.
     */

    /* bind server address to socket sd */
    if (bind(sd, (struct sockaddr *) & ser_addr, sizeof (ser_addr)) < 0) {
        perror("server bind");
        exit(1);
    }

    /* become a listening socket */
    listen(sd, 5);

    while (1) {

        /* wait to accept a client request for connection */
        cli_addrlen = sizeof (cli_addr);
        nsd = accept(sd, (struct sockaddr *) & cli_addr, &cli_addrlen);
        if (nsd < 0) {
            if (errno == EINTR) /* if interrupted by SIGCHLD */
                continue;
            perror("server:accept");
            exit(1);
        }

        /* create a child process to handle this client */
        if ((pid = fork()) < 0) {
            perror("fork");
            exit(1);
        } else if (pid > 0) {
            close(nsd);
            continue; /* parent to wait for next client */
        }

        /* now in child, serve the current client */
        close(sd);
        serve_a_client(nsd);
        exit(0);
    }
}

