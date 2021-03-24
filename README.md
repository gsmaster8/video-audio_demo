# video-audio_demo
一个音视频后台开发练习demo，包括复用/解复用demo(muxer_demuxer_demo)，客户端/服务端demo(client_server_demo)

## muxer_demuxer_demo
### demuxer
可将flv格式封装的文件解复用为音频和视频两个文件。视频支持h264编码格式，音频支持mp3、aac编码格式。
运行demuxer.c：make demuxer && ./demuxer
生成了两个文件 demuxer.h264 和 demuxer.aac 为解析出来的视频/音频编码数据。
