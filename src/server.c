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
#include <libavformat/avformat.h>

#define BUF_SIZE       65507
#define FILENAME_LEN   128         // 文件路径大小

char recvbuf[BUF_SIZE];

const char *configure_file = "configure";   // 配置文件路径

const char *filename_audio;  // Not sure is .aac or .mp3
const char *filename_video = "./avfile/muxer.h264";
const char *out_filename = "./avfile/output.mp4";

enum RECV_TYPE {AUDIO, VIDEO};
const char *AUDIO_AAC_HEAD = "AUDIO_A";
const char *AUDIO_MP3_HEAD = "AUDIO_M";
const char *VIDEO_AVL_HEAD = "VIDEO_H";
const char *FILE_END = "END";
const int HEAD_FILE_SIZE = 8;
const int END_FILE_SIZE = 4;

struct sockaddr_in client_addr;
socklen_t client_len;
int my_port;

int audio_size = 0;
int video_size = 0;

/*
*  解析配置文件
*/
int readConfigure() {
    FILE *file = fopen(configure_file, "r");
    if (file == NULL) {
        printf("Open configure file failed!\n");
        return -1;
    }
    fread(recvbuf, 1, 2*FILENAME_LEN, file);

    int index = 0;
    while (recvbuf[index] == ' ')
        index++;

    char *ptr = strchr(recvbuf + index, ' ');
    char port[FILENAME_LEN];
    if (ptr == NULL) {
        printf("Wrong format of configure!\n");
        fclose(file);
        return -1;
    }
    
    strcpy(port, ptr);
    my_port = atoi(port);
    if (my_port <= 0) {
        printf("Cannot extract server port!\n");
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}

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
    //clock_t start, finish;

    if (type == AUDIO) {
        file = fopen(filename_audio, "wb+");
        //printf("\n=================Start receive audio file!===================\n\n");      
    }
    else if (type == VIDEO) {
        file = fopen(filename_video, "wb+");
        //printf("\n=================Start receive video file!===================\n\n");
    }  
    else
        return -1;
    
    //start = clock();

    __bzero(recvbuf, BUF_SIZE);
    while (1) {
        data_s = recvfrom(fd, recvbuf, BUF_SIZE, 0, (struct sockaddr *)&client_addr, &client_len);

        if (data_s == END_FILE_SIZE && strcmp(recvbuf, FILE_END) == 0) {
            /*
            if (type == AUDIO)
                printf("Recv %d bytes audio data from client in total\n", audio_size);
            else if (type == VIDEO)
                printf("Recv %d bytes video data from client in total\n", video_size);

            finish = clock();
            */
            break;
        }

        if (data_s > 0) {
            //printf("Recv %d bytes data from client\n", data_s);
            fwrite(recvbuf, data_s, 1, file);
            if (type == AUDIO)
                audio_size += data_s;
            else if (type == VIDEO)
                video_size += data_s;
        }
    }
/*
    double duration = (double)(finish - start) / CLOCKS_PER_SEC;    //  用时
    printf( "Time cost of transmission is : %f seconds\n", duration);

    printf("\n=========================Received============================\n");
*/
    fclose(file);
    return 0;
}

int main() {
    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL, *ofmt_ctx = NULL;
    AVOutputFormat *ofmt = NULL;
    AVPacket pkt;
    int ret, i, size;
    int videoindex_v = -1, videoindex_out = -1; // 表示输入/输出video流的index
    int audioindex_a = -1, audioindex_out = -1; // 表示输入/输出audio流的index
    int64_t cur_pts_v = 0, cur_pts_a = 0;
    int frame_index = 0;

    struct sockaddr_in my_addr;

    if ((ret = readConfigure()) < 0)
        exit(-1);   
    printf("\nServer:  =====  server port: %d  =====\n", my_port);

    
    __bzero(&my_addr, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(my_port);
    my_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);
    setnonblocking(sockfd);

    /* bind */
    if (bind(sockfd, (struct sockaddr *)&my_addr, sizeof(my_addr)) < 0) {
        printf("Error with bind socket!\n");
        goto ERROR;
    }

    __bzero(recvbuf, BUF_SIZE);
    while ((size = recvfrom(sockfd, recvbuf, BUF_SIZE-1, 0, (struct sockaddr *)&client_addr, &client_len)) != 0) {

        if (size == -1 && (errno == EAGAIN || errno == EWOULDBLOCK))  // 非阻塞，未读到数据
            continue;

        else if (size == HEAD_FILE_SIZE) {
           // printf("Recv %s from client\n", recvbuf);

            if (strcmp(recvbuf, AUDIO_AAC_HEAD) == 0) {
                filename_audio = "./avfile/muxer.aac";
                RecvAVData(sockfd, AUDIO);
            }
            else if (strcmp(recvbuf, AUDIO_MP3_HEAD) == 0) {
                filename_audio = "./avfile/muxer.mp3";
                RecvAVData(sockfd, AUDIO);
            }
            else if (strcmp(recvbuf, VIDEO_AVL_HEAD) == 0)
                RecvAVData(sockfd, VIDEO);
        }

        else
            __bzero(recvbuf, BUF_SIZE);

        if (audio_size && video_size)
            break;
    }

    printf("\nServer:  =====  Recieved all avfile  =====\n");
    printf("        Recieve audio file: %d Bytes\n", audio_size);
    printf("        Recieve video file: %d Bytes\n", video_size);

    // 打开输入文件，读取信息
    if ((ret = avformat_open_input(&ifmt_ctx_v, filename_video, 0, 0)) < 0) {
        printf("Could not open input file!\n");
        goto ERROR;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		goto ERROR;
	}

	if ((ret = avformat_open_input(&ifmt_ctx_a, filename_audio, 0, 0)) < 0) {
		printf( "Could not open input file.");
		goto ERROR;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		goto ERROR;
	}

    // 初始化输出文件
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
		printf( "Could not create output context!\n");
		ret = AVERROR_UNKNOWN;
		goto ERROR;
	}
    ofmt = ofmt_ctx->oformat; // 取输出格式的句柄

    // 为输出文件创建音视频流并增加编解码信息
    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) { // 视频
        AVStream *in_stream = ifmt_ctx_v->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        videoindex_v = i; // 输入video文件的stream_index
        if (!out_stream) {
            printf( "Failed allocating output stream!\n");
            ret = AVERROR_UNKNOWN;
            goto ERROR;
        }
        videoindex_out = out_stream->index; // 输出文件video的stream_index
        // 增加编解码信息
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
            printf( "Failed to copy context from input to output stream codec context!\n");
            goto ERROR;
        }
        out_stream->codec->codec_tag = 0; // ?
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) // 如果需要global header
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        break;
    }
    
    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) { // 音频
        AVStream *in_stream = ifmt_ctx_a->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        audioindex_a = i; // 输入audio文件的stream_index
        if (!out_stream) {
            printf( "Failed allocating output stream!\n");
            ret = AVERROR_UNKNOWN;
            goto ERROR;
        }
        audioindex_out = out_stream->index; // 输出文件audio的stream_index
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
            printf( "Failed to copy context from input to output stream codec context!\n");
            goto ERROR;
        }
        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        break;
	}

    // 如果没有打开需要手动打开
    if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
			printf( "Could not open output file '%s'!\n", out_filename);
			goto ERROR;
		}
	}
    // 写文件头
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		printf( "Error occurred when opening output file!\n");
		goto ERROR;
	}

    // 输出视频流信息
    printf("\nServer:  ==========Output Information==========\n");
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	printf("===============================================\n");

    // 写视频/音频帧
    while (1) {
        AVFormatContext *ifmt_ctx = NULL;
        int in_stream_index = -1, out_stream_index = -1;
        AVStream *in_stream = NULL, *out_stream = NULL;

        // 判断这帧是视频帧还是音频帧
        if(av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, cur_pts_a, ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){ 
            // 选择视频帧
            ifmt_ctx = ifmt_ctx_v;
            in_stream_index = videoindex_v; // 定位输入文件的stream
            out_stream_index = videoindex_out; 
        }
        else {
            // 选择音频帧
            ifmt_ctx = ifmt_ctx_a;
            in_stream_index = audioindex_a;
            out_stream_index = audioindex_out;
        }

        // 读取一帧
        if (av_read_frame(ifmt_ctx, &pkt) >= 0) {

            in_stream = ifmt_ctx->streams[in_stream_index];
            out_stream = ofmt_ctx->streams[out_stream_index];

            // 如果该帧没有PTS，需要补齐
            if (pkt.pts == AV_NOPTS_VALUE) {
                AVRational time_base1 = in_stream->time_base;
                // 2帧之间的持续时间
                int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                // 补齐参数
                pkt.pts = (double)(frame_index * calc_duration) / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                pkt.dts = pkt.pts;
                pkt.duration = (double)calc_duration / (double)(av_q2d(time_base1) * AV_TIME_BASE);
                frame_index++;
            }
            
            if (out_stream_index == videoindex_out)
                cur_pts_v = pkt.pts;
            else
                cur_pts_a = pkt.pts;
        }
        else {
            break;
        }
        
        //Convert PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index = out_stream_index;

        //printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
		//Write
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			printf( "Error muxing packet\n");
			break;
		}
		av_packet_unref(&pkt);
    }

    // 写文件尾
	av_write_trailer(ofmt_ctx);   

ERROR:
    avformat_close_input(&ifmt_ctx_v);
	avformat_close_input(&ifmt_ctx_a);
	/* close output */
	if (ofmt_ctx && !(ofmt->flags & AVFMT_NOFILE))
		avio_close(ofmt_ctx->pb);
	avformat_free_context(ofmt_ctx);
	if (ret < 0 && ret != AVERROR_EOF) {
		printf( "Error occurred.\n");
		return -1;
	}
    return 0;
}