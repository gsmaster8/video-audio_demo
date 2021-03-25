#include <stdio.h>
#include <libavformat/avformat.h>

int main() {
    AVFormatContext *ifmt_ctx_v = NULL, *ifmt_ctx_a = NULL, *ofmt_ctx = NULL;
    AVOutputFormat *ofmt = NULL;
    AVPacket pkt;
    int ret, i;
    int videoindex_v = -1, videoindex_out = -1; // ��ʾ����/���video����index
    int audioindex_a = -1, audioindex_out = -1; // ��ʾ����/���audio����index
    int64_t cur_pts_v = 0, cur_pts_a = 0;
    int frame_index = 0;

    const char *in_filename_v = "muxer.h264";
    const char *in_filename_a = "muxer.aac";
    const char *out_filename = "muxer.mp4";

    // �������ļ�����ȡ��Ϣ
    if ((ret = avformat_open_input(&ifmt_ctx_v, in_filename_v, 0, 0)) < 0) {
        printf("Could not open input file!\n");
        goto END;
    }
    if ((ret = avformat_find_stream_info(ifmt_ctx_v, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		goto END;
	}

	if ((ret = avformat_open_input(&ifmt_ctx_a, in_filename_a, 0, 0)) < 0) {
		printf( "Could not open input file.");
		goto END;
	}
	if ((ret = avformat_find_stream_info(ifmt_ctx_a, 0)) < 0) {
		printf( "Failed to retrieve input stream information");
		goto END;
	}
    // �����Ϣ
    printf("===========Input Information==========\n");
	av_dump_format(ifmt_ctx_v, 0, in_filename_v, 0);
	av_dump_format(ifmt_ctx_a, 0, in_filename_a, 0);
	printf("======================================\n");

    // ��ʼ������ļ�
    avformat_alloc_output_context2(&ofmt_ctx, NULL, NULL, out_filename);
    if (!ofmt_ctx) {
		printf( "Could not create output context!\n");
		ret = AVERROR_UNKNOWN;
		goto END;
	}
    ofmt = ofmt_ctx->oformat; // ȡ�����ʽ�ľ��

    // Ϊ����ļ���������Ƶ�������ӱ������Ϣ
    for (i = 0; i < ifmt_ctx_v->nb_streams; i++) { // ��Ƶ
        AVStream *in_stream = ifmt_ctx_v->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        videoindex_v = i; // ����video�ļ���stream_index
        if (!out_stream) {
            printf( "Failed allocating output stream!\n");
            ret = AVERROR_UNKNOWN;
            goto END;
        }
        videoindex_out = out_stream->index; // ����ļ�video��stream_index
        // ���ӱ������Ϣ
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
            printf( "Failed to copy context from input to output stream codec context!\n");
            goto END;
        }
        out_stream->codec->codec_tag = 0; // ?
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) // �����Ҫglobal header
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        break;
    }
    
    for (i = 0; i < ifmt_ctx_a->nb_streams; i++) { // ��Ƶ
        AVStream *in_stream = ifmt_ctx_a->streams[i];
        AVStream *out_stream = avformat_new_stream(ofmt_ctx, in_stream->codec->codec);
        audioindex_a = i; // ����audio�ļ���stream_index
        if (!out_stream) {
            printf( "Failed allocating output stream!\n");
            ret = AVERROR_UNKNOWN;
            goto END;
        }
        audioindex_out = out_stream->index; // ����ļ�audio��stream_index
        if (avcodec_copy_context(out_stream->codec, in_stream->codec) < 0) {
            printf( "Failed to copy context from input to output stream codec context!\n");
            goto END;
        }
        out_stream->codec->codec_tag = 0;
        if (ofmt_ctx->oformat->flags & AVFMT_GLOBALHEADER)
            out_stream->codec->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;
        break;
	}

    // ���û�д���Ҫ�ֶ���
    if (!(ofmt->flags & AVFMT_NOFILE)) {
		if (avio_open(&ofmt_ctx->pb, out_filename, AVIO_FLAG_WRITE) < 0) {
			printf( "Could not open output file '%s'!\n", out_filename);
			goto END;
		}
	}
    // д�ļ�ͷ
    if (avformat_write_header(ofmt_ctx, NULL) < 0) {
		printf( "Error occurred when opening output file!\n");
		goto END;
	}

    // �����Ƶ����Ϣ
    printf("==========Output Information==========\n");
	av_dump_format(ofmt_ctx, 0, out_filename, 1);
	printf("======================================\n");

    // д��Ƶ/��Ƶ֡
    while (1) {
        AVFormatContext *ifmt_ctx = NULL;
        int in_stream_index = -1, out_stream_index = -1;
        AVStream *in_stream = NULL, *out_stream = NULL;

        // �ж���֡����Ƶ֡������Ƶ֡
        if(av_compare_ts(cur_pts_v, ifmt_ctx_v->streams[videoindex_v]->time_base, cur_pts_a, ifmt_ctx_a->streams[audioindex_a]->time_base) <= 0){ 
            // ѡ����Ƶ֡
            ifmt_ctx = ifmt_ctx_v;
            in_stream_index = videoindex_v; // ��λ�����ļ���stream
            out_stream_index = videoindex_out; 
        }
        else {
            // ѡ����Ƶ֡
            ifmt_ctx = ifmt_ctx_a;
            in_stream_index = audioindex_a;
            out_stream_index = audioindex_out;
        }

        // ��ȡһ֡
        if (av_read_frame(ifmt_ctx, &pkt) >= 0) {

            in_stream = ifmt_ctx->streams[in_stream_index];
            out_stream = ofmt_ctx->streams[out_stream_index];

            // �����֡û��PTS����Ҫ����
            if (pkt.pts == AV_NOPTS_VALUE) {
                AVRational time_base1 = in_stream->time_base;
                // 2֮֡��ĳ���ʱ��
                int64_t calc_duration = (double)AV_TIME_BASE / av_q2d(in_stream->r_frame_rate);
                // �������
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
        
        //��д PTS/DTS
		pkt.pts = av_rescale_q_rnd(pkt.pts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.dts = av_rescale_q_rnd(pkt.dts, in_stream->time_base, out_stream->time_base, (AV_ROUND_NEAR_INF|AV_ROUND_PASS_MINMAX));
		pkt.duration = av_rescale_q(pkt.duration, in_stream->time_base, out_stream->time_base);
		pkt.pos = -1;
		pkt.stream_index = out_stream_index;

        //printf("Write 1 Packet. size:%5d\tpts:%lld\n", pkt.size, pkt.pts);
		//д�ļ�
		if (av_interleaved_write_frame(ofmt_ctx, &pkt) < 0) {
			printf( "Error muxing packet\n");
			break;
		}
		av_packet_unref(&pkt);
    }

    // д�ļ�β
	av_write_trailer(ofmt_ctx);
    

END:
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