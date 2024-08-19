#include "config.h"
#include "tcpserver.h"
#include <QDebug>

TcpServer::TcpServer(QObject *parent)
    : QTcpServer{parent}, m_socket(nullptr)
{
    Config config;
    if (listen(QHostAddress(config.m_info.ip), config.m_info.port)) {
        qDebug() << "网络服务启动在端口：" << config.m_info.ip << ":" <<config.m_info.port;
        } else {
        qDebug() << "网络服务启动失败！" << errorString();
        }
}

TcpServer::~TcpServer()
{
    if(m_socket != nullptr)
    {
        delete m_socket;
        m_socket = nullptr;
    }
}

void TcpServer::incomingConnection(qintptr socketDescriptor)
{
    m_socket = new QTcpSocket(this);
    if (m_socket->setSocketDescriptor(socketDescriptor)) {
        connect(m_socket, &QTcpSocket::readyRead, this, &TcpServer::onReadyRead);
        connect(m_socket, &QTcpSocket::disconnected, this, &TcpServer::onDisconnected);
        qDebug() << "客户端接入: " << reinterpret_cast<qintptr>(m_socket);
    } else {
        delete m_socket;
        m_socket = nullptr;
        qDebug() << "客户端接入失败，销毁socket:" << reinterpret_cast<qintptr>(m_socket);
    }
}

void TcpServer::onReadyRead()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (clientSocket) {
        QByteArray data = clientSocket->readAll();
        qDebug() << "接收到客户端" << reinterpret_cast<qintptr>(clientSocket) <<"数据: " << data;
        emit message(data, clientSocket);
    }
}

void TcpServer::onDisconnected()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (clientSocket) {
        clientSocket->deleteLater();
        qDebug() << "客户端断开连接:" << reinterpret_cast<qintptr>(clientSocket);
    }
}
