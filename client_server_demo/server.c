#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>

#define MY_PORT  5388
#define BUF_SIZE 1024

char recvbuf[BUF_SIZE];

const char *filename_audio = "output.aac";
const char *filename_video = "output.h264";

const char *AUDIO_HEAD = "AUDIO";
const char *VIDEO_HEAD = "VIDEO";
const char *FILE_END = "END";
const int HEAD_FILE_SIZE = 6;
const int END_FILE_SIZE = 4;

int audio_size = 0;
int video_size = 0;

struct sockaddr_in client_addr;
socklen_t client_len;

enum RECV_TYPE {AUDIO, VIDEO};

/*
*  设置非阻塞fd
*/
int setnonblocking(int fd) {
    int old_option = fcntl(fd, F_GETFL);
    int new_option = old_option | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_option);
    return old_option;
}

int RecvAVData(int fd, enum RECV_TYPE type) {
    int data_s = 0;
    FILE *file;
    clock_t start, finish;

    if (type == AUDIO) {
        file = fopen(filename_audio, "wb+");
        printf("\n=================Start receive audio file!===================\n\n");      
    }
    else if (type == VIDEO) {
        file = fopen(filename_video, "wb+");
        printf("\n=================Start receive video file!===================\n\n");
    }  
    else
        return -1;
    
    start = clock();

    __bzero(recvbuf, BUF_SIZE);
    while (1) {
        data_s = recvfrom(fd, recvbuf, BUF_SIZE, 0, &client_addr, &client_len);

        if (data_s == END_FILE_SIZE && strcmp(recvbuf, FILE_END) == 0) {
            if (type == AUDIO)
                printf("Recv %d bytes audio data from client in total\n", audio_size);
            else if (type == VIDEO)
                printf("Recv %d bytes video data from client in total\n", video_size);

            finish = clock();
            break;
        }

        if (data_s > 0) {
            //printf("Recv %d bytes audio data from client\n", data_s);
            //if (data_s < 10)
              //  printf("%s\n", recvbuf);
            fwrite(recvbuf, data_s, 1, file);
            if (type == AUDIO)
                audio_size += data_s;
            else if (type == VIDEO)
                video_size += data_s;
        }
    }

    double duration = (double)(finish - start) / CLOCKS_PER_SEC;    //  用时
    printf( "Time cost of transmission is : %f seconds\n", duration);

    printf("\n=========================Received============================\n");
    fclose(file);
    return 0;
}

int main() {
    struct sockaddr_in my_addr;
    int size;

    __bzero(&my_addr, sizeof(my_addr));

    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(MY_PORT);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    setnonblocking(sockfd);
    
    /* bind */
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        printf("Error with bind socket!\n");
        goto END;
    }

    __bzero(recvbuf, BUF_SIZE);
    while (size = recvfrom(sockfd, recvbuf, BUF_SIZE-1, 0, (struct sockaddr *)&client_addr, &client_len)) {

        if (size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))  // 非阻塞，未读到数据
            continue;

        else if (size == HEAD_FILE_SIZE) {
            //printf("Recv %s from client\n", recvbuf);

            if (strcmp(recvbuf, AUDIO_HEAD) == 0)
                RecvAVData(sockfd, AUDIO);
            else if (strcmp(recvbuf, VIDEO_HEAD) == 0)
                RecvAVData(sockfd, VIDEO);
        }

        else
            __bzero(recvbuf, BUF_SIZE);

        if (audio_size && video_size)
            break;
    }

END:
    close(sockfd);
    return 0;
}