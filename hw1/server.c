#include <unistd.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <ctype.h>

#define ERR_EXIT(a) do { perror(a); exit(1); } while(0)

#define OBJ_NUM 3

#define FOOD_INDEX 0
#define CONCERT_INDEX 1
#define ELECS_INDEX 2
#define RECORD_PATH "./bookingRecord"

static char* obj_names[OBJ_NUM] = {"Food", "Concert", "Electronics"};

typedef struct {
    char hostname[512];  // server's hostname
    unsigned short port;  // port to listen
    int listen_fd;  // fd to wait for a new connection
} server;

typedef struct {
    int id;          // 902001-902020
    int bookingState[OBJ_NUM]; // array of booking number of each object (0 or positive numbers)
}record;

typedef struct {
    char host[512];  // client's host
    int conn_fd;  // fd to talk with client
    char buf[512];  // data sent by/to client
    size_t buf_len;  // bytes used by buf
    int id;
    int wait_for_write;  // used by handle_read to know if the header is read or not.
    int wait_for_read;
    record Record;
} request;

server svr;  // server
request* requestP = NULL;  // point to a list of requests
int maxfd;  // size of open file descriptor table, size of request list

const char* accept_read_header = "ACCEPT_FROM_READ";
const char* accept_write_header = "ACCEPT_FROM_WRITE";
const char* operation_failed_header = "[Error] Operation failed. Please try again.\n";
const char* locked_header = "Locked.\n";
const char* over_items = "[Error] Sorry, but you cannot book more than 15 items in total.\n";
const char* less_items = "[Error] Sorry, but you cannot book less than 0 items.\n";
const unsigned char IAC_IP[3] = "\xff\xf4";

static void init_server(unsigned short port);
// initailize a server, exit for error

static void init_request(request* reqP);
// initailize a request instance

static void free_request(request* reqP);
// free resources used by a request instance

int handle_read(request* reqP) {
    /*  Return value:
     *      1: read successfully
     *      0: read EOF (client down)
     *     -1: read failed
     */
    int r;
    char buf[512];
    memset(buf, 0, strlen(buf));
    // Read in request from client
    r = read(reqP->conn_fd, buf, sizeof(buf));
    if (r < 0) return -1;
    if (r == 0) return 0;
    char* p1 = strstr(buf, "\015\012");
    int newline_len = 2;
    if (p1 == NULL) {
       p1 = strstr(buf, "\012");
        if (p1 == NULL) {
            if (!strncmp(buf, IAC_IP, 2)) {
                // Client presses ctrl+C, regard as disconnection
                fprintf(stderr, "Client presses ctrl+C....\n");
                return 0;
            }
            ERR_EXIT("this really should not happen...");
        }
    }
    size_t len = p1 - buf + 1;
    memmove(reqP->buf, buf, len);
    reqP->buf[len - 1] = '\0';
    reqP->buf_len = len-1;
    return 1;
}

// connect off
void connect_off(request* requestP, fd_set *master){
    FD_CLR(requestP->conn_fd, master);
    close(requestP->conn_fd);
    free_request(requestP);
    return;
}

// check book
int bad(char a[]) {
    for (int i = 0; i < strlen(a); i++) {
        if (isalpha(a[i]))
            return 1;
    }
    if (a[0] == '-') {
        if (strlen(a) == 1)
            return 1;
        for (int i = 1; i < strlen(a); i++) {
            if (!isdigit(a[i]))
                return 1;
        }
    }else {
        for (int i = 0; i < strlen(a); i++) {
            if (!isdigit(a[i]))
                return 1;
        }
    }
    return 0;
}

//check id
int is_num(char *buf){
    for(int i = 0; i < strlen(buf); i++){
        if(!isdigit(buf[i])){
            return 0;
        }
    }
    return 1;
}


int main(int argc, char** argv) {

    // Parse args.
    if (argc != 2) {
        fprintf(stderr, "usage: %s [port]\n", argv[0]);
        exit(1);
    }

    struct sockaddr_in cliaddr;  // used by accept()
    int clilen;

    int conn_fd;  // fd for a new connection with client
    int file_fd;  // fd for file that we open for reading
    char buf[512];
    int buf_len;
    char str1[16], str2[16], str3[16];
    int write_lock[21];
    int read_lock[21];
    for (int i = 0; i < 21; i++)
        write_lock[i] = read_lock[i] = 0;

    fd_set readfds;
    fd_set master;

    struct flock file_lock;

    // Initialize server
    init_server((unsigned short) atoi(argv[1]));

    FD_ZERO(&master);
    FD_SET(svr.listen_fd, &master);
    file_fd = open(RECORD_PATH, O_RDWR);

    // Loop for handling connections
    fprintf(stderr, "\nstarting on %.80s, port %d, fd %d, maxconn %d...\n", svr.hostname, svr.port, svr.listen_fd, maxfd);

    while (1) {
        // TODO: Add IO multiplexing
        
        readfds = master;

        if (select(maxfd, &readfds, NULL, NULL, NULL) < 0) {
            ERR_EXIT("select");
        }

        for(int i = 0; i<maxfd; i++){
            if(FD_ISSET(i, &readfds) == 1){
                if(i == svr.listen_fd){
                    // Check new connection
                    clilen = sizeof(cliaddr);
                    conn_fd = accept(svr.listen_fd, (struct sockaddr*)&cliaddr, (socklen_t*)&clilen);
                    if (conn_fd < 0) {
                        if (errno == EINTR || errno == EAGAIN) continue;  // try again
                        if (errno == ENFILE) {
                            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
                            continue;
                        }
                        ERR_EXIT("accept");
                    }
                    requestP[conn_fd].conn_fd = conn_fd;
                    strcpy(requestP[conn_fd].host, inet_ntoa(cliaddr.sin_addr));
                    fprintf(stderr, "getting a new request... fd %d from %s\n", conn_fd, requestP[conn_fd].host);
                    FD_SET(conn_fd, &master);
                    sprintf(buf, "Please enter your id (to check your booking state):\n");
                    write(conn_fd, buf, strlen(buf));
                }else {
#ifdef READ_SERVER      
                    if (requestP[i].wait_for_read == 0) {
                        // first read
                        int ret = handle_read(&requestP[i]); // parse data from client to requestP[conn_fd].buf
                        fprintf(stderr, "ret = %d\n", ret);
                        // ctrl+c
                        if (ret == 0) {
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        if (ret < 0) {
                            fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                            continue;
                        }
                        //invalid id
                        if (!is_num(requestP[i].buf)) {
                            write(requestP[i].conn_fd, operation_failed_header, strlen(operation_failed_header));
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        requestP[i].id = atoi(requestP[i].buf);
                        if(requestP[i].id > 902020 || requestP[i].id < 902001) {
                            write(requestP[i].conn_fd, operation_failed_header, strlen(operation_failed_header));
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        // read lock
                        file_lock.l_type = F_RDLCK;
                        file_lock.l_whence = SEEK_SET;
                        file_lock.l_start = (requestP[i].id-902001)*sizeof(record);
                        file_lock.l_len = sizeof(record);
                        if(fcntl(file_fd, F_SETLK, &file_lock) < 0){
                            //locked
                            write(requestP[i].conn_fd, locked_header, strlen(locked_header));
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        read_lock[requestP[i].id-902000]++;

                        //read into requsetP.record
                        lseek(file_fd, (requestP[i].id-902001)*sizeof(record), SEEK_SET);
                        read(file_fd, &(requestP[i].Record), sizeof(record));
                        sprintf(buf, "Food: %d booked\nConcert: %d booked\nElectronics: %d booked\n\n(Type Exit to leave...)\n", requestP[i].Record.bookingState[FOOD_INDEX], requestP[i].Record.bookingState[CONCERT_INDEX], requestP[i].Record.bookingState[ELECS_INDEX]);
                        write(requestP[i].conn_fd, buf, strlen(buf));
                        requestP[i].wait_for_read = 1;
                    }else {
                        //second read
                        int ret = handle_read(&requestP[i]); // parse data from client to requestP[conn_fd].buf
                        fprintf(stderr, "ret = %d\n", ret);
                        // ctrl+c
                        if (ret == 0) {
                            // unlock
                            read_lock[requestP[i].id-902000]--;
                            if (read_lock[requestP[i].id-902000] == 0) {
                                file_lock.l_type = F_UNLCK;
                                file_lock.l_whence = SEEK_SET;
                                file_lock.l_start = (requestP[i].id-902001)*sizeof(record);
                                file_lock.l_len = sizeof(record);
                                if(fcntl(file_fd, F_SETLK, &file_lock) < 0){
                                    ERR_EXIT("fcntl");
                                }
                            }
                            requestP[i].wait_for_read = 0;
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        if (ret < 0) {
                            fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                            continue;
                        }
                        // exit
                        if(strcmp("Exit", requestP[i].buf) == 0) {
                            // unlock
                            read_lock[requestP[i].id-902000]--;
                            if (read_lock[requestP[i].id-902000] == 0) {
                                file_lock.l_type = F_UNLCK;
                                file_lock.l_whence = SEEK_SET;
                                file_lock.l_start = (requestP[i].id-902001)*sizeof(record);
                                file_lock.l_len = sizeof(record);
                                if(fcntl(file_fd, F_SETLK, &file_lock) < 0){
                                    ERR_EXIT("fcntl");
                                }
                            }
                            requestP[i].wait_for_read = 0;
                            connect_off(&requestP[i], &master);
                            break;
                        }
                    }
#elif defined WRITE_SERVER
                    if (requestP[i].wait_for_write == 0) {
                        // first write
                        int ret = handle_read(&requestP[i]); // parse data from client to requestP[conn_fd].buf
                        // ctrl+c
                        if (ret == 0) {
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        fprintf(stderr, "ret = %d\n", ret);
                        if (ret < 0) {
                            fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                            continue;
                        }
                        //invalid id
                        if (!is_num(requestP[i].buf)) {
                            write(requestP[i].conn_fd, operation_failed_header, strlen(operation_failed_header));
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        requestP[i].id = atoi(requestP[i].buf);
                        if(requestP[i].id > 902020 || requestP[i].id < 902001) {
                            write(requestP[i].conn_fd, operation_failed_header, strlen(operation_failed_header));
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        // write lock
                        file_lock.l_type = F_WRLCK;
                        file_lock.l_whence = SEEK_SET;
                        file_lock.l_start = (requestP[i].id-902001)*sizeof(record);
                        file_lock.l_len = sizeof(record);

                        if (write_lock[requestP[i].id-902000] == 1) {
                            //locked
                            write(requestP[i].conn_fd, locked_header, strlen(locked_header));
                            connect_off(&requestP[i], &master);
                            break; 
                        }
                        
                        if (fcntl(file_fd, F_SETLK, &file_lock) < 0) {
                            //locked
                            write(requestP[i].conn_fd, locked_header, strlen(locked_header));
                            connect_off(&requestP[i], &master);
                            break; 
                        }

                        requestP[i].wait_for_write = 1;
                        write_lock[requestP[i].id-902000] = 1;

                        // read into requsetP.record
                        lseek(file_fd, (requestP[i].id-902001)*sizeof(record), SEEK_SET);
                        read(file_fd, &(requestP[i].Record), sizeof(record));
                        sprintf(buf, "Food: %d booked\nConcert: %d booked\nElectronics: %d booked\n\nPlease input your booking command. (Food, Concert, Electronics. Positive/negative value increases/decreases the booking amount.):\n", requestP[i].Record.bookingState[FOOD_INDEX], requestP[i].Record.bookingState[CONCERT_INDEX], requestP[i].Record.bookingState[ELECS_INDEX]);
                        write(requestP[i].conn_fd, buf, strlen(buf));
                    }else {
                        // second write
                        int ret = handle_read(&requestP[i]); // parse data from client to requestP[conn_fd].buf
                        fprintf(stderr, "ret = %d\n", ret);
                        // ctrl+c
                        if (ret == 0) {
                            // unlock 
                            file_lock.l_type = F_UNLCK;
                            file_lock.l_whence = SEEK_SET;
                            file_lock.l_start = (requestP[i].id-902001)*sizeof(record);
                            file_lock.l_len = sizeof(record);
                            if(fcntl(file_fd, F_SETLK, &file_lock) < 0){
                                ERR_EXIT("fcntl");
                            }
                            write_lock[requestP[i].id-902000] = 0;
                            requestP[i].wait_for_write = 0;
                            connect_off(&requestP[i], &master);
                            break;
                        }
                        if (ret < 0) {
                            fprintf(stderr, "bad request from %s\n", requestP[conn_fd].host);
                            continue;
                        }
                        int num_scanf = sscanf(requestP[i].buf, "%s%s%s", str1, str2, str3);
                        if ((num_scanf != 3)||(bad(str1)||bad(str2)||bad(str3))) {
                            // invalid 
                            write(requestP[i].conn_fd, operation_failed_header, strlen(operation_failed_header));
                        }else {
                            if (requestP[i].Record.bookingState[0] + requestP[i].Record.bookingState[1] + requestP[i].Record.bookingState[2] + atoi(str1) + atoi(str2) + atoi(str3) > 15) {
                                // over 15
                                write(requestP[i].conn_fd, over_items, strlen(over_items)); 
                            }else if ((requestP[i].Record.bookingState[0] + atoi(str1) < 0)||(requestP[i].Record.bookingState[1] + atoi(str2) < 0)||(requestP[i].Record.bookingState[2] + atoi(str3) < 0)) {
                                // less than 0
                                write(requestP[i].conn_fd, less_items, strlen(less_items));
                            }else {
                                // update record
                                requestP[i].Record.bookingState[0] += atoi(str1);
                                requestP[i].Record.bookingState[1] += atoi(str2);
                                requestP[i].Record.bookingState[2] += atoi(str3);
                                sprintf(buf, "Bookings for user %d are updated, the new booking state is:\nFood: %d booked\nConcert: %d booked\nElectronics: %d booked\n", requestP[i].id, requestP[i].Record.bookingState[0], requestP[i].Record.bookingState[1], requestP[i].Record.bookingState[2]);
                                write(requestP[i].conn_fd, buf, strlen(buf));
                                lseek(file_fd, ((requestP[i].id-902000)*4-3)*sizeof(int), SEEK_SET);
                                for (int k = 0; k < OBJ_NUM; k++)
                                    write(file_fd, &(requestP[i].Record.bookingState[k]), sizeof(int));
                            }
                        }
                        // unlock 
                        file_lock.l_type = F_UNLCK;
                        file_lock.l_whence = SEEK_SET;
                        file_lock.l_start = (requestP[i].id-902001)*sizeof(record);
                        file_lock.l_len = sizeof(record);
                        if(fcntl(file_fd, F_SETLK, &file_lock) < 0){
                            ERR_EXIT("fcntl");
                        }
                        write_lock[requestP[i].id-902000] = 0;
                        requestP[i].wait_for_write = 0;
                        connect_off(&requestP[i], &master);
                        break;
                    }   
#endif              
                }
            }
        }
    }
    free(requestP);
    return 0;
}

// ======================================================================================================
// You don't need to know how the following codes are working
#include <fcntl.h>

static void init_request(request* reqP) {
    reqP->conn_fd = -1;
    reqP->buf_len = 0;
    reqP->id = 0;
    reqP->wait_for_write = 0;
    reqP->wait_for_read = 0;
}

static void free_request(request* reqP) {
    /*if (reqP->filename != NULL) {
        free(reqP->filename);
        reqP->filename = NULL;
    }*/
    init_request(reqP);
}

static void init_server(unsigned short port) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname(svr.hostname, sizeof(svr.hostname));
    svr.port = port;

    svr.listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (svr.listen_fd < 0) ERR_EXIT("socket");

    bzero(&servaddr, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);
    tmp = 1;
    if (setsockopt(svr.listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*)&tmp, sizeof(tmp)) < 0) {
        ERR_EXIT("setsockopt");
    }
    if (bind(svr.listen_fd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0) {
        ERR_EXIT("bind");
    }
    if (listen(svr.listen_fd, 1024) < 0) {
        ERR_EXIT("listen");
    }

    // Get file descripter table size and initialize request table
    maxfd = getdtablesize();
    requestP = (request*) malloc(sizeof(request) * maxfd);
    if (requestP == NULL) {
        ERR_EXIT("out of memory allocating all requests");
    }
    for (int i = 0; i < maxfd; i++) {
        init_request(&requestP[i]);
    }
    requestP[svr.listen_fd].conn_fd = svr.listen_fd;
    strcpy(requestP[svr.listen_fd].host, svr.hostname);

    return;
}