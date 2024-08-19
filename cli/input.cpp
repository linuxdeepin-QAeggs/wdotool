#include "input.h"
#include <QProcess>

Input::Input(QObject *parent)
    : QObject{parent}
{
    setupUinputDevice(m_config.m_info);
    connect(&m_server, &TcpServer::message, this, &Input::onMessage);
//    connect(this, &Input::exitLoop, &m_loop, &QEventLoop::quit);
    connect(this, &Input::moveEvent, this, &Input::moveCursorToPosition);
#ifdef USE_DEEPIN_KF5_WAYLAND
    m_registry = new KWayland::Client::Registry(this);
    m_registry->create(KWayland::Client::ConnectionThread::fromApplication(this));
    m_registry->setup();
    connect(m_registry, &KWayland::Client::Registry::seatAnnounced, this, [this] (quint32 name, quint32 version) {
       m_seat = m_registry->createSeat(name, version, this);
    });
    connect(m_registry, &KWayland::Client::Registry::dataControlDeviceManagerAnnounced, this, [this] (quint32 name, quint32 version) {
       m_dataControlDeviceManager = m_registry->createDataControlDeviceManager(name, version, this);
       if (m_dataControlDeviceManager != Q_NULLPTR)
       {
           m_dataControlDeviceV1=m_dataControlDeviceManager->getDataDevice(m_seat, this);
           if (!m_dataControlDeviceV1)
               return;
           connect(m_dataControlDeviceV1, &KWayland::Client::DataControlDeviceV1::dataOffered,
                   this, &Input::onDataControlDeviceV1);
       }
    });
#endif
}

Input::~Input()
{
    close(m_screen);
    close(m_mouse);
}

void Input::setText(QString text)
{
#ifndef USE_DEEPIN_KF5_WAYLAND
    QProcess process;
    process.start("xclip", QStringList() << "-selection" << "clipboard");
    process.write(text.toUtf8());
    process.closeWriteChannel();
    process.waitForFinished();
#else
    m_dataControlSourceV1 = m_dataControlDeviceManager->createDataSource(this);
    connect(m_dataControlSourceV1, &KWayland::Client::DataControlSourceV1::sendDataRequested, this, &Input::writeByte);
    QMimeData* m_mimeData = new QMimeData();
    m_mimeData->setText(text);
    m_sendDataMap.insert(m_dataControlSourceV1, m_mimeData);
    for (const QString &format : m_mimeData->formats()) {
        m_dataControlSourceV1->offer(format);
    }
    m_dataControlDeviceV1->setSelection(0, m_dataControlSourceV1);
#endif
}

QString Input::getText()
{
#ifndef USE_DEEPIN_KF5_WAYLAND
    QProcess process;
    process.start("xclip", QStringList() << "-selection" << "clipboard" << "-o");
    process.waitForFinished();
    QString clipboardText = process.readAll();
    return clipboardText;
#else
    return m_mimeDataRead->text();
#endif
}

#ifdef USE_DEEPIN_KF5_WAYLAND
void Input::writeByte(const QString &mimeType, qint32 fd)
{
    KWayland::Client::DataControlSourceV1 *dataSource = qobject_cast<KWayland::Client::DataControlSourceV1*>(sender());
    QFile f;
    if (f.open(fd, QFile::WriteOnly, QFile::AutoCloseHandle)) {
        QByteArray content = m_sendDataMap[dataSource]->text().toUtf8();
        const QByteArray &ba = content;
        f.write(ba);
        f.close();
        disconnect(dataSource);
        dataSource->destroy();
        delete (m_sendDataMap[dataSource]);
        delete dataSource;
        m_sendDataMap.remove(dataSource);
    }
}
void Input::onDataControlDeviceV1(KWayland::Client::DataControlOfferV1* offer)
{
    qDebug() << "data offered";
    if (!offer)
        return;

    if(m_mimeDataRead==nullptr)
    {
        m_mimeDataRead=new QMimeData();
    }else {
        delete m_mimeDataRead;
        m_mimeDataRead=new QMimeData();
    }
    m_mimeDataRead->clear();

    QList<QString> mimeTypeList = offer->offeredMimeTypes();
    int mimeTypeCount = mimeTypeList.count();

    // 将所有的数据插入到mime data中
    static QMutex setMimeDataMutex;
    static int mimeTypeIndex = 0;
    mimeTypeIndex = 0;
    for (const QString &mimeType : mimeTypeList) {
        int pipeFds[2];
        if (pipe(pipeFds) != 0) {
            qWarning() << "Create pipe failed.";
            return;
        }
        fcntl(pipeFds[0], F_SETFD, FD_CLOEXEC);
        fcntl(pipeFds[0], F_SETFL, O_SYNC);
        fcntl(pipeFds[1], F_SETFD, FD_CLOEXEC);
        fcntl(pipeFds[1], F_SETFL, O_SYNC);
        // 根据mime类取数据，写入pipe中
        offer->receive(mimeType, pipeFds[1]);
        close(pipeFds[1]);
        // 异步从pipe中读取数据写入mime data中
        QtConcurrent::run([pipeFds, this, mimeType, mimeTypeCount] {
            QFile readPipe;
            if (readPipe.open(pipeFds[0], QIODevice::ReadOnly)) {
                if (readPipe.isReadable()) {
                    const QByteArray &data = readPipe.readAll();
                    if (!data.isEmpty()) {
                        // 需要加锁进行同步，否则可能会崩溃
                        QMutexLocker locker(&setMimeDataMutex);
                        m_mimeDataRead->setData(mimeType, data);
                    } else {
                        qWarning() << "Pipe data is empty, mime type: " << mimeType;
                    }
                } else {
                    qWarning() << "Pipe is not readable";
                }
            } else {
                qWarning() << "Open pipe failed!";
            }
            close(pipeFds[0]);
            if (++mimeTypeIndex >= mimeTypeCount) {
                mimeTypeIndex = 0;
            }
        });
    }
}

void Input::single_finger_down()
{
    writeEvent(m_screen,EV_ABS,ABS_MT_TRACKING_ID,0x24);
    writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_X,global_x);
    writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_Y,global_y);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_X,global_x);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_Y,global_y);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOUCH_MAJOR,0x3);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOUCH_MINOR,0x3);
    writeEvent(m_screen,EV_KEY,BTN_TOUCH,0x1);
    writeEvent(m_screen,EV_ABS,ABS_X,global_x);
    writeEvent(m_screen,EV_ABS,ABS_Y,global_y);
    writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
    writeEvent(m_screen,EV_SYN,SYN_REPORT,0x0);
    single_flage = true;
    usleep(5000); // 延迟 5ms
    writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
    writeEvent(m_screen,EV_SYN,SYN_REPORT,0x0);
    usleep(5000);
}

void Input::single_finger_up()
{
    writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_X,global_x);
    writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_Y,global_y);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_X,global_x);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_Y,global_y);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOUCH_MAJOR,0x0);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOUCH_MINOR,0x0);
    writeEvent(m_screen,EV_ABS,ABS_X,global_x);
    writeEvent(m_screen,EV_ABS,ABS_Y,global_y);
    writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
    writeEvent(m_screen,EV_SYN,SYN_REPORT,0x0);
    usleep(5000);
    writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
    writeEvent(m_screen,EV_SYN,SYN_REPORT,0x0);
    usleep(5000);
    writeEvent(m_screen,EV_ABS,ABS_MT_TRACKING_ID,-1);
    writeEvent(m_screen,EV_KEY,BTN_TOUCH,0x0);
    writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
    writeEvent(m_screen,EV_SYN,SYN_REPORT,0x0);
    single_flage = false;
    usleep(5000); // 延迟 5ms
}

QPoint Input::getAdded(QPoint &start, QPoint &end)
{
    //uinput操作和窗管坐标之间补偿
    int added_x=0;
    int added_y=0;
    if(end.x() > start.x())
    {
        added_x=1;
    }else if(end.x() < start.x())
    {
        added_x=-1;
    }

    if(end.y() > start.y())
    {
        added_y=1;
    }else if(end.y() < start.y())
    {
        added_y=-1;
    }
    QPoint point(added_x, added_y);
    return point;
}

void Input::initCursor()
{
    writeEvent(m_screen,EV_ABS,ABS_MT_TRACKING_ID,0x24);
    writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_X,960);
    writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_Y,540);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_X,960);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_Y,540);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOUCH_MAJOR,0x3);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOUCH_MINOR,0x3);
    writeEvent(m_screen,EV_KEY,BTN_TOUCH,0x1);
    writeEvent(m_screen,EV_ABS,ABS_X,960);
    writeEvent(m_screen,EV_ABS,ABS_Y,540);
    writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
    writeEvent(m_screen,EV_SYN,SYN_REPORT,0x0);

    writeEvent(m_screen,EV_ABS,ABS_MT_TOUCH_MAJOR,0x0);
    writeEvent(m_screen,EV_ABS,ABS_MT_TOUCH_MINOR,0x0);
    writeEvent(m_screen,EV_ABS,ABS_MT_TRACKING_ID,-1);
    writeEvent(m_screen,EV_KEY,BTN_TOUCH,0x0);
    writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
    writeEvent(m_screen,EV_SYN,SYN_REPORT,0x0);
    global_x=960;
    global_y=540;
}

#endif
void Input::onMessage(QByteArray jsonstr, QTcpSocket *clientSocket)
{
    InputEvent inputEvent = processEvent(jsonstr);
    switch (inputEvent.eventType) {
        case EV_KEY:
            keyEvent(inputEvent.event, inputEvent.x);
            break;
        case EV_REL:
            if (inputEvent.event == REL_WHEEL)
            {
                scroll(inputEvent.x);
            }
            break;
        case EV_ABS:
            moveTo(inputEvent.x, inputEvent.y);
            clientSocket->write(createJsonString(0, 0));
            clientSocket->flush();
            break;
        case (EV_MAX+1):
            {
                // 向客户端发送应答
                QPoint point = getGlobalPosition();
                clientSocket->write(createJsonString(point.x(), point.y()));
                clientSocket->flush();
            }
            break;
        case (EV_MAX+2):
            {
                // 向客户端发送应答
                QPoint point = getGlobalScreenSize();
                clientSocket->write(createJsonString(point.x(), point.y()));
                clientSocket->flush();
            }
            break;
        case (EV_MAX+3):
            {
                this->setText(inputEvent.text);
            }
            break;
        case (EV_MAX+4):
            {
                // 向客户端发送应答
                clientSocket->write(createJsonString(0, 0, getText()));
                clientSocket->flush();
            }
            break;
        default:
            printf("Unknown event\n");
            break;
    }
}


QPoint Input::getGlobalPosition()
{
#ifndef USE_DEEPIN_KF5_WAYLAND
    Display *display;
    Window root_window;
    Window returned_root_window, returned_child_window;
    int root_x, root_y;
    int win_x, win_y;
    unsigned int mask;

    // 打开与 X server 的连接
    display = XOpenDisplay(nullptr);
    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        exit(-1);
    }

    // 获取默认屏幕的根窗口
    root_window = DefaultRootWindow(display);

        // 获取光标的位置
    if (XQueryPointer(display, root_window, &returned_root_window, &returned_child_window,
                      &root_x, &root_y, &win_x, &win_y, &mask)) {
    } else {
        fprintf(stderr, "获取光标位置失败\n");
    }

    // 关闭与 X server 的连接
    XCloseDisplay(display);
    QPoint point(root_x, root_y);
    return point;
#else
    QPoint point(global_x, global_y);
    return point;
#endif
}

QPoint Input::getGlobalScreenSize()
{
#ifndef USE_DEEPIN_KF5_WAYLAND
    // 1. 打开与 X 服务器的连接
    Display *display = XOpenDisplay(nullptr);
    if (display == nullptr) {
        fprintf(stderr, "Unable to open X display");
        exit(-1);
    }

    // 2. 获取默认屏幕的尺寸
    int screen = DefaultScreen(display);  // 获取默认屏幕编号
    int width = DisplayWidth(display, screen);  // 获取屏幕宽度
    int height = DisplayHeight(display, screen);  // 获取屏幕高度


    // 4. 关闭 X 服务器的连接
    XCloseDisplay(display);
    QPoint point(width, height);
    return point;
#else
    int width=QApplication::desktop()->width();
    int height=QApplication::desktop()->height();
    QPoint size(width,height);
    return size;
#endif
}

InputEvent Input::processEvent(QByteArray jsonstr) {

    // 将JSON数据转换为InputEvent结构体
    InputEvent inputEvent;
    // 解析 JSON 数据
    QJsonDocument jsonDoc = QJsonDocument::fromJson(jsonstr);
    if (jsonDoc.isNull() || !jsonDoc.isObject()) {
        qWarning() << "JSON 解析失败或不是一个 JSON 对象";
        exit(-1);
    }

    QJsonObject jsonObj = jsonDoc.object();

    // 提取数据
    if (jsonObj.contains("eventType") && jsonObj["eventType"].isDouble()) {
        inputEvent.eventType = jsonObj["eventType"].toInt();
        //qDebug() << "eventType:" << inputEvent.eventType;
    }

    if (jsonObj.contains("x") && jsonObj["x"].isDouble()) {
        inputEvent.x = jsonObj["x"].toInt();
        //qDebug() << "x:" << inputEvent.x;
    }

    if (jsonObj.contains("y") && jsonObj["y"].isDouble()) {
        inputEvent.y = jsonObj["y"].toInt();
        //qDebug() << "y:" << inputEvent.y;
    }

    if (jsonObj.contains("event") && jsonObj["event"].isDouble()) {
        inputEvent.event = jsonObj["event"].toInt();
        //qDebug() << "event:" << inputEvent.event;
    }

    if (jsonObj.contains("text") && jsonObj["text"].isString()) {
        inputEvent.text = jsonObj["text"].toString();
        //qDebug() << "text:" << inputEvent.text;
    }

    // 打印结构体内容作为示例
    printf("Event: %d X: %d Y: %d\n", inputEvent.event, inputEvent.x, inputEvent.y);

    return inputEvent;
}

//写入事件
void Input::writeEvent(int fd, int type, int code, int val)
{
    struct timeval time;
    struct input_event ie;
    gettimeofday(&time, NULL);
    ie.type = type;
    ie.code = code;
    ie.value = val;
    /* timestamp values below are ignored */
    ie.time.tv_sec = time.tv_sec;
    ie.time.tv_usec = time.tv_usec;
    if(code == MSC_TIMESTAMP)
    {
        ie.value = time.tv_sec * 1000000 + time.tv_usec;
    }
//    ie.time.tv_sec = 0;
//    ie.time.tv_usec = 0;
    write(fd, &ie, sizeof(ie));
}

//检查ioctl结果
void Input::check(int ioctlresult, const char* errormsg)
{
    if (ioctlresult < 0)
    {
        printf("ioctl failed: %s\n", errormsg);
        exit(-1);
    }
}


//创建并设置设备
int Input::setupUinputDevice(Info info) {
    //创建键盘设备
    m_screen = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (m_screen < 0) {
        perror("Unable to open /dev/uinput");
        return m_screen;
    }

    //激活同步事件
    check(ioctl(m_screen, UI_SET_EVBIT, EV_SYN), "UI_SET_EVBIT EV_SYN");

    check(ioctl(m_screen, UI_SET_EVBIT, EV_KEY), "UI_SET_EVBIT, EV_KEY");
    check(ioctl(m_screen, UI_SET_KEYBIT, BTN_TOUCH), "UI_SET_KEYBIT BTN_TOUCH");
    check(ioctl(m_screen, UI_SET_KEYBIT, ABS_MT_TOUCH_MAJOR), "UI_SET_KEYBIT ABS_MT_TOUCH_MAJOR");
    check(ioctl(m_screen, UI_SET_KEYBIT, ABS_MT_TOUCH_MINOR), "UI_SET_KEYBIT ABS_MT_TOUCH_MINOR");

    //设置支持绝对坐标事件
    check(ioctl(m_screen, UI_SET_PROPBIT, INPUT_PROP_DIRECT), "UI_SET_PROPBIT INPUT_PROP_POINTING_STICK");
    check(ioctl(m_screen, UI_SET_EVBIT, EV_ABS), "UI_SET_EVBIT EV_ABS");
    check(ioctl(m_screen, UI_SET_ABSBIT, ABS_X), "UI_SETEVBIT ABS_X");
    check(ioctl(m_screen, UI_SET_ABSBIT, ABS_Y), "UI_SETEVBIT ABS_Y");
    check(ioctl(m_screen, UI_SET_ABSBIT, ABS_MT_SLOT), "UI_SET_ABSBIT ABS_MT_SLOT");
    check(ioctl(m_screen, UI_SET_ABSBIT, ABS_MT_TRACKING_ID), "UI_SET_ABSBIT ABS_MT_TRACKING_ID");
    check(ioctl(m_screen, UI_SET_ABSBIT, ABS_MT_POSITION_X), "UI_SET_ABSBIT ABS_MT_POSITION_X");
    check(ioctl(m_screen, UI_SET_ABSBIT, ABS_MT_POSITION_Y), "UI_SET_ABSBIT ABS_MT_POSITION_Y");
    check(ioctl(m_screen, UI_SET_ABSBIT, ABS_MT_TOOL_X), "UI_SET_ABSBIT ABS_MT_TOOL_X");
    check(ioctl(m_screen, UI_SET_ABSBIT, ABS_MT_TOOL_Y), "UI_SET_ABSBIT ABS_MT_TOOL_Y");

    check(ioctl(m_screen, UI_SET_EVBIT, EV_MSC), "UI_SET_EVBIT EV_MSC");
    check(ioctl(m_screen, UI_SET_MSCBIT, MSC_TIMESTAMP), "UI_SETEVBIT MSC_TIMESTAMP");


    //设置虚拟设备版本信息
    struct uinput_setup usetup_keyboard;
    memset(&usetup_keyboard, 0, sizeof(usetup_keyboard));
    snprintf(usetup_keyboard.name, UINPUT_MAX_NAME_SIZE, "wdotool-keyboard");
    usetup_keyboard.id.bustype = BUS_I2C;
    usetup_keyboard.id.vendor  = 0x04f3; // wacom
    usetup_keyboard.id.product = 0x2841;
    usetup_keyboard.id.version = 0x1;
    usetup_keyboard.ff_effects_max = 0;


    //设置绝对坐标事件依赖的屏幕分辨率信息
    struct uinput_abs_setup xabs;
    xabs.code = ABS_X;
    xabs.absinfo.minimum = 0;
    xabs.absinfo.maximum = info.screen_width;
    xabs.absinfo.fuzz = 0;
    xabs.absinfo.flat = 0;
    xabs.absinfo.resolution = info.resolution_w;
    xabs.absinfo.value = 0;

    struct uinput_abs_setup yabs;
    yabs.code = ABS_Y;
    yabs.absinfo.minimum = 0;
    yabs.absinfo.maximum = info.screen_height;
    yabs.absinfo.fuzz = 0;
    yabs.absinfo.flat = 0;
    yabs.absinfo.resolution = info.resolution_h;
    yabs.absinfo.value = 0;

    struct uinput_abs_setup mpxabs;
    mpxabs.code = ABS_MT_POSITION_X;
    mpxabs.absinfo.minimum = 0;
    mpxabs.absinfo.maximum = info.screen_width;
    mpxabs.absinfo.fuzz = 0;
    mpxabs.absinfo.flat = 0;
    mpxabs.absinfo.resolution = info.resolution_w;
    mpxabs.absinfo.value = 0;

    struct uinput_abs_setup mpyabs;
    mpyabs.code = ABS_MT_POSITION_Y;
    mpyabs.absinfo.minimum = 0;
    mpyabs.absinfo.maximum = info.screen_height;
    mpyabs.absinfo.fuzz = 0;
    mpyabs.absinfo.flat = 0;
    mpyabs.absinfo.resolution = info.resolution_h;
    mpyabs.absinfo.value = 0;

    struct uinput_abs_setup mtxabs;
    mtxabs.code = ABS_MT_TOOL_X;
    mtxabs.absinfo.minimum = 0;
    mtxabs.absinfo.maximum = info.screen_width;
    mtxabs.absinfo.fuzz = 0;
    mtxabs.absinfo.flat = 0;
    mtxabs.absinfo.resolution = info.resolution_w;
    mtxabs.absinfo.value = 0;

    struct uinput_abs_setup mtyabs;
    mtyabs.code = ABS_MT_TOOL_Y;
    mtyabs.absinfo.minimum = 0;
    mtyabs.absinfo.maximum = info.screen_height;
    mtyabs.absinfo.fuzz = 0;
    mtyabs.absinfo.flat = 0;
    mtyabs.absinfo.resolution = info.resolution_h;
    mtyabs.absinfo.value = 0;

    struct uinput_abs_setup msolt;
    msolt.code = ABS_MT_SLOT;
    msolt.absinfo.minimum = 0;
    msolt.absinfo.maximum = 9;
    msolt.absinfo.fuzz = 0;
    msolt.absinfo.flat = 0;
    msolt.absinfo.resolution = 0;
    msolt.absinfo.value = 0;

    struct uinput_abs_setup tid;
    tid.code = ABS_MT_TRACKING_ID;
    tid.absinfo.minimum = 0;
    tid.absinfo.maximum = UINT16_MAX;
    tid.absinfo.fuzz = 0;
    tid.absinfo.flat = 0;
    tid.absinfo.resolution = 0;
    tid.absinfo.value = 0;

    struct uinput_abs_setup ma;
    ma.code = ABS_MT_TOUCH_MAJOR;
    ma.absinfo.minimum = 0;
    ma.absinfo.maximum = 255;
    ma.absinfo.fuzz = 0;
    ma.absinfo.flat = 0;
    ma.absinfo.resolution = 0;
    ma.absinfo.value = 0;

    struct uinput_abs_setup mi;
    mi.code = ABS_MT_TOUCH_MINOR;
    mi.absinfo.minimum = 0;
    mi.absinfo.maximum = 255;
    mi.absinfo.fuzz = 0;
    mi.absinfo.flat = 0;
    mi.absinfo.resolution = 0;
    mi.absinfo.value = 0;


    check(ioctl(m_screen, UI_DEV_SETUP, &usetup_keyboard), "UI_DEV_SETUP");
    check(ioctl(m_screen, UI_ABS_SETUP, &xabs), "ABS_X setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &yabs), "ABS_Y setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &mpxabs), "ABS_MT_POSITION_X setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &mpyabs), "ABS_MT_POSITION_Y setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &mtxabs), "ABS_MT_TOOL_X setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &mtyabs), "ABS_MT_TOOL_Y setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &tid), "ABS_MT_TRACKING_ID setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &ma), "ABS_MT_TOUCH_MAJOR setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &mi), "ABS_MT_TOUCH_MINOR setup");
    check(ioctl(m_screen, UI_ABS_SETUP, &msolt), "ABS_MT_SLOT setup");

    check(ioctl(m_screen, UI_DEV_CREATE), "device creation");
    sleep(2);

//==============================================================
    //创建鼠标设备
    m_mouse = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (m_mouse < 0) {
        perror("Unable to open /dev/uinput");
        return m_mouse;
    }
    //激活同步事件
    check(ioctl(m_mouse, UI_SET_EVBIT, EV_SYN), "UI_SET_EVBIT EV_SYN");
    //设置支持鼠标支持的所有事件
    check(ioctl(m_mouse, UI_SET_EVBIT, EV_REL), "UI_SET_EVBIT, EV_REL");
    check(ioctl(m_mouse, UI_SET_RELBIT, REL_X), "UI_SET_RELBIT, REL_X");
    check(ioctl(m_mouse, UI_SET_RELBIT, REL_Y), "UI_SET_RELBIT, REL_Y");
    check(ioctl(m_mouse, UI_SET_RELBIT, REL_WHEEL), "UI_SET_RELBIT, REL_WHEEL");
    check(ioctl(m_mouse, UI_SET_RELBIT, REL_HWHEEL), "UI_SET_RELBIT, REL_HWHEEL");

    check(ioctl(m_mouse, UI_SET_EVBIT, EV_KEY), "UI_SET_EVBIT EV_KEY");
    static const int key_list[] = {KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_LEFTSHIFT, KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_KPASTERISK, KEY_LEFTALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, KEY_SCROLLLOCK, KEY_KP7, KEY_KP8, KEY_KP9, KEY_KPMINUS, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KPPLUS, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP0, KEY_KPDOT, KEY_ZENKAKUHANKAKU, KEY_102ND, KEY_F11, KEY_F12, KEY_RO, KEY_KATAKANA, KEY_HIRAGANA, KEY_HENKAN, KEY_KATAKANAHIRAGANA, KEY_MUHENKAN, KEY_KPJPCOMMA, KEY_KPENTER, KEY_RIGHTCTRL, KEY_KPSLASH, KEY_SYSRQ, KEY_RIGHTALT, KEY_LINEFEED, KEY_HOME, KEY_UP, KEY_PAGEUP, KEY_LEFT, KEY_RIGHT, KEY_END, KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE, KEY_MACRO, KEY_MUTE, KEY_VOLUMEDOWN, KEY_VOLUMEUP, KEY_POWER, KEY_KPEQUAL, KEY_KPPLUSMINUS, KEY_PAUSE, KEY_SCALE, KEY_KPCOMMA, KEY_HANGEUL, KEY_HANGUEL, KEY_HANJA, KEY_YEN, KEY_LEFTMETA, KEY_RIGHTMETA, KEY_COMPOSE, KEY_STOP, KEY_AGAIN, KEY_PROPS, KEY_UNDO, KEY_FRONT, KEY_COPY, KEY_OPEN, KEY_PASTE, KEY_FIND, KEY_CUT, KEY_HELP, KEY_MENU, KEY_CALC, KEY_SETUP, KEY_SLEEP, KEY_WAKEUP, KEY_FILE, KEY_SENDFILE, KEY_DELETEFILE, KEY_XFER, KEY_PROG1, KEY_PROG2, KEY_WWW, KEY_MSDOS, KEY_COFFEE, KEY_SCREENLOCK, KEY_ROTATE_DISPLAY, KEY_DIRECTION, KEY_CYCLEWINDOWS, KEY_MAIL, KEY_BOOKMARKS, KEY_COMPUTER, KEY_BACK, KEY_FORWARD, KEY_CLOSECD, KEY_EJECTCD, KEY_EJECTCLOSECD, KEY_NEXTSONG, KEY_PLAYPAUSE, KEY_PREVIOUSSONG, KEY_STOPCD, KEY_RECORD, KEY_REWIND, KEY_PHONE, KEY_ISO, KEY_CONFIG, KEY_HOMEPAGE, KEY_REFRESH, KEY_EXIT, KEY_MOVE, KEY_EDIT, KEY_SCROLLUP, KEY_SCROLLDOWN, KEY_KPLEFTPAREN, KEY_KPRIGHTPAREN, KEY_NEW, KEY_REDO, KEY_F13, KEY_F14, KEY_F15, KEY_F16, KEY_F17, KEY_F18, KEY_F19, KEY_F20, KEY_F21, KEY_F22, KEY_F23, KEY_F24, KEY_PLAYCD, KEY_PAUSECD, KEY_PROG3, KEY_PROG4, KEY_DASHBOARD, KEY_SUSPEND, KEY_CLOSE, KEY_PLAY, KEY_FASTFORWARD, KEY_BASSBOOST, KEY_PRINT, KEY_HP, KEY_CAMERA, KEY_SOUND, KEY_QUESTION, KEY_EMAIL, KEY_CHAT, KEY_SEARCH, KEY_CONNECT, KEY_FINANCE, KEY_SPORT, KEY_SHOP, KEY_ALTERASE, KEY_CANCEL, KEY_BRIGHTNESSDOWN, KEY_BRIGHTNESSUP, KEY_MEDIA, KEY_SWITCHVIDEOMODE, KEY_KBDILLUMTOGGLE, KEY_KBDILLUMDOWN, KEY_KBDILLUMUP, KEY_SEND, KEY_REPLY, KEY_FORWARDMAIL, KEY_SAVE, KEY_DOCUMENTS, KEY_BATTERY, KEY_BLUETOOTH, KEY_WLAN, KEY_UWB, KEY_UNKNOWN, KEY_VIDEO_NEXT, KEY_VIDEO_PREV, KEY_BRIGHTNESS_CYCLE, KEY_BRIGHTNESS_AUTO, KEY_BRIGHTNESS_ZERO, KEY_DISPLAY_OFF, KEY_WWAN, KEY_WIMAX, KEY_RFKILL, KEY_MICMUTE, KEY_OK, KEY_SELECT, KEY_GOTO, KEY_CLEAR, KEY_POWER2, KEY_OPTION, KEY_INFO, KEY_TIME, KEY_VENDOR, KEY_ARCHIVE, KEY_PROGRAM, KEY_CHANNEL, KEY_FAVORITES, KEY_EPG, KEY_PVR, KEY_MHP, KEY_LANGUAGE, KEY_TITLE, KEY_SUBTITLE, KEY_ANGLE, KEY_ZOOM, KEY_MODE, KEY_KEYBOARD, KEY_SCREEN, KEY_PC, KEY_TV, KEY_TV2, KEY_VCR, KEY_VCR2, KEY_SAT, KEY_SAT2, KEY_CD, KEY_TAPE, KEY_RADIO, KEY_TUNER, KEY_PLAYER, KEY_TEXT, KEY_DVD, KEY_AUX, KEY_MP3, KEY_AUDIO, KEY_VIDEO, KEY_DIRECTORY, KEY_LIST, KEY_MEMO, KEY_CALENDAR, KEY_RED, KEY_GREEN, KEY_YELLOW, KEY_BLUE, KEY_CHANNELUP, KEY_CHANNELDOWN, KEY_FIRST, KEY_LAST, KEY_AB, KEY_NEXT, KEY_RESTART, KEY_SLOW, KEY_SHUFFLE, KEY_BREAK, KEY_PREVIOUS, KEY_DIGITS, KEY_TEEN, KEY_TWEN, KEY_VIDEOPHONE, KEY_GAMES, KEY_ZOOMIN, KEY_ZOOMOUT, KEY_ZOOMRESET, KEY_WORDPROCESSOR, KEY_EDITOR, KEY_SPREADSHEET, KEY_GRAPHICSEDITOR, KEY_PRESENTATION, KEY_DATABASE, KEY_NEWS, KEY_VOICEMAIL, KEY_ADDRESSBOOK, KEY_MESSENGER, KEY_DISPLAYTOGGLE, KEY_BRIGHTNESS_TOGGLE, KEY_SPELLCHECK, KEY_LOGOFF, KEY_DOLLAR, KEY_EURO, KEY_FRAMEBACK, KEY_FRAMEFORWARD, KEY_CONTEXT_MENU, KEY_MEDIA_REPEAT, KEY_10CHANNELSUP, KEY_10CHANNELSDOWN, KEY_IMAGES, KEY_DEL_EOL, KEY_DEL_EOS, KEY_INS_LINE, KEY_DEL_LINE, KEY_FN, KEY_FN_ESC, KEY_FN_F1, KEY_FN_F2, KEY_FN_F3, KEY_FN_F4, KEY_FN_F5, KEY_FN_F6, KEY_FN_F7, KEY_FN_F8, KEY_FN_F9, KEY_FN_F10, KEY_FN_F11, KEY_FN_F12, KEY_FN_1, KEY_FN_2, KEY_FN_D, KEY_FN_E, KEY_FN_F, KEY_FN_S, KEY_FN_B, KEY_BRL_DOT1, KEY_BRL_DOT2, KEY_BRL_DOT3, KEY_BRL_DOT4, KEY_BRL_DOT5, KEY_BRL_DOT6, KEY_BRL_DOT7, KEY_BRL_DOT8, KEY_BRL_DOT9, KEY_BRL_DOT10, KEY_NUMERIC_0, KEY_NUMERIC_1, KEY_NUMERIC_2, KEY_NUMERIC_3, KEY_NUMERIC_4, KEY_NUMERIC_5, KEY_NUMERIC_6, KEY_NUMERIC_7, KEY_NUMERIC_8, KEY_NUMERIC_9, KEY_NUMERIC_STAR, KEY_NUMERIC_POUND, KEY_NUMERIC_A, KEY_NUMERIC_B, KEY_NUMERIC_C, KEY_NUMERIC_D, KEY_CAMERA_FOCUS, KEY_WPS_BUTTON, KEY_TOUCHPAD_TOGGLE, KEY_TOUCHPAD_ON, KEY_TOUCHPAD_OFF, KEY_CAMERA_ZOOMIN, KEY_CAMERA_ZOOMOUT, KEY_CAMERA_UP, KEY_CAMERA_DOWN, KEY_CAMERA_LEFT, KEY_CAMERA_RIGHT, KEY_ATTENDANT_ON, KEY_ATTENDANT_OFF, KEY_ATTENDANT_TOGGLE, KEY_LIGHTS_TOGGLE, KEY_ALS_TOGGLE, KEY_BUTTONCONFIG, KEY_TASKMANAGER, KEY_JOURNAL, KEY_CONTROLPANEL, KEY_APPSELECT, KEY_SCREENSAVER, KEY_VOICECOMMAND, KEY_BRIGHTNESS_MIN, KEY_BRIGHTNESS_MAX, KEY_KBDINPUTASSIST_PREV, KEY_KBDINPUTASSIST_NEXT, KEY_KBDINPUTASSIST_PREVGROUP, KEY_KBDINPUTASSIST_NEXTGROUP, KEY_KBDINPUTASSIST_ACCEPT, KEY_KBDINPUTASSIST_CANCEL, KEY_RIGHT_UP, KEY_RIGHT_DOWN, KEY_LEFT_UP, KEY_LEFT_DOWN, KEY_ROOT_MENU, KEY_MEDIA_TOP_MENU, KEY_NUMERIC_11, KEY_NUMERIC_12, KEY_AUDIO_DESC, KEY_3D_MODE, KEY_NEXT_FAVORITE, KEY_STOP_RECORD, KEY_PAUSE_RECORD, KEY_VOD, KEY_UNMUTE, KEY_FASTREVERSE, KEY_SLOWREVERSE, KEY_DATA, KEY_MIN_INTERESTING, BTN_MISC, BTN_0, BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7, BTN_8, BTN_9, BTN_MOUSE, BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA, BTN_FORWARD, BTN_BACK, BTN_TASK, BTN_JOYSTICK, BTN_TRIGGER, BTN_THUMB, BTN_THUMB2, BTN_TOP, BTN_TOP2, BTN_PINKIE, BTN_BASE, BTN_BASE2, BTN_BASE3, BTN_BASE4, BTN_BASE5, BTN_BASE6, BTN_DEAD, BTN_GAMEPAD, BTN_SOUTH, BTN_A, BTN_EAST, BTN_B, BTN_C, BTN_NORTH, BTN_X, BTN_WEST, BTN_Y, BTN_Z, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR, BTN_DIGI, BTN_TOOL_PEN, BTN_TOOL_RUBBER, BTN_TOOL_BRUSH, BTN_TOOL_PENCIL, BTN_TOOL_AIRBRUSH, BTN_TOOL_FINGER, BTN_TOOL_MOUSE, BTN_TOOL_LENS, BTN_TOOL_QUINTTAP, BTN_TOUCH, BTN_STYLUS, BTN_STYLUS2, BTN_TOOL_DOUBLETAP, BTN_TOOL_TRIPLETAP, BTN_TOOL_QUADTAP, BTN_WHEEL, BTN_GEAR_DOWN, BTN_GEAR_UP, BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT, BTN_TRIGGER_HAPPY, BTN_TRIGGER_HAPPY1, BTN_TRIGGER_HAPPY2, BTN_TRIGGER_HAPPY3, BTN_TRIGGER_HAPPY4, BTN_TRIGGER_HAPPY5, BTN_TRIGGER_HAPPY6, BTN_TRIGGER_HAPPY7, BTN_TRIGGER_HAPPY8, BTN_TRIGGER_HAPPY9, BTN_TRIGGER_HAPPY10, BTN_TRIGGER_HAPPY11, BTN_TRIGGER_HAPPY12, BTN_TRIGGER_HAPPY13, BTN_TRIGGER_HAPPY14, BTN_TRIGGER_HAPPY15, BTN_TRIGGER_HAPPY16, BTN_TRIGGER_HAPPY17, BTN_TRIGGER_HAPPY18, BTN_TRIGGER_HAPPY19, BTN_TRIGGER_HAPPY20, BTN_TRIGGER_HAPPY21, BTN_TRIGGER_HAPPY22, BTN_TRIGGER_HAPPY23, BTN_TRIGGER_HAPPY24, BTN_TRIGGER_HAPPY25, BTN_TRIGGER_HAPPY26, BTN_TRIGGER_HAPPY27, BTN_TRIGGER_HAPPY28, BTN_TRIGGER_HAPPY29, BTN_TRIGGER_HAPPY30, BTN_TRIGGER_HAPPY31, BTN_TRIGGER_HAPPY32, BTN_TRIGGER_HAPPY33, BTN_TRIGGER_HAPPY34, BTN_TRIGGER_HAPPY35, BTN_TRIGGER_HAPPY36, BTN_TRIGGER_HAPPY37, BTN_TRIGGER_HAPPY38, BTN_TRIGGER_HAPPY39, BTN_TRIGGER_HAPPY40};
    for (int i=0; i<sizeof(key_list)/sizeof(int); i++) {
        check(ioctl(m_mouse, UI_SET_KEYBIT, key_list[i]), "UI_SET_KEYBIT LIST");
    }
    //设置虚拟设备版本信息
    struct uinput_setup usetup_mouse;
    memset(&usetup_mouse, 0, sizeof(usetup_mouse));
    snprintf(usetup_mouse.name, UINPUT_MAX_NAME_SIZE, "wdotool-mouse");
    usetup_mouse.id.bustype = BUS_I2C;
    usetup_mouse.id.vendor  = 0x04f4; // wacom
    usetup_mouse.id.product = 0x2842;
    usetup_mouse.id.version = 0x1;
    usetup_mouse.ff_effects_max = 0;
    check(ioctl(m_mouse, UI_DEV_SETUP, &usetup_mouse), "UI_DEV_SETUP");

    check(ioctl(m_mouse, UI_DEV_CREATE), "device creation");
    sleep(3);
#ifdef USE_DEEPIN_KF5_WAYLAND
    initCursor();
#endif
}

int Input::isDifferenceWithinLimit(QPoint &p1, QPoint &p2, int limit)
{
    int xDiff = std::abs(p1.x() - p2.x());
    int yDiff = std::abs(p1.y() - p2.y());
    if(xDiff > limit || yDiff > limit)
    {
        return 1;
    }
    return 0;
}

QVector<QPoint> Input::calculateIntermediatePoints(QPoint &start, QPoint &end, double step) {
    QVector<QPoint> points;

    // Calculate the difference between start and end points
    double dx = end.x() - start.x();
    double dy = end.y() - start.y();

    // Calculate the distance between start and end points
    double distance = std::sqrt(dx * dx + dy * dy);

    // Normalize the direction vector
    double norm_dx = dx / distance;
    double norm_dy = dy / distance;

    // Calculate the number of steps required
    int num_steps = static_cast<int>(distance / step);

    // Generate points along the line
    for (int i = 0; i <= num_steps; ++i) {
        double t = i * step;
        int x = static_cast<int>(std::round(start.x() + t * norm_dx));
        int y = static_cast<int>(std::round(start.y() + t * norm_dy));
        QPoint tmp(x, y);
        points.push_back(tmp);
    }

    return points;
}

int Input::calculateStep(int distance)
{
    if (distance >= 100) {
        return 10; // 距离较远时，步长最大为10
    } else if (distance >= 50) {
        return 5;  // 距离缩小时，步长逐渐减小
    } else if (distance >= 20) {
        return 3;
    } else if (distance >= 10) {
        return 2;
    } else {
        return 1;  // 当距离非常接近目标时，步长为1
    }
}

void Input::moveCursorToPosition(int x, int y)
{
    // 获取当前光标位置
    QPoint currentPos = getGlobalPosition();
    qDebug()<<"获取当前光标位置:" <<currentPos.x() << "," <<currentPos.y();
    qDebug()<<"目标光标位置:" << x << "," << y;

    // 计算X和Y方向的差值
    int dx = x - currentPos.x();
    int dy = y - currentPos.y();

    // 如果已经到达目标位置，则退出循环
    if (dx == 0 && dy == 0) {
        emit exitLoop();
        return;
    }

    // 计算每次移动的步长，避免移动太快错过目标点
    int stepX = (dx == 0) ? 0 : calculateStep(std::abs(dx)) * (dx > 0 ? 1 : -1);
    int stepY = (dy == 0) ? 0 : calculateStep(std::abs(dy)) * (dy > 0 ? 1 : -1);
    qDebug()<<"移动步长:" << stepX << "," << stepX;


    // 发送修正后的鼠标事件
    writeEvent(m_mouse,EV_REL,REL_X,stepX);
    writeEvent(m_mouse,EV_REL,REL_Y,stepY);
    writeEvent(m_mouse,EV_SYN,SYN_REPORT,0x0);

    // 稍微延迟一下，以避免发送太快
    usleep(5000); // 延迟 5ms
    emit moveEvent(x,y);
}

void Input::moveTo(int x, int y)
{
#ifndef USE_DEEPIN_KF5_WAYLAND
    moveCursorToPosition(x, y);
#else
    QPoint start = getGlobalPosition();
    QPoint end(x, y);

    if(isDifferenceWithinLimit(start, end, 4))
    {
        if(!single_flage)
        {
            single_finger_down();
        }

        QVector<QPoint> line = calculateIntermediatePoints(start, end, 1.0);
        int count = line.size();
        for(int i=0; i<count;++i)
        {
            qDebug()<<"X:"<<line[i].x()<<"Y:"<<line[i].y();
            writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_X,line[i].x());
            writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_Y,line[i].y());
            writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_X,line[i].x());
            writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_Y,line[i].y());
            writeEvent(m_screen,EV_ABS,ABS_X,line[i].x());
            writeEvent(m_screen,EV_ABS,ABS_Y,line[i].y());
            writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
            writeEvent(m_screen,EV_SYN,SYN_REPORT,0x0);
            usleep(5000);
        }

//        QPoint point = getAdded(start, end);
//        writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_X,line[count-1].x()+point.x());
//        writeEvent(m_screen,EV_ABS,ABS_MT_POSITION_Y,line[count-1].y()+point.y());
//        writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_X,line[count-1].x()+point.x());
//        writeEvent(m_screen,EV_ABS,ABS_MT_TOOL_Y,line[count-1].y()+point.y());
//        writeEvent(m_screen,EV_ABS,ABS_X,line[count-1].x()+point.x());
//        writeEvent(m_screen,EV_ABS,ABS_Y,line[count-1].y()+point.y());
//        writeEvent(m_screen,EV_MSC,MSC_TIMESTAMP,0x0);
//        writeEvent(m_screen,EV_SYN,SYN_REPORT,320x0);
//        usleep(5000);
        global_x=x;
        global_y=y;
        if(single_flage)
        {
            single_finger_up();
        }
    }
#endif
}

void Input::keyEvent(int code, int val)
{
#ifndef USE_DEEPIN_KF5_WAYLAND
    writeEvent(m_mouse, EV_KEY, code, val);
    writeEvent(m_mouse, EV_SYN, SYN_REPORT, 0x0);
#else
    if(code = BTN_LEFT && val == 0x1)
    {
        single_finger_down();
    }else if(code = BTN_LEFT && val == 0x0)
    {
        single_finger_up();
    }else{
        writeEvent(m_mouse, EV_KEY, code, val);
        writeEvent(m_mouse, EV_SYN, SYN_REPORT, 0x0);
    }
#endif
}

void Input::scroll(int val)
{
//    reduceToZero(val);
    if(val >= 0)
    {
        writeEvent(m_mouse, EV_REL, REL_WHEEL, val);
        writeEvent(m_mouse, EV_SYN,SYN_REPORT,0);
    }else
    {
        writeEvent(m_mouse, EV_REL, REL_WHEEL, val);
        writeEvent(m_mouse, EV_SYN, SYN_REPORT,0);
    }

}

QByteArray Input::createJsonString(int x, int y, QString text) {
    // 创建一个 JSON 对象
    QJsonObject jsonObj;
    jsonObj["x"] = x;
    jsonObj["y"] = y;
    jsonObj["text"] = text;

    // 将 JSON 对象转换为 JSON 文档
    QJsonDocument jsonDoc(jsonObj);

    // 将 JSON 文档转换为 JSON 字符串
    QString jsonString = jsonDoc.toJson(QJsonDocument::Compact);

    // 返回 JSON 字符串
    return jsonString.toUtf8();
}
