# video-audio_demo
一个音视频后台开发练习demo，实现以下功能：

> 客户端：从flv文件分离出音视频的数据，通过socket（udp）发送服务端，本地传输，不考虑丢包的情况；
> 服务器：把收到的音视频数据包封装成MP4文件存在本地磁盘。

我们将功能拆分，分为“复用/解复用”模块和“客户端/服务端”模块，我们分别对这两个模块进行了实现，demo存放在对应的文件夹中：

>复用/解复用demo(muxer_demuxer_demo)  
>客户端/服务端demo(client_server_demo)

最终，我们将两个模块的代码进行整合，并把项目的源码放在src文件夹中。

**配置文件：** 可以对输入的flv路径和服务器绑定端口进行配置，格式为“flv路径 + ‘ ’ + 端口”。

**运行方式：** chmod +x ./run.sh && ./run.sh

**运行结果：** 生成了output.mp4，存放在avfile文件夹中。

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
可以将本地的音频文件（input.aac）和视频文件（input.h264）通过UDP协议发送给服务端。考虑到IP分片和服务端接收速度等因素，我们一个UDP发送1024Bytes，发送40000Bytes字节后会sleep1秒，保证服务端顺利接收文件。

更新： 由于本次实验客户端-服务端 是在同一主机上模拟，所以不考虑丢包的情况。为了尽可能减少频繁IO带来的切换开销，我们把一次UDP发送的数据设置为最大65507（Centos下），MAX_SIZE_ONE_TIME_SEND设置为130000。 优化前后对比如下：

|| 传输音频文件大小 |传输音频文件用时|传输视频文件大小|传输视频文件用时|音频传输速率|视频传输速率|
|--|--|--|--|--|--|--|
|更新前| 1853466Bytes |44.72s|4563480Bytes|110.95s|40.47KB/s|40.17KB/s|
|更新后| 1853466Bytes |14.15s|4563480Bytes|34.37s|127.92KB/s|129.66KB/s|


### server
从客户端接收音视频文件并保存到本地（output.aac/ output.h264），考虑到写文件IO的耗时性，未来将尝试一次多写的方式来优化服务端接收文件的耗时。

编译client_server_demo： make all

运行1：
终端一：./client
终端二：./server

运行2：
终端：chmod +x ./run.sh && ./run.sh （不建议，客户端和服务端的输出会混淆）
