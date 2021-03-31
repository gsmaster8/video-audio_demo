#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <libavformat/avformat.h>

#define  SERVER_ADDR       "127.0.0.1" // 服务端地址
#define  BUF_SIZE          65507       // 发送缓存区大小，UDP能传输的最大值
#define  ADTS_HEADER_SIZE  7           // ADTs头大小
#define  FILENAME_LEN      128         // 文件路径大小

int DEMUXER_AAC = 0;    // 标识是否使用AAC编码，>0为是
const char *out_filename_audio = "./avfile/demuxer.mp3";    // 解复用得到的音频文件,默认mp3
const char *out_filename_video = "./avfile/demuxer.h264";   // 解复用得到的视频文件
const char *configure_file = "configure";   // 配置文件路径
/*
*  配置文件相关
*/
char input_flv_file[FILENAME_LEN];
int server_port;

/*
* UDP传输相关
*/
enum SEND_TYPE {AUDIO_A, AUDIO_M, VIDEO_H};
const char *AUDIO_AAC_HEAD = "AUDIO_A";
const char *AUDIO_MP3_HEAD = "AUDIO_M";
const char *VIDEO_AVL_HEAD = "VIDEO_H";
const char *FILE_END = "END";
const int HEAD_FILE_SIZE = 8;
const int END_FILE_SIZE = 4;
const int MAX_SIZE_ONE_TIME_SEND = 130000;  // 发送130000字节就 sleep 1秒

char filebuf[BUF_SIZE]; // 发送缓冲区

typedef struct {
	int write_adts;  // 是否解析过
	int objecttype;  // 编码类型 占AudioSpecificConfig的 5bit
	int sample_rate_index;  //采样率 占AudioSpecificConfig的 4bit
	int channel_conf;  // 通道数 占AudioSpecificConfig的 4bit
}ADTSContext;

/*
*  解析配置文件
*/
int readConfigure() {
    FILE *file = fopen(configure_file, "r");
    if (file == NULL) {
        printf("Open configure file failed!\n");
        return -1;
    }
    fread(filebuf, 1, 2*FILENAME_LEN, file);

    int index = 0;
    while (filebuf[index] == ' ')
        index++;

    char *ptr = strchr(filebuf + index, ' ');
    char port[FILENAME_LEN];
    if (ptr == NULL) {
        printf("Wrong format of configure!\n");
        fclose(file);
        return -1;
    }

    //strncpy(input_flv_file, filebuf + index, ptr - filebuf - index);
    memcpy(input_flv_file, filebuf + index, ptr - filebuf - index);
    if ((*ptr) == ' ')
        ptr++;
    
    strcpy(port, ptr);
    server_port = atoi(port);
    if (server_port <= 0) {
        printf("Cannot extract server port!\n");
        fclose(file);
        return -1;
    }
    fclose(file);
    return 0;
}

/*
* 从AudioSpecificConfig结构（第一个AudioTag）提取ADTSContext信息
*/
int aac_decode_extradata(ADTSContext *adts, unsigned char *pbuf, int bufsize) {  
	if (!adts || !pbuf || bufsize < 2) {  
		return -1;  
	}
    if (adts->write_adts) // 已经解码过
        return 0;
    int aot, aotext, samfreindex;  
	int channelconfig;  
	unsigned char *p = pbuf;  
	aot = (p[0] >> 3) & 0x1f;  // 取 object_Type
	if (aot == 31) {  // 如果为特殊值 31
		aotext = ((p[0] << 3) | (p[1] >> 5)) & 0x3f; // 接着取6 bits 
		aot = 32 + aotext;  // 32+ 6bits
		samfreindex = (p[1] >> 1) & 0x0f; // 4 bits的采样率  
		if (samfreindex == 0x0f) {  // 如果为特殊值  15
			channelconfig = ((p[4] << 3) | (p[5] >> 5)) & 0x0f;  // 跳过Frequency的 24bits, 取4 bits
		}  
		else {  
			channelconfig = ((p[1] << 3) | (p[2] >> 5)) & 0x0f;  // 取 4bits
		}  
	}  
	else {  
		samfreindex = ((p[0] << 1) | (p[1] >> 7)) & 0x0f;  // 同上
		if (samfreindex == 0x0f) {  
			channelconfig = (p[4] >> 3) & 0x0f;  
		}  
		else {  
			channelconfig = (p[1] >> 3) & 0x0f;  
		}  
	}  
#ifdef AOT_PROFILE_CTRL  
	if (aot < 2) aot = 2;  
#endif  
	adts->objecttype = aot - 1;  // 减一，与ADTs头的profile字段对应
	adts->sample_rate_index = samfreindex;  
	adts->channel_conf = channelconfig;  
	adts->write_adts = 1;  
	return 0;  
}

/*
*  根据ADTSContext内容生成ADTs（7Bytes）插入到每一个音频帧前
*/
int aac_set_adts_head(ADTSContext *acfg, unsigned char *buf, int size) {         
	unsigned char byte;        
	buf[0] = 0xff;  
	buf[1] = 0xf1;  // MPEG-4 无CRC检验
	byte = 0;  
	byte |= (acfg->objecttype & 0x03) << 6;  // Profile: 2bits
	byte |= (acfg->sample_rate_index & 0x0f) << 2;  // Frequency_index : 4bits
	byte |= (acfg->channel_conf & 0x07) >> 2;  
	buf[2] = byte;  
	byte = 0;  
	byte |= (acfg->channel_conf & 0x07) << 6;  
	byte |= (ADTS_HEADER_SIZE + size) >> 11;  // 取13位的前2位
	buf[3] = byte;  
	byte = 0;  
	byte |= (ADTS_HEADER_SIZE + size) >> 3; // 取13位的第4-11位 
	buf[4] = byte;  
	byte = 0;  
	byte |= ((ADTS_HEADER_SIZE + size) & 0x7) << 5;  // 取13位的后3位
	byte |= (0x7ff >> 6) & 0x1f;  // 取11位的前5位
	buf[5] = byte;  
	byte = 0;  
	byte |= (0x7ff & 0x3f) << 2;  //取11位的后6位
	buf[6] = byte;     
	return 0;  
}

/*
*  发送音视频文件
*  成功返回>0， 失败返回-1
*/
int SendAVData(int fd, struct sockaddr_in server_addr, FILE *file, enum SEND_TYPE type) {
    int ret = 0, size = 0, tmp = 0, loop_time = 1;
    //clock_t start, finish;
    if (type == AUDIO_A) {
        if ((ret = sendto(fd, AUDIO_AAC_HEAD, HEAD_FILE_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)  // 发送音频文件头
            return ret;
        //printf("\n=================Start transmit audio file!===================\n\n");
    }
    else if (type == AUDIO_M) {
        if ((ret = sendto(fd, AUDIO_MP3_HEAD, HEAD_FILE_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)  // 发送音频文件头
            return ret;
        //printf("\n=================Start transmit audio file!===================\n\n");
    }
    else if (type == VIDEO_H){
        if ((ret = sendto(fd, VIDEO_AVL_HEAD, HEAD_FILE_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)  // 发送视频文件头
            return ret;
        //printf("\n=================Start transmit video file!===================\n\n");
    }
    else
        return -1;
    //start = clock();
    __bzero(filebuf, BUF_SIZE);
    while ((tmp = fread(filebuf, 1, BUF_SIZE, file)) > 0) {
        size += tmp;
        //printf("Read %d bytes data from audio file\n", tmp);
        if ((ret = sendto(fd, filebuf, tmp, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)
            return ret;
        //printf("Success send %d bytes audio data\n", ret);
        __bzero(filebuf, BUF_SIZE);
        if (size > loop_time * MAX_SIZE_ONE_TIME_SEND) {
            loop_time++;
            sleep(1);
        }
    }
    printf("        Success send %d bytes file!\n", size);
    sleep(1); // 防止服务端socket还有缓存没读完
    if ((ret = sendto(fd, FILE_END, END_FILE_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)  // 发送文件尾
        return ret;
    //finish = clock();
    //double duration = (double)(finish - start) / CLOCKS_PER_SEC;    //  用时
    //printf( "Time cost of transmission is : %f seconds\n", duration);
    //printf("\n===================Transmission completed!====================\n");
    return ret;
}

int main() {
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket pkt;
    int videoindex = -1, audioindex = -1;
    int ret, i;

    if ((ret = readConfigure()) < 0)
        exit(-1);   
    //printf("\nClient:  =====input file: %s  =====\n", input_flv_file);

    if ((ret = avformat_open_input(&ifmt_ctx, input_flv_file, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto ERROR;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto ERROR;
    }

    for (i=0; i<ifmt_ctx->nb_streams; i++) { //nb_streams：视音频流的个数
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            videoindex = i;
        else if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            if (ifmt_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC) {// 判断是否是aac编码
                DEMUXER_AAC = 1;
                out_filename_audio = "./avfile/demuxer.aac";
            }
        }       
    }

    printf("\nClient:  ==========Input Information==========\n");
	av_dump_format(ifmt_ctx, 0, input_flv_file, 0);  // 打印信息
	printf("==============================================\n");

    FILE *fp_audio = fopen(out_filename_audio,"wb+");  
	FILE *fp_video = fopen(out_filename_video,"wb+");

    AVBSFContext *video_bsf_ctx = NULL; 
    const AVBitStreamFilter *video_filter = av_bsf_get_by_name("h264_mp4toannexb");
    if (video_filter == NULL) {
        printf("Get video bsf failed!\n");
        goto ERROR;
    }

    ADTSContext stADTSContext = {0};
	unsigned char pAdtsHead[7];

    while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
        // 视频帧
        if (pkt.stream_index == videoindex) {
            if ((ret = av_bsf_alloc(video_filter, &video_bsf_ctx)) != 0) {
                printf("Alloc video bsf failed!\n");
                goto ERROR;
            }
            ret = avcodec_parameters_from_context(video_bsf_ctx->par_in, ifmt_ctx->streams[videoindex]->codec);
            if (ret < 0) {
                printf("Set video Codec failed!\n");
                goto ERROR;
            }
            ret = av_bsf_init(video_bsf_ctx);
            if (ret < 0) {
                printf("Init video bsf failed!\n");
                goto ERROR;
            }
            av_bsf_send_packet(video_bsf_ctx, &pkt);
            ret = av_bsf_receive_packet(video_bsf_ctx, &pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                printf("Receive video Pkt failed!\n");
                goto ERROR;
            }
            fwrite(pkt.data, 1, pkt.size, fp_video);
        }
        // 音频帧
        else if (pkt.stream_index == audioindex) {
            if (DEMUXER_AAC) {
                if ((ret = aac_decode_extradata(&stADTSContext, ifmt_ctx->streams[audioindex]->codec->extradata, ifmt_ctx->streams[audioindex]->codec->extradata_size)) == -1) {
                    printf("Decode extradata failed!\n");
                    goto ERROR;
                } // 解析AudioSpecificConfig结构
			    aac_set_adts_head(&stADTSContext, pAdtsHead, pkt.size); // 生成ADTs头
			    fwrite(pAdtsHead, 1, 7, fp_audio); // 写ADTs头
            }
            fwrite(pkt.data, 1, pkt.size, fp_audio);
        }
        av_packet_unref(&pkt);
    }

    printf("\nClient:  =====  Demuxer: %s, %s  =====\n", out_filename_audio, out_filename_video);
    /*
    * 关闭文件 *重要，否则会导致发送文件不全
    */
    fclose(fp_video);
    fclose(fp_audio);
    fp_audio = NULL;
    fp_video = NULL;

    /*
    * 打开input 文件
    */
    FILE *input_audio = fopen(out_filename_audio, "rb");
    if (input_audio == NULL) {
        printf("Open input audio file failed!\n");
        goto ERROR;
    }
    FILE *input_video = fopen(out_filename_video, "rb");
    if (input_video == NULL) {
        printf("Open input video file failed!\n");
        goto ERROR;
    }

     /*
    * 创建socket
    */
    struct sockaddr_in server_address;
    __bzero(&server_address, sizeof(server_address));  // 初始化

    server_address.sin_family = AF_INET;  // 地址族
    server_address.sin_port = htons(server_port);  // 端口
    inet_pton(AF_INET, SERVER_ADDR, &server_address.sin_addr);  // socket 地址

    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);  // ipv4, 数据报服务

    printf("\nClient:  =====  Start send avfile  =====\n");
    /*
    *  依次发送数据
    */
    if (DEMUXER_AAC) {
        ret = SendAVData(sockfd, server_address, input_audio, AUDIO_A);
        if (ret < 0) {
            printf("Send audio file failed!\n");
            goto ERROR;
        }
    } 
    else {
        ret = SendAVData(sockfd, server_address, input_audio, AUDIO_M);
        if (ret < 0) {
            printf("Send audio file failed!\n");
            goto ERROR;
        }
    }

    if ((ret = SendAVData(sockfd, server_address, input_video, VIDEO_H)) < 0) {
        printf("Send video file failed!\n");
        goto ERROR;
    }

ERROR:
    if (ifmt_ctx)
        avformat_close_input(&ifmt_ctx);
    if (fp_audio)
        fclose(fp_audio);
    if (fp_video)
        fclose(fp_video);
    if (video_bsf_ctx)
        av_bsf_free(&video_bsf_ctx);
    if (input_audio)
        fclose(input_audio);
    if (input_video)
        fclose(input_video);
    close(sockfd);
    if (ret < 0) {
        printf("Run client failed!\n");
    }
    return 0;
}