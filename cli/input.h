#ifndef INPUT_H
#define INPUT_H

#include "config.h"
#include "tcpserver.h"
#include <QCoreApplication>
#include <QSet>
#include <QVector>
#include <QPoint>
#include <QJsonDocument>
#include <QJsonObject>
#include <QEventLoop>
#include <iostream>
#include <cmath>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <linux/uinput.h>
#ifndef USE_DEEPIN_KF5_WAYLAND
#include <X11/Xlib.h>
#else
#include <QApplication>
#include <QDesktopWidget>
#include <QMap>
#include <QMutex>
#include <QMutexLocker>
#include <QMimeData>
#include <QtConcurrent>
#include <KWayland/Client/registry.h>
#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/ddeseat.h>
#include <KWayland/Client/fakeinput.h>
#include <KWayland/Client/datacontrolsource.h>
#include <KWayland/Client/datacontroldevice.h>
#include <KWayland/Client/datacontroldevicemanager.h>
#include <KWayland/Client/datacontroloffer.h>
#endif

class Input : public QObject
{
    Q_OBJECT
public:
    explicit Input(QObject *parent = nullptr);
    ~Input();

public slots:
    InputEvent processEvent(QByteArray jsonstr);
    void writeEvent(int fd, int type, int code, int val);
    void check(int ioctlresult, const char* errormsg);
    int setupUinputDevice(Info info);
    void moveTo(int x, int y);
    void keyEvent(int code, int val);
    void scroll(int val);
    QByteArray createJsonString(int x, int y, QString text = "");
    QPoint getGlobalPosition();
    QPoint getGlobalScreenSize();
    void onMessage(QByteArray jsonstr, QTcpSocket *clientSocket);
    void setText(QString text);
    QString getText();
    QVector<QPoint> calculateIntermediatePoints(QPoint &start, QPoint &end, double step=1.0);
    int isDifferenceWithinLimit(QPoint &p1, QPoint &p2, int limit = 3);
    int calculateStep(int distance);
    void moveCursorToPosition(int x, int y);

#ifdef USE_DEEPIN_KF5_WAYLAND
    void initCursor();
    QPoint getAdded(QPoint &start, QPoint &end);
    void writeByte(const QString &mimeType, qint32 fd);
    void onDataControlDeviceV1(KWayland::Client::DataControlOfferV1* offer);
    void single_finger_down();
    void single_finger_up();
#endif
signals:
    void moveEvent(int x, int y);
    void exitLoop();

private:
    Config m_config;
    TcpServer m_server;
    int m_screen = -1;
    int m_mouse = -1;
    QEventLoop m_loop;
//    QSet<int> m_set_mouse_key = {BTN_MOUSE, BTN_LEFT, BTN_RIGHT, BTN_MIDDLE};
#ifdef USE_DEEPIN_KF5_WAYLAND
    int global_x = 0;
    int global_y = 0;
    bool single_flage = false;
    KWayland::Client::Registry * m_registry = Q_NULLPTR;
    KWayland::Client::Seat *m_seat = Q_NULLPTR;
    KWayland::Client::DataControlDeviceV1 *m_dataControlDeviceV1=Q_NULLPTR;
    KWayland::Client::DataControlSourceV1 *m_dataControlSourceV1=Q_NULLPTR;
    KWayland::Client::DataControlDeviceManager *m_dataControlDeviceManager = Q_NULLPTR;
    QMimeData *m_mimeDataRead=Q_NULLPTR;
    QMap<KWayland::Client::DataControlSourceV1 *, QMimeData *> m_sendDataMap;
#endif
};

#endif // INPUT_H
