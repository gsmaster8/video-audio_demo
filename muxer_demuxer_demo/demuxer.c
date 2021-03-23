#include <stdio.h>
#include <libavformat/avformat.h>

#define  FFMPEG_NEW
#define  ADTS_HEADER_SIZE (7)

int DEMUXER_AAC = 0;

typedef struct {
	int write_adts;  
	int objecttype;  
	int sample_rate_index;  
	int channel_conf;  
}ADTSContext;
 
int aac_decode_extradata(ADTSContext *adts, unsigned char *pbuf, int bufsize) {  
	int aot, aotext, samfreindex;  
	int i, channelconfig;  
	unsigned char *p = pbuf;  
	if (!adts || !pbuf || bufsize<2) {  
		return -1;  
	}  
	aot = (p[0]>>3)&0x1f;  
	if (aot == 31) {  
		aotext = (p[0]<<3 | (p[1]>>5)) & 0x3f;  
		aot = 32 + aotext;  
		samfreindex = (p[1]>>1) & 0x0f;   
		if (samfreindex == 0x0f) {  
			channelconfig = ((p[4]<<3) | (p[5]>>5)) & 0x0f;  
		}  
		else {  
			channelconfig = ((p[1]<<3)|(p[2]>>5)) & 0x0f;  
		}  
	}  
	else  
	{  
		samfreindex = ((p[0]<<1)|p[1]>>7) & 0x0f;  
		if (samfreindex == 0x0f) {  
			channelconfig = (p[4]>>3) & 0x0f;  
		}  
		else {  
			channelconfig = (p[1]>>3) & 0x0f;  
		}  
	}  
#ifdef AOT_PROFILE_CTRL  
	if (aot < 2) aot = 2;  
#endif  
	adts->objecttype = aot-1;  
	adts->sample_rate_index = samfreindex;  
	adts->channel_conf = channelconfig;  
	adts->write_adts = 1;  
	return 0;  
}  
 
int aac_set_adts_head(ADTSContext *acfg, unsigned char *buf, int size) {         
	unsigned char byte;    
	if (size < ADTS_HEADER_SIZE) {  
		return -1;  
	}       
	buf[0] = 0xff;  
	buf[1] = 0xf1;  
	byte = 0;  
	byte |= (acfg->objecttype & 0x03) << 6;  
	byte |= (acfg->sample_rate_index & 0x0f) << 2;  
	byte |= (acfg->channel_conf & 0x07) >> 2;  
	buf[2] = byte;  
	byte = 0;  
	byte |= (acfg->channel_conf & 0x07) << 6;  
	byte |= (ADTS_HEADER_SIZE + size) >> 11;  
	buf[3] = byte;  
	byte = 0;  
	byte |= (ADTS_HEADER_SIZE + size) >> 3;  
	buf[4] = byte;  
	byte = 0;  
	byte |= ((ADTS_HEADER_SIZE + size) & 0x7) << 5;  
	byte |= (0x7ff >> 6) & 0x1f;  
	buf[5] = byte;  
	byte = 0;  
	byte |= (0x7ff & 0x3f) << 2;  
	buf[6] = byte;     
	return 0;  
}

int main() {
    AVFormatContext *ifmt_ctx = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex = -1, audioindex = -1;
    const char *in_filename = "fish.flv";
    const char *out_filename_v = "ffmpeg_demo.h264";
    char *out_filename_a = "ffmpeg_demo.mp3"; // 默认mp3编码

    //av_register_all(); 已废弃

    if ((ret = avformat_open_input(&ifmt_ctx, in_filename, 0, 0)) < 0) {
        printf("Could not open input file.");
        goto ERROR;
    }

    if ((ret = avformat_find_stream_info(ifmt_ctx, 0)) < 0) {
        printf("Failed to retrieve input stream information");
        goto ERROR;
    }

    videoindex = -1;
    for (i=0; i<ifmt_ctx->nb_streams; i++) { //nb_streams：视音频流的个数
        if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO)
            videoindex = i;
        else if (ifmt_ctx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audioindex = i;
            if (ifmt_ctx->streams[i]->codecpar->codec_id == AV_CODEC_ID_AAC) {// 判断是否是aac编码
                DEMUXER_AAC = 1;
                out_filename_a = "ffmpeg_demo.aac";
            }
        }
            
    }

    printf("\nInput Video===========================\n");
	av_dump_format(ifmt_ctx, 0, in_filename, 0);  // 打印信息
	printf("\n======================================\n");

    FILE *fp_audio=fopen(out_filename_a,"wb+");  
	FILE *fp_video=fopen(out_filename_v,"wb+");

#ifdef FFMPEG_NEW
    AVBSFContext *video_bsf_ctx = NULL; 
    const AVBitStreamFilter *video_filter = av_bsf_get_by_name("h264_mp4toannexb");
    if (video_filter == NULL) {
        printf("Get video bsf failed!\n");
        goto ERROR;
    }
#else 
    AVBitStreamFilterContext* h264bsfc =  av_bitstream_filter_init("h264_mp4toannexb");
#endif

    ADTSContext stADTSContext;
	unsigned char pAdtsHead[7];

    while (av_read_frame(ifmt_ctx, &pkt) >= 0) {
        if (pkt.stream_index == videoindex) {
#ifdef FFMPEG_NEW
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
#else
            av_bitstream_filter_filter(h264bsfc, ifmt_ctx->streams[videoindex]->codec, NULL, &pkt.data, &pkt.size, pkt.data, pkt.size, 0);
#endif
            printf("Write Video Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
            fwrite(pkt.data, 1, pkt.size, fp_video);
        }
        else if (pkt.stream_index == audioindex) {
            if (DEMUXER_AAC) {
                aac_decode_extradata(&stADTSContext, ifmt_ctx->streams[audioindex]->codec->extradata, ifmt_ctx->streams[audioindex]->codec->extradata_size);
			    aac_set_adts_head(&stADTSContext, pAdtsHead, pkt.size);
			    fwrite(pAdtsHead, 1, 7, fp_audio);
            }
            printf("Write Audio Packet. size:%d\tpts:%lld\n",pkt.size,pkt.pts);
            fwrite(pkt.data, 1, pkt.size, fp_audio);
        }
        av_packet_unref(&pkt);
    }
#ifdef FFMPEG_NEW
    av_bsf_free(&video_bsf_ctx);
#else
    av_bitstream_filter_close(h264bsfc); 
#endif

    fclose(fp_video);
	fclose(fp_audio);
 
	avformat_close_input(&ifmt_ctx);
    return 0;

ERROR:
    if (ifmt_ctx)
        avformat_close_input(&ifmt_ctx);
    if (fp_audio)
        fclose(fp_audio);
    if (fp_video)
        fclose(fp_video);
#ifdef FFMPEG_NEW
    if (video_bsf_ctx)
        av_bsf_free(&video_bsf_ctx);
#else
    if (h264bsfc)
        av_bitstream_filter_close(h264bsfc); 
#endif
    return -1;
}