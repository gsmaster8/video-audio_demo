#include <stdio.h>
#include <libavformat/avformat.h>

#define  NEW_FFMPEG_API 1
#define  ADTS_HEADER_SIZE (7)

int DEMUXER_AAC = 0; // 默认解复用为mp3

typedef struct {
	int write_adts;  
	int objecttype;  // 编码类型 占AudioSpecificConfig的 5bit
	int sample_rate_index;  //采样率 占AudioSpecificConfig的 4bit
	int channel_conf;  // 通道数 占AudioSpecificConfig的 4bit
}ADTSContext;

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
* 生成ADTs（7Bytes）插入到每一个音频帧前
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

int main() {
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex = -1, audioindex = -1;
    const char *in_filename = "fish.flv";
    const char *out_filename_v = "demuxer.h264";
    char *out_filename_a = "demuxer.mp3"; // 默认mp3编码

    //av_register_all(); 已废弃

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto END;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto END;
    }

    videoindex = -1;
    for (i=0; i<ifmt_ctx->nb_streams; i++) { //nb_streams：视音频流的个数
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            videoindex = i;
        else if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            if (ifmt_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC) {// 判断是否是aac编码
                DEMUXER_AAC = 1;
                out_filename_a = "demuxer.aac";
            }
        }  
    }

    printf("\nInput Video===========================\n");
	av_dump_format(ifmt_ctx, 0, in_filename, 0);  // 打印信息
	printf("\n======================================\n");

    FILE *fp_audio=fopen(out_filename_a,"wb+");  
	FILE *fp_video=fopen(out_filename_v,"wb+");

#if NEW_FFMPEG_API
    AVBSFContext *video_bsf_ctx = NULL; 
    const AVBitStreamFilter *video_filter = av_bsf_get_by_name("h264_mp4toannexb");
    if (video_filter == NULL) {
        printf("Get video bsf failed!\n");
        goto END;
    }
#else 
    AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb");
#endif

    ADTSContext stADTSContext = {0};
	unsigned char pAdtsHead[7];

    while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == videoindex) {
#if NEW_FFMPEG_API
            if ((ret = av_bsf_alloc(video_filter, &video_bsf_ctx)) != 0) {
                printf("Alloc video bsf failed!\n");
                goto END;
            }
            ret = avcodec_parameters_from_context(video_bsf_ctx->par_in, ifmt_ctx->streams[videoindex]->codec);
            if (ret < 0) {
                printf("Set video Codec failed!\n");
                goto END;
            }
            ret = av_bsf_init(video_bsf_ctx);
            if (ret < 0) {
                printf("Init video bsf failed!\n");
                goto END;
            }
            av_bsf_send_packet(video_bsf_ctx, &pkt);
            ret = av_bsf_receive_packet(video_bsf_ctx, &pkt);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
                break;
            else if (ret < 0) {
                printf("Receive video Pkt failed!\n");
                goto END;
            }
#else
            av_bitstream_filter_filter(h264bsfc, ifmt_ctx->streams[videoindex]->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
            //printf("Write Video Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
            fwrite(pkt.data, 1, pkt.size, fp_video);
        }
        else if (pkt.stream_index == audioindex) {
            if (DEMUXER_AAC) {
                if ((ret = aac_decode_extradata(&stADTSContext, ifmt_ctx->streams[audioindex]->codec->extradata, ifmt_ctx->streams[audioindex]->codec->extradata_size)) == -1) {
                    printf("Decode extradata failed!\n");
                    goto END;
                } // 解析AudioSpecificConfig结构
			    aac_set_adts_head(&stADTSContext, pAdtsHead, pkt.size); // 生成ADTs头
			    fwrite(pAdtsHead, 1, 7, fp_audio); // 写ADTs头
            }
            //printf("Write Audio Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
            fwrite(pkt.data, 1, pkt.size, fp_audio);
        }
        av_packet_unref(&pkt);
    }

END:
    fclose(fp_video);
	fclose(fp_audio);
    avformat_close_input(&ifmt_ctx);

#if NEW_FFMPEG_API
    av_bsf_free(&video_bsf_ctx);
#else
    av_bitstream_filter_close(h264bsfc); 
#endif
    if (ret < 0 && ret != AVERROR_EOF) {
		printf( "Error occurred.\n");
		return -1;
	}
    return 0;
}