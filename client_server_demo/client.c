#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <assert.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>

#define SERVER_ADDR "127.0.0.1"
#define SERVER_IPOR 5388
#define BUF_SIZE  1024

char filebuf[BUF_SIZE]; // 发送缓冲区

const int MAX_SIZE_ONE_TIME_SEND = 40000;  // 由于如果udp传输过快，服务端会接受不及时，故规定发送一定数量的字节就 sleep 1秒 

const char *filename_audio = "input.aac";
const char *filename_video = "input.h264";

const char *AUDIO_HEAD = "AUDIO";
const char *VIDEO_HEAD = "VIDEO";
const char *FILE_END = "END";
const int HEAD_FILE_SIZE = 6;
const int END_FILE_SIZE = 4;

enum SEND_TYPE {AUDIO, VIDEO};

/*
*  发送音视频文件
*  成功返回>0， 失败返回-1
*/
int SendAVData(int fd, struct sockaddr_in server_addr, FILE *file, enum SEND_TYPE type) {
    int ret = 0, size = 0, tmp = 0, loop_time = 1;
    clock_t start, finish;

    if (type == AUDIO) {
        if ((ret = sendto(fd, AUDIO_HEAD, HEAD_FILE_SIZE, 0, &server_addr, sizeof(server_addr))) < 0)  // 发送音频文件头
            return ret;
        printf("\n=================Start transmit audio file!===================\n\n");
    }
    else if (type == VIDEO){
        if ((ret = sendto(fd, VIDEO_HEAD, HEAD_FILE_SIZE, 0, &server_addr, sizeof(server_addr))) < 0)  // 发送视频文件头
            return ret;
        printf("\n=================Start transmit video file!===================\n\n");
    }
    else
        return -1;

    start = clock();
    __bzero(filebuf, BUF_SIZE);
    while ((tmp = fread(filebuf, 1, BUF_SIZE, file)) > 0) {
        size += tmp;
        //printf("Read %d bytes data from audio file\n", tmp);

        if ((ret = sendto(fd, filebuf, tmp, 0, &server_addr, sizeof(server_addr))) < 0)
            return ret;
        //printf("Success send %d bytes audio data\n", ret);

        __bzero(filebuf, BUF_SIZE);

        if (size > loop_time * MAX_SIZE_ONE_TIME_SEND) {
            loop_time++;
            sleep(1);
        }
        //sleep(2);
    }
    printf("Success send %d bytes file!\n", size);

    if ((ret = sendto(fd, FILE_END, END_FILE_SIZE, 0, &server_addr, sizeof(server_addr))) < 0)  // 发送文件尾
        return ret;

    finish = clock();
    double duration = (double)(finish - start) / CLOCKS_PER_SEC;    //  用时
    printf( "Time cost of transmission is : %f seconds\n", duration);
    
    printf("\n===================Transmission completed!====================\n");
    
    return ret;
}

int main() {
    int ret;
    /*
    * 打开input 文件
    */
    FILE *input_audio = fopen(filename_audio, "rb");
    if (input_audio == NULL) {
        printf("Open input audio file failed!\n");
        return -1;
    }
    
    FILE *input_video = fopen(filename_video, "rb");
    if (input_video == NULL) {
        printf("Open input video file failed!\n");
        return -1;
    }

    /*
    * 创建socket
    */
    struct sockaddr_in server_address;
    __bzero(&server_address, sizeof(server_address));  // 初始化

    server_address.sin_family = AF_INET;  // 地址族
    server_address.sin_port = htons(SERVER_IPOR);  // 端口
    inet_pton(AF_INET, SERVER_ADDR, &server_address.sin_addr);  // socket 地址

    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);  // ipv4, 数据报服务

    /*
    *  依次发送数据
    */
    if ((ret = SendAVData(sockfd, server_address, input_audio, AUDIO)) < 0) {
        printf("Send audio file failed!\n");
        goto END;
    }

    if ((ret = SendAVData(sockfd, server_address, input_video, VIDEO)) < 0) {
        printf("Send video file failed!\n");
        goto END;
    }
    
END:
    fclose(input_audio);
    fclose(input_video);
    close(sockfd); // 关闭fd
    return 0;
}
