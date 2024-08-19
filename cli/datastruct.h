#ifndef DATASTRUCT_H
#define DATASTRUCT_H
#include <QString>

// 定义包含事件类型、坐标、偏移量和事件的结构体
typedef struct {
    QString text = "";
    int x;
    int y;
    int event;
    int eventType;
} InputEvent;

typedef struct {
    int screen_width;
    int screen_height;
    int resolution_w;
    int resolution_h;
    QString ip;
    int port;
} Info;

#endif // DATASTRUCT_H
