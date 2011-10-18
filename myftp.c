/*
 * myftp.c
 * author: B. Jordan
 *
 * This file implements the myftp client. This includes the client implementation
 * of the protocol specified in myftp.pdf
 */

#include <dirent.h>
#include <stdlib.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>     /* struct sockaddr_in, htons, htonl */
#include <netdb.h>          /* struct hostent, gethostbyname() */
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "stream.h"         /* MAX_BLOCK_SIZE, readn(), writen() */
#include "types.h"
#include "token.h"

#define SERV_TCP_PORT 40903 /* Server's "well-known" port number */

main(int argc, char *argv[]) {

    int sd, n, nr, nw, len, tk = 0, i, j = 0;
    char buf[MAX_BLOCK_SIZE];
    char host[60];
    char path[MAX_BLOCK_SIZE];
    char files[MAX_BLOCK_SIZE];
    char tmp[MAX_BLOCK_SIZE];
    char filename[MAX_BLOCK_SIZE];
    char *tokens[MAX_TOKENS];
    struct sockaddr_in ser_addr;
    struct hostent *hp;

    /* Get the server host name. */
    if (argc == 1) /* Assume server is running on the local host. */
        gethostname(host, sizeof (host));
    else if (argc == 2) /* Use the given host name. */
        strcpy(host, argv[1]);
    else {
        printf("Usage: %s [ <server_host_name> [ <server_port> ] ] \n", argv[0]);
        exit(1);
    }

    /* Get the host address and build a server socket address. */
    bzero((char *) & ser_addr, sizeof (ser_addr));
    ser_addr.sin_family = AF_INET;
    ser_addr.sin_port = htons(SERV_TCP_PORT);

    if ((hp = gethostbyname(host)) == NULL) {
        printf("host %s not found\n", host);
        exit(1);
    }

    ser_addr.sin_addr.s_addr = *(u_long *) hp->h_addr;

    /* Create TCP socket and connect socket to server address. */
    sd = socket(PF_INET, SOCK_STREAM, 0);

    if (connect(sd, (struct sockaddr *) & ser_addr, sizeof (ser_addr)) < 0) {
        perror("client connect");
        exit(1);
    }

    while (++i) {
        printf("myftp[%d]: ", i);
        fgets(buf, sizeof (buf), stdin);
        nr = strlen(buf);
        if (buf[nr - 1] == '\n') {
            buf[nr - 1] = '\0';
            --nr;
        }

        memset(tmp, 0x0, MAX_BLOCK_SIZE);
        memcpy(tmp, buf, MAX_BLOCK_SIZE);

        /* Tokenize user input. */
        tk = tokenise(tmp, tokens);

        if (strcmp(buf, "quit") == 0) {
            printf("client disconnect.\n");
            exit(0);
        }

        if (strcmp(buf, "pwd") == 0) {

            bzero(buf, sizeof (buf));
            bzero(path, sizeof (path));

            /* Set the opcode. */
            buf[0] = PWD_1;
            nw = writen(sd, buf, 1);

            /* Read the return opcode. */
            nr = readn(sd, &buf[0], sizeof (buf));

            if (buf[0] == PWD_1) {

                nr = readn(sd, &buf[1], sizeof (buf));
                bcopy(&buf[1], &len, 2);
                len = (int) ntohs(len);

                /* Read status code. */
                char status;
                nr = readn(sd, &buf[3], sizeof (buf));

                if (buf[3] == PWD_STATUS_OKAY) {

                    nr = readn(sd, path, sizeof (buf));
                    printf("\t%s\n", path);

                } else
                    printf("\tpwd returned with status PWD_STATUS_ERROR.\n");
            }

        } else if (strncmp(buf, "cd ", 3) == 0) {

            if (tk == 2) {

                bzero(buf, sizeof (buf));

                buf[0] = CD_1;
                nw = writen(sd, buf, 1);

                nw = strlen(tokens[1]);
                len = htons(nw);
                bcopy(&len, &buf[1], 2);

                writen(sd, &buf[1], 2);
                writen(sd, tokens[1], nw);

                readn(sd, &buf[0], sizeof (buf));

                if (buf[0] == CD_1) {
                    readn(sd, &buf[1], sizeof (buf));

                    if (buf[1] == CD_STATUS_OKAY)
                        printf("\tnavigated directory successfully.\n");
                    else
                        printf("\tcd returned with status CD_STATUS_ERROR. check path: %s \n", tokens[1]);
                }

            } else
                printf("\tmalformed command. correct syntax is: cd <path>\n");

        } else if (strncmp(buf, "dir", 3) == 0) {

            bzero(buf, sizeof (buf));
            bzero(files, sizeof (files));

            buf[0] = DIR_1;
            nw = writen(sd, buf, 1);

            nr = readn(sd, &buf[0], sizeof (buf));

            if (buf[0] == DIR_1) {

                nr = readn(sd, &buf[1], sizeof (buf));
                bcopy(&buf[1], &len, 2);
                len = (int) ntohs(len);

                char status;
                nr = readn(sd, &buf[3], sizeof (buf));

                if (buf[3] == DIR_STATUS_OKAY) {

                    nr = readn(sd, files, sizeof (buf));
                    printf("%s\n\n", files);

                } else
                    printf("\tdir returned with status DIR_STATUS_ERROR.\n");
            }
        } else if (strncmp(buf, "get", 3) == 0) {

            if (tk == 2) {
                int fsize, fd, cr = 0;
                char chunk[MAX_BLOCK_SIZE];

                bzero(buf, sizeof (buf));

                buf[0] = GET_1;
                nw = writen(sd, buf, 1);

                nw = strlen(tokens[1]);
                len = htons(nw);
                bcopy(&len, &buf[1], 2);

                writen(sd, &buf[1], 2);
                writen(sd, tokens[1], nw);

                readn(sd, &buf[0], sizeof (buf));

                if (buf[0] == GET_1) {
                    readn(sd, &buf[1], sizeof (buf));

                    if (buf[1] == GET_STATUS_READ_ERROR)
                        printf("\tget returned with status GET_STATUS_READ_ERROR. check file %s.\n", tokens[1]);
                    else if (buf[1] == GET_STATUS_PERM_ERROR)
                        printf("\tget returned with status GET_STATUS_PERM_ERROR. check permissions.\n");
                    else if (buf[1] == GET_STATUS_OTHER_ERROR)
                        printf("\tget returned with status GET_STATUS_OTHER_ERROR.\n");

                    if (buf[1] == GET_STATUS_READY) {
                        bzero(buf, sizeof (buf));

                        /* write opcode */
                        buf[0] = GET_2;
                        writen(sd, &buf[0], 1);

                        /* write filename length */
                        bcopy(&len, &buf[1], 2);
                        writen(sd, &buf[1], 2);

                        /* write filename */
                        writen(sd, tokens[1], nw);

                        /* clear the buffer for next message */
                        bzero(buf, sizeof (buf));

                        /* read opcode */
                        readn(sd, &buf[0], sizeof (buf));

                        if (buf[0] == GET_2) {

                            /* read status */
                            readn(sd, &buf[1], sizeof (buf));

                            if (buf[1] == GET_STATUS_READY) {

                                /* read length */
                                readn(sd, &buf[2], sizeof (buf));
                                bcopy(&buf[2], &fsize, 4);
                                len = ntohl(fsize);

                                /* create a new file */
                                if ((fd = open(tokens[1], O_WRONLY | O_CREAT | O_TRUNC, 0666)) != -1) {

                                    /* allocate and initialize chunk memory */
                                    char chunk[MAX_BLOCK_SIZE];
                                    lseek(fd, 0, SEEK_SET);

                                    /* read the first block */
                                    nr = readn(sd, chunk, MAX_BLOCK_SIZE);

                                    /* if file size is below max block size */
                                    if (len < MAX_BLOCK_SIZE)
                                        /* write the used portion of the chunk to file */
                                        nw = write(fd, chunk, len);
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

                                    printf("\tfile saved.\n");

                                } else
                                    printf("\terror creating new file.\n");


                            } else
                                printf("\terror getting file.\n");
                        }
                    }

                }
            } else
                printf("\tmalformed command. correct syntax is: get <filename>\n");
        } else if (strncmp(buf, "put", 3) == 0) {
            if (tk == 2) {
                int fsize, fd, cw = 0, nw;
                char *newf;
                char chunk[MAX_BLOCK_SIZE];

                memset(buf, '\0', MAX_BLOCK_SIZE);

                if ((fd = open(tokens[1], O_RDONLY, 0766)) == -1) {
                    printf("\tfile does not exist in local path.\n");
                } else {
                    /* write opcode */
                    buf[0] = PUT_1;
                    nw = writen(sd, buf, 1);

                    /* write filename length */
                    nw = strlen(tokens[1]);
                    len = htons(nw);
                    bcopy(&len, &buf[1], 2);
                    writen(sd, &buf[1], 2);

                    /* write filename */
                    writen(sd, tokens[1], nw);

                    /* clear buffer for the server response */
                    memset(buf, '\0', MAX_BLOCK_SIZE);

                    /* read the return opcode */
                    readn(sd, &buf[0], sizeof (buf));

                    /* read status */
                    readn(sd, &buf[1], sizeof (buf));

                    if (buf[1] == PUT_STATUS_READY) {

                        /* write opcode */
                        buf[0] = PUT_2;
                        nw = writen(sd, buf, 1);

                        /* write length */
                        fsize = lseek(fd, 0, SEEK_END);
                        lseek(fd, 0, SEEK_SET);
                        len = htonl(fsize);
                        bcopy(&len, &buf[1], 4);
                        nw = writen(sd, &buf[1], 4);

                        /**
                         * this next part writes the file stream.
                         */

                        /* set aside a block of memory to buffer chunks */
                        char chunk[MAX_BLOCK_SIZE];

                        /* initialize chunk memory */
                        memset(chunk, '\0', MAX_BLOCK_SIZE);

                        /* set zero offset */
                        lseek(fd, 0, SEEK_SET);

                        /* read the file into the first block and write it to socket */
                        nr = read(fd, chunk, MAX_BLOCK_SIZE);
                        nw = writen(sd, chunk, MAX_BLOCK_SIZE);

                        /* add bytes written to the total bytes written */
                        cw += nw;

                        /* keep reading chunks until file entire file has been read */
                        while (cw < fsize) {
                            /* reinitialize chunk */
                            memset(chunk, '\0', MAX_BLOCK_SIZE);

                            /* move the offset to start of next chunk */
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
                        
                        printf("\tokay to write file.\n");

                    } else
                        printf("\tfile already exists in server path.\n");
                }
            }
        } else if (strcmp(buf, "lpwd") == 0) {
            bzero(buf, MAX_BLOCK_SIZE);
            getcwd(buf, MAX_BLOCK_SIZE);
            printf("\t%s\n", buf);

        } else if (strncmp(buf, "lcd", 3) == 0) {
            if (tk == 2) {
                if (chdir(tokens[1]) != 0) {
                    printf("\tinvalid directory specified.\n");
                }
            } else {
                printf("\tmalformed command. correct syntax is: lcd <path>\n");
            }

        } else if (strncmp(buf, "ldir", 4) == 0) {

            DIR *dp; // directory pointer
            struct dirent *dirp; // directory structure
            int fc = 0; // file count

            memset(buf, '\0', MAX_BLOCK_SIZE);
            memset(tmp, '\0', MAX_BLOCK_SIZE);
            memset(files, '\0', MAX_BLOCK_SIZE);

            /* return the directory pointer of the current path */
            if ((dp = opendir(".")) == NULL)
                return;

            /* built a string with the name of each file, one per line */
            while ((dirp = readdir(dp)) != NULL) {

                fc++;

                strcpy(tmp, dirp->d_name);

                if (tmp[0] != '.') {

                    if (fc != 1)
                        strcat(files, "\n\t");
                    else
                        strcat(files, "\t");

                    strcat(files, dirp->d_name);
                }
            }

            /* output the string containing file names */
            printf("%s\n\n", files);
        }
    }
}
