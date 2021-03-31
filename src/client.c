#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <libavformat/avformat.h>

#define  SERVER_ADDR       "127.0.0.1" // ����˵�ַ
#define  BUF_SIZE          65507       // ���ͻ�������С��UDP�ܴ�������ֵ
#define  ADTS_HEADER_SIZE  7           // ADTsͷ��С
#define  FILENAME_LEN      128         // �ļ�·����С

int DEMUXER_AAC = 0;    // ��ʶ�Ƿ�ʹ��AAC���룬>0Ϊ��
const char *out_filename_audio = "./avfile/demuxer.mp3";    // �⸴�õõ�����Ƶ�ļ�,Ĭ��mp3
const char *out_filename_video = "./avfile/demuxer.h264";   // �⸴�õõ�����Ƶ�ļ�
const char *configure_file = "configure";   // �����ļ�·��
/*
*  �����ļ����
*/
char input_flv_file[FILENAME_LEN];
int server_port;

/*
* UDP�������
*/
enum SEND_TYPE {AUDIO_A, AUDIO_M, VIDEO_H};
const char *AUDIO_AAC_HEAD = "AUDIO_A";
const char *AUDIO_MP3_HEAD = "AUDIO_M";
const char *VIDEO_AVL_HEAD = "VIDEO_H";
const char *FILE_END = "END";
const int HEAD_FILE_SIZE = 8;
const int END_FILE_SIZE = 4;
const int MAX_SIZE_ONE_TIME_SEND = 130000;  // ����130000�ֽھ� sleep 1��

char filebuf[BUF_SIZE]; // ���ͻ�����

typedef struct {
	int write_adts;  // �Ƿ������
	int objecttype;  // �������� ռAudioSpecificConfig�� 5bit
	int sample_rate_index;  //������ ռAudioSpecificConfig�� 4bit
	int channel_conf;  // ͨ���� ռAudioSpecificConfig�� 4bit
}ADTSContext;

/*
*  ���������ļ�
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
* ��AudioSpecificConfig�ṹ����һ��AudioTag����ȡADTSContext��Ϣ
*/
int aac_decode_extradata(ADTSContext *adts, unsigned char *pbuf, int bufsize) {  
	if (!adts || !pbuf || bufsize < 2) {  
		return -1;  
	}
    if (adts->write_adts) // �Ѿ������
        return 0;
    int aot, aotext, samfreindex;  
	int channelconfig;  
	unsigned char *p = pbuf;  
	aot = (p[0] >> 3) & 0x1f;  // ȡ object_Type
	if (aot == 31) {  // ���Ϊ����ֵ 31
		aotext = ((p[0] << 3) | (p[1] >> 5)) & 0x3f; // ����ȡ6 bits 
		aot = 32 + aotext;  // 32+ 6bits
		samfreindex = (p[1] >> 1) & 0x0f; // 4 bits�Ĳ�����  
		if (samfreindex == 0x0f) {  // ���Ϊ����ֵ  15
			channelconfig = ((p[4] << 3) | (p[5] >> 5)) & 0x0f;  // ����Frequency�� 24bits, ȡ4 bits
		}  
		else {  
			channelconfig = ((p[1] << 3) | (p[2] >> 5)) & 0x0f;  // ȡ 4bits
		}  
	}  
	else {  
		samfreindex = ((p[0] << 1) | (p[1] >> 7)) & 0x0f;  // ͬ��
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
	adts->objecttype = aot - 1;  // ��һ����ADTsͷ��profile�ֶζ�Ӧ
	adts->sample_rate_index = samfreindex;  
	adts->channel_conf = channelconfig;  
	adts->write_adts = 1;  
	return 0;  
}

/*
*  ����ADTSContext��������ADTs��7Bytes�����뵽ÿһ����Ƶ֡ǰ
*/
int aac_set_adts_head(ADTSContext *acfg, unsigned char *buf, int size) {         
	unsigned char byte;        
	buf[0] = 0xff;  
	buf[1] = 0xf1;  // MPEG-4 ��CRC����
	byte = 0;  
	byte |= (acfg->objecttype & 0x03) << 6;  // Profile: 2bits
	byte |= (acfg->sample_rate_index & 0x0f) << 2;  // Frequency_index : 4bits
	byte |= (acfg->channel_conf & 0x07) >> 2;  
	buf[2] = byte;  
	byte = 0;  
	byte |= (acfg->channel_conf & 0x07) << 6;  
	byte |= (ADTS_HEADER_SIZE + size) >> 11;  // ȡ13λ��ǰ2λ
	buf[3] = byte;  
	byte = 0;  
	byte |= (ADTS_HEADER_SIZE + size) >> 3; // ȡ13λ�ĵ�4-11λ 
	buf[4] = byte;  
	byte = 0;  
	byte |= ((ADTS_HEADER_SIZE + size) & 0x7) << 5;  // ȡ13λ�ĺ�3λ
	byte |= (0x7ff >> 6) & 0x1f;  // ȡ11λ��ǰ5λ
	buf[5] = byte;  
	byte = 0;  
	byte |= (0x7ff & 0x3f) << 2;  //ȡ11λ�ĺ�6λ
	buf[6] = byte;     
	return 0;  
}

/*
*  ��������Ƶ�ļ�
*  �ɹ�����>0�� ʧ�ܷ���-1
*/
int SendAVData(int fd, struct sockaddr_in server_addr, FILE *file, enum SEND_TYPE type) {
    int ret = 0, size = 0, tmp = 0, loop_time = 1;
    //clock_t start, finish;
    if (type == AUDIO_A) {
        if ((ret = sendto(fd, AUDIO_AAC_HEAD, HEAD_FILE_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)  // ������Ƶ�ļ�ͷ
            return ret;
        //printf("\n=================Start transmit audio file!===================\n\n");
    }
    else if (type == AUDIO_M) {
        if ((ret = sendto(fd, AUDIO_MP3_HEAD, HEAD_FILE_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)  // ������Ƶ�ļ�ͷ
            return ret;
        //printf("\n=================Start transmit audio file!===================\n\n");
    }
    else if (type == VIDEO_H){
        if ((ret = sendto(fd, VIDEO_AVL_HEAD, HEAD_FILE_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)  // ������Ƶ�ļ�ͷ
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
    sleep(1); // ��ֹ�����socket���л���û����
    if ((ret = sendto(fd, FILE_END, END_FILE_SIZE, 0, (struct sockaddr *)&server_addr, sizeof(server_addr))) < 0)  // �����ļ�β
        return ret;
    //finish = clock();
    //double duration = (double)(finish - start) / CLOCKS_PER_SEC;    //  ��ʱ
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

    for (i=0; i<ifmt_ctx->nb_streams; i++) { //nb_streams������Ƶ���ĸ���
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            videoindex = i;
        else if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            if (ifmt_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC) {// �ж��Ƿ���aac����
                DEMUXER_AAC = 1;
                out_filename_audio = "./avfile/demuxer.aac";
            }
        }       
    }

    printf("\nClient:  ==========Input Information==========\n");
	av_dump_format(ifmt_ctx, 0, input_flv_file, 0);  // ��ӡ��Ϣ
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
        // ��Ƶ֡
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
        // ��Ƶ֡
        else if (pkt.stream_index == audioindex) {
            if (DEMUXER_AAC) {
                if ((ret = aac_decode_extradata(&stADTSContext, ifmt_ctx->streams[audioindex]->codec->extradata, ifmt_ctx->streams[audioindex]->codec->extradata_size)) == -1) {
                    printf("Decode extradata failed!\n");
                    goto ERROR;
                } // ����AudioSpecificConfig�ṹ
			    aac_set_adts_head(&stADTSContext, pAdtsHead, pkt.size); // ����ADTsͷ
			    fwrite(pAdtsHead, 1, 7, fp_audio); // дADTsͷ
            }
            fwrite(pkt.data, 1, pkt.size, fp_audio);
        }
        av_packet_unref(&pkt);
    }

    printf("\nClient:  =====  Demuxer: %s, %s  =====\n", out_filename_audio, out_filename_video);
    /*
    * �ر��ļ� *��Ҫ������ᵼ�·����ļ���ȫ
    */
    fclose(fp_video);
    fclose(fp_audio);
    fp_audio = NULL;
    fp_video = NULL;

    /*
    * ��input �ļ�
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
    * ����socket
    */
    struct sockaddr_in server_address;
    __bzero(&server_address, sizeof(server_address));  // ��ʼ��

    server_address.sin_family = AF_INET;  // ��ַ��
    server_address.sin_port = htons(server_port);  // �˿�
    inet_pton(AF_INET, SERVER_ADDR, &server_address.sin_addr);  // socket ��ַ

    int sockfd = socket(PF_INET, SOCK_DGRAM, 0);  // ipv4, ���ݱ�����

    printf("\nClient:  =====  Start send avfile  =====\n");
    /*
    *  ���η�������
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