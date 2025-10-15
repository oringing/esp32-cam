#ifndef HTTP_SERVER_H
#define HTTP_SERVER_H

// HTTP服务器配置
#define HTTP_SERVER_PORT 8080 //端口号
#define HTTP_MAX_OPEN_SOCKETS 1 //最大并发连接数
#define VIDEO_FPS 30 //帧率

void http_server_start(void);

#endif