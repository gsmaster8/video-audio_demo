# video-audio_demo
一个音视频后台开发练习demo，包括两个模块：

复用/解复用demo(muxer_demuxer_demo)

客户端/服务端demo(client_server_demo)

最终项目源码放在src文件夹中。

## muxer_demuxer_demo

### demuxer
可将flv格式封装的文件解复用为音频和视频两个文件。视频支持h264编码格式，音频支持mp3、aac编码格式。

运行demuxer.c：make demuxer && ./demuxer

生成了两个文件 demuxer.h264 和 demuxer.aac 为解复用出来的视频/音频编码数据。

### muxer
可将音频流和视频流复用成mp4格式的音视频流。

运行muxer.c：make muxer && ./muxer

生成了一个 muxer.mp4 文件，为复用得到的音视频流。

## client_server_demo

### client

### server
