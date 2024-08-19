#include "config.h"

Config::Config(QObject *parent)
    : QObject{parent}
{
    QString filePath = "/etc/wdotoold/wdotoold.json";

    // 打开文件
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        qWarning() << "无法打开配置文件:" << file.errorString();
        exit(-1);
    }

    // 读取文件内容
    QByteArray fileData = file.readAll();
    file.close();

    // 解析JSON数据
    QJsonDocument jsonDoc = QJsonDocument::fromJson(fileData);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qWarning() << "JSON解析失败或不是一个JSON对象";
        exit(-1);
    }

    QJsonObject jsonObj = jsonDoc.object();

    // 提取数据
    if (jsonObj.contains("screen_width")) {
        m_info.screen_width = jsonObj["screen_width"].toInt();
        qDebug() << "Screen Width:" << m_info.screen_width;
    }

    if (jsonObj.contains("screen_height")) {
        m_info.screen_height = jsonObj["screen_height"].toInt();
        qDebug() << "Screen Height:" << m_info.screen_height;
    }

    if (jsonObj.contains("resolution_w")) {
        m_info.resolution_w = jsonObj["resolution_w"].toInt();
        qDebug() << "Resolution:" << m_info.resolution_w;
    }
    if (jsonObj.contains("resolution_h")) {
        m_info.resolution_h = jsonObj["resolution_h"].toInt();
        qDebug() << "Resolution:" << m_info.resolution_h;
    }

    if (jsonObj.contains("ip")) {
        m_info.ip = jsonObj["ip"].toString();
        qDebug() << "IP:" << m_info.ip;
    }

    if (jsonObj.contains("port")) {
        m_info.port = jsonObj["port"].toInt();
        qDebug() << "Port:" << m_info.port;
    }

}
