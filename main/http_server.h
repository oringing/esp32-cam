#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

// HTTP服务器配置
#define HTTP_SERVER_PORT 80 //端口
#define HTTP_MAX_OPEN_SOCKETS 5 
#define VIDEO_FPS 60 //帧数

void http_server_start(void);

#endif