#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <signal.h>
#include <linux/uinput.h>
#include <json-c/json.h>
#include <X11/Xlib.h>

static int fd = -1;
static int server_fd = -1;
static int new_socket = -1;

static int global_x = 0;
static int global_y = 0;

// 定义包含事件类型、坐标、偏移量和事件的结构体
typedef struct {
    int x;
    int y;
    int event;
    int eventType;
} InputEvent;

typedef struct {
    int screen_width;
    int screen_height;
    int resolution;
    int port;
} Info;

InputEvent processEvent(char *jsonStr) {
    struct json_object *parsed_json;
    struct json_object *eventType;
    struct json_object *x;
    struct json_object *y;
    struct json_object *event;

    // 解析JSON字符串
    parsed_json = json_tokener_parse(jsonStr);

    json_object_object_get_ex(parsed_json, "eventType", &eventType);
    json_object_object_get_ex(parsed_json, "x", &x);
    json_object_object_get_ex(parsed_json, "y", &y);
    json_object_object_get_ex(parsed_json, "event", &event);

    // 将JSON数据转换为InputEvent结构体
    InputEvent inputEvent;
    inputEvent.x = json_object_get_int(x);
    inputEvent.y = json_object_get_int(y);
    inputEvent.event = json_object_get_int(event);
    inputEvent.eventType = json_object_get_int(eventType);

    // 打印结构体内容作为示例
    printf("Event: %d X: %d Y: %d\n", inputEvent.event, inputEvent.x, inputEvent.y);

    // 释放JSON对象
    json_object_put(parsed_json);

    return inputEvent;
}

//读取外部配置json文件
Info read_config(const char *filename) {
    FILE *fp;
    char buffer[1024];
    struct json_object *parsed_json;
    struct json_object *screen_width;
    struct json_object *screen_height;
    struct json_object *resolution;
    struct json_object *port;

    // 打开配置文件
    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Unable to open configuration file\n");
        exit(EXIT_FAILURE);
    }

    // 读取文件内容到缓冲区
    fread(buffer, sizeof(char), 1024, fp);
    fclose(fp);

    // 解析JSON数据
    parsed_json = json_tokener_parse(buffer);
    if (parsed_json == NULL) {
        fprintf(stderr, "Error parsing JSON data\n");
        exit(EXIT_FAILURE);
    }

    json_object_object_get_ex(parsed_json, "screen_width", &screen_width);
    json_object_object_get_ex(parsed_json, "screen_height", &screen_height);
    json_object_object_get_ex(parsed_json, "resolution", &resolution);
    json_object_object_get_ex(parsed_json, "port", &port);

    Info info;
    info.screen_width = json_object_get_int(screen_width);
    info.screen_height = json_object_get_int(screen_height);
    info.resolution = json_object_get_int(resolution);
    info.port = json_object_get_int(port);

    // 打印结构体内容作为示例
    printf("screen_width: %d screen_height: %d resolution: %d\n port:%d\n", info.screen_width, info.screen_height, info.resolution, info.port);

    // 释放JSON对象
    json_object_put(parsed_json);

    return info;
}

int get_env_type()
{
    //判断桌面环境
    const char *session_type = getenv("XDG_SESSION_TYPE");

    if (session_type && strcmp(session_type, "x11") == 0) {
        return 1;
    } else if (session_type && strcmp(session_type, "wayland") == 0) {
        return 2;
    } 
    return -1;
}

void get_global_position()
{
    Display *display;
    Window root_window;
    Window returned_root_window, returned_child_window;
    int root_x, root_y;
    int win_x, win_y;
    unsigned int mask;

    // 打开与 X server 的连接
    display = XOpenDisplay(NULL);
    if (display == NULL) {
        fprintf(stderr, "Unable to open X display\n");
        exit(1);
    }

    // 获取默认屏幕的根窗口
    root_window = DefaultRootWindow(display);

        // 获取光标的位置
    if (XQueryPointer(display, root_window, &returned_root_window, &returned_child_window,
                      &root_x, &root_y, &win_x, &win_y, &mask)) {
        printf("(%d, %d)\n", root_x, root_y);
        global_x=root_x;
        global_y=root_y;
    } else {
        fprintf(stderr, "获取光标位置失败\n");
    }

    // 关闭与 X server 的连接
    XCloseDisplay(display);
}

//写入事件
void emit(int fd, int type, int code, int val)
{
    struct timeval time;
    struct input_event ie;
    ie.type = type;
    ie.code = code;
    ie.value = val;
    /* timestamp values below are ignored */
    ie.time.tv_sec = time.tv_sec;
    ie.time.tv_usec = time.tv_usec;
    write(fd, &ie, sizeof(ie));
}

//检查ioctl结果
void check(int ioctlresult, const char* errormsg)
{
    if (ioctlresult < 0)
    {
        printf("ioctl failed: %s\n", errormsg);
        exit(-1);
    }
}

//创建并设置设备
int setup_uinput_device(Info info) {
    struct uinput_setup usetup;
    fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);

    if (fd < 0) {
        perror("Unable to open /dev/uinput");
        return fd;
    }

    //激活同步事件
    check(ioctl(fd, UI_SET_EVBIT, EV_SYN), "UI_SET_EVBIT EV_SYN");
    //设置支持鼠标左右中键
    check(ioctl(fd, UI_SET_EVBIT, EV_KEY), "UI_SET_EVBIT, EV_KEY");
    static const int key_list[] = {KEY_ESC, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9, KEY_0, KEY_MINUS, KEY_EQUAL, KEY_BACKSPACE, KEY_TAB, KEY_Q, KEY_W, KEY_E, KEY_R, KEY_T, KEY_Y, KEY_U, KEY_I, KEY_O, KEY_P, KEY_LEFTBRACE, KEY_RIGHTBRACE, KEY_ENTER, KEY_LEFTCTRL, KEY_A, KEY_S, KEY_D, KEY_F, KEY_G, KEY_H, KEY_J, KEY_K, KEY_L, KEY_SEMICOLON, KEY_APOSTROPHE, KEY_GRAVE, KEY_LEFTSHIFT, KEY_BACKSLASH, KEY_Z, KEY_X, KEY_C, KEY_V, KEY_B, KEY_N, KEY_M, KEY_COMMA, KEY_DOT, KEY_SLASH, KEY_RIGHTSHIFT, KEY_KPASTERISK, KEY_LEFTALT, KEY_SPACE, KEY_CAPSLOCK, KEY_F1, KEY_F2, KEY_F3, KEY_F4, KEY_F5, KEY_F6, KEY_F7, KEY_F8, KEY_F9, KEY_F10, KEY_NUMLOCK, KEY_SCROLLLOCK, KEY_KP7, KEY_KP8, KEY_KP9, KEY_KPMINUS, KEY_KP4, KEY_KP5, KEY_KP6, KEY_KPPLUS, KEY_KP1, KEY_KP2, KEY_KP3, KEY_KP0, KEY_KPDOT, KEY_ZENKAKUHANKAKU, KEY_102ND, KEY_F11, KEY_F12, KEY_RO, KEY_KATAKANA, KEY_HIRAGANA, KEY_HENKAN, KEY_KATAKANAHIRAGANA, KEY_MUHENKAN, KEY_KPJPCOMMA, KEY_KPENTER, KEY_RIGHTCTRL, KEY_KPSLASH, KEY_SYSRQ, KEY_RIGHTALT, KEY_LINEFEED, KEY_HOME, KEY_UP, KEY_PAGEUP, KEY_LEFT, KEY_RIGHT, KEY_END, KEY_DOWN, KEY_PAGEDOWN, KEY_INSERT, KEY_DELETE, KEY_MACRO, KEY_MUTE, KEY_VOLUMEDOWN, KEY_VOLUMEUP, KEY_POWER, KEY_KPEQUAL, KEY_KPPLUSMINUS, KEY_PAUSE, KEY_SCALE, KEY_KPCOMMA, KEY_HANGEUL, KEY_HANGUEL, KEY_HANJA, KEY_YEN, KEY_LEFTMETA, KEY_RIGHTMETA, KEY_COMPOSE, KEY_STOP, KEY_AGAIN, KEY_PROPS, KEY_UNDO, KEY_FRONT, KEY_COPY, KEY_OPEN, KEY_PASTE, KEY_FIND, KEY_CUT, KEY_HELP, KEY_MENU, KEY_CALC, KEY_SETUP, KEY_SLEEP, KEY_WAKEUP, KEY_FILE, KEY_SENDFILE, KEY_DELETEFILE, KEY_XFER, KEY_PROG1, KEY_PROG2, KEY_WWW, KEY_MSDOS, KEY_COFFEE, KEY_SCREENLOCK, KEY_ROTATE_DISPLAY, KEY_DIRECTION, KEY_CYCLEWINDOWS, KEY_MAIL, KEY_BOOKMARKS, KEY_COMPUTER, KEY_BACK, KEY_FORWARD, KEY_CLOSECD, KEY_EJECTCD, KEY_EJECTCLOSECD, KEY_NEXTSONG, KEY_PLAYPAUSE, KEY_PREVIOUSSONG, KEY_STOPCD, KEY_RECORD, KEY_REWIND, KEY_PHONE, KEY_ISO, KEY_CONFIG, KEY_HOMEPAGE, KEY_REFRESH, KEY_EXIT, KEY_MOVE, KEY_EDIT, KEY_SCROLLUP, KEY_SCROLLDOWN, KEY_KPLEFTPAREN, KEY_KPRIGHTPAREN, KEY_NEW, KEY_REDO, KEY_F13, KEY_F14, KEY_F15, KEY_F16, KEY_F17, KEY_F18, KEY_F19, KEY_F20, KEY_F21, KEY_F22, KEY_F23, KEY_F24, KEY_PLAYCD, KEY_PAUSECD, KEY_PROG3, KEY_PROG4, KEY_DASHBOARD, KEY_SUSPEND, KEY_CLOSE, KEY_PLAY, KEY_FASTFORWARD, KEY_BASSBOOST, KEY_PRINT, KEY_HP, KEY_CAMERA, KEY_SOUND, KEY_QUESTION, KEY_EMAIL, KEY_CHAT, KEY_SEARCH, KEY_CONNECT, KEY_FINANCE, KEY_SPORT, KEY_SHOP, KEY_ALTERASE, KEY_CANCEL, KEY_BRIGHTNESSDOWN, KEY_BRIGHTNESSUP, KEY_MEDIA, KEY_SWITCHVIDEOMODE, KEY_KBDILLUMTOGGLE, KEY_KBDILLUMDOWN, KEY_KBDILLUMUP, KEY_SEND, KEY_REPLY, KEY_FORWARDMAIL, KEY_SAVE, KEY_DOCUMENTS, KEY_BATTERY, KEY_BLUETOOTH, KEY_WLAN, KEY_UWB, KEY_UNKNOWN, KEY_VIDEO_NEXT, KEY_VIDEO_PREV, KEY_BRIGHTNESS_CYCLE, KEY_BRIGHTNESS_AUTO, KEY_BRIGHTNESS_ZERO, KEY_DISPLAY_OFF, KEY_WWAN, KEY_WIMAX, KEY_RFKILL, KEY_MICMUTE, KEY_OK, KEY_SELECT, KEY_GOTO, KEY_CLEAR, KEY_POWER2, KEY_OPTION, KEY_INFO, KEY_TIME, KEY_VENDOR, KEY_ARCHIVE, KEY_PROGRAM, KEY_CHANNEL, KEY_FAVORITES, KEY_EPG, KEY_PVR, KEY_MHP, KEY_LANGUAGE, KEY_TITLE, KEY_SUBTITLE, KEY_ANGLE, KEY_ZOOM, KEY_MODE, KEY_KEYBOARD, KEY_SCREEN, KEY_PC, KEY_TV, KEY_TV2, KEY_VCR, KEY_VCR2, KEY_SAT, KEY_SAT2, KEY_CD, KEY_TAPE, KEY_RADIO, KEY_TUNER, KEY_PLAYER, KEY_TEXT, KEY_DVD, KEY_AUX, KEY_MP3, KEY_AUDIO, KEY_VIDEO, KEY_DIRECTORY, KEY_LIST, KEY_MEMO, KEY_CALENDAR, KEY_RED, KEY_GREEN, KEY_YELLOW, KEY_BLUE, KEY_CHANNELUP, KEY_CHANNELDOWN, KEY_FIRST, KEY_LAST, KEY_AB, KEY_NEXT, KEY_RESTART, KEY_SLOW, KEY_SHUFFLE, KEY_BREAK, KEY_PREVIOUS, KEY_DIGITS, KEY_TEEN, KEY_TWEN, KEY_VIDEOPHONE, KEY_GAMES, KEY_ZOOMIN, KEY_ZOOMOUT, KEY_ZOOMRESET, KEY_WORDPROCESSOR, KEY_EDITOR, KEY_SPREADSHEET, KEY_GRAPHICSEDITOR, KEY_PRESENTATION, KEY_DATABASE, KEY_NEWS, KEY_VOICEMAIL, KEY_ADDRESSBOOK, KEY_MESSENGER, KEY_DISPLAYTOGGLE, KEY_BRIGHTNESS_TOGGLE, KEY_SPELLCHECK, KEY_LOGOFF, KEY_DOLLAR, KEY_EURO, KEY_FRAMEBACK, KEY_FRAMEFORWARD, KEY_CONTEXT_MENU, KEY_MEDIA_REPEAT, KEY_10CHANNELSUP, KEY_10CHANNELSDOWN, KEY_IMAGES, KEY_DEL_EOL, KEY_DEL_EOS, KEY_INS_LINE, KEY_DEL_LINE, KEY_FN, KEY_FN_ESC, KEY_FN_F1, KEY_FN_F2, KEY_FN_F3, KEY_FN_F4, KEY_FN_F5, KEY_FN_F6, KEY_FN_F7, KEY_FN_F8, KEY_FN_F9, KEY_FN_F10, KEY_FN_F11, KEY_FN_F12, KEY_FN_1, KEY_FN_2, KEY_FN_D, KEY_FN_E, KEY_FN_F, KEY_FN_S, KEY_FN_B, KEY_BRL_DOT1, KEY_BRL_DOT2, KEY_BRL_DOT3, KEY_BRL_DOT4, KEY_BRL_DOT5, KEY_BRL_DOT6, KEY_BRL_DOT7, KEY_BRL_DOT8, KEY_BRL_DOT9, KEY_BRL_DOT10, KEY_NUMERIC_0, KEY_NUMERIC_1, KEY_NUMERIC_2, KEY_NUMERIC_3, KEY_NUMERIC_4, KEY_NUMERIC_5, KEY_NUMERIC_6, KEY_NUMERIC_7, KEY_NUMERIC_8, KEY_NUMERIC_9, KEY_NUMERIC_STAR, KEY_NUMERIC_POUND, KEY_NUMERIC_A, KEY_NUMERIC_B, KEY_NUMERIC_C, KEY_NUMERIC_D, KEY_CAMERA_FOCUS, KEY_WPS_BUTTON, KEY_TOUCHPAD_TOGGLE, KEY_TOUCHPAD_ON, KEY_TOUCHPAD_OFF, KEY_CAMERA_ZOOMIN, KEY_CAMERA_ZOOMOUT, KEY_CAMERA_UP, KEY_CAMERA_DOWN, KEY_CAMERA_LEFT, KEY_CAMERA_RIGHT, KEY_ATTENDANT_ON, KEY_ATTENDANT_OFF, KEY_ATTENDANT_TOGGLE, KEY_LIGHTS_TOGGLE, KEY_ALS_TOGGLE, KEY_BUTTONCONFIG, KEY_TASKMANAGER, KEY_JOURNAL, KEY_CONTROLPANEL, KEY_APPSELECT, KEY_SCREENSAVER, KEY_VOICECOMMAND, KEY_BRIGHTNESS_MIN, KEY_BRIGHTNESS_MAX, KEY_KBDINPUTASSIST_PREV, KEY_KBDINPUTASSIST_NEXT, KEY_KBDINPUTASSIST_PREVGROUP, KEY_KBDINPUTASSIST_NEXTGROUP, KEY_KBDINPUTASSIST_ACCEPT, KEY_KBDINPUTASSIST_CANCEL, KEY_RIGHT_UP, KEY_RIGHT_DOWN, KEY_LEFT_UP, KEY_LEFT_DOWN, KEY_ROOT_MENU, KEY_MEDIA_TOP_MENU, KEY_NUMERIC_11, KEY_NUMERIC_12, KEY_AUDIO_DESC, KEY_3D_MODE, KEY_NEXT_FAVORITE, KEY_STOP_RECORD, KEY_PAUSE_RECORD, KEY_VOD, KEY_UNMUTE, KEY_FASTREVERSE, KEY_SLOWREVERSE, KEY_DATA, KEY_MIN_INTERESTING, BTN_MISC, BTN_0, BTN_1, BTN_2, BTN_3, BTN_4, BTN_5, BTN_6, BTN_7, BTN_8, BTN_9, BTN_MOUSE, BTN_LEFT, BTN_RIGHT, BTN_MIDDLE, BTN_SIDE, BTN_EXTRA, BTN_FORWARD, BTN_BACK, BTN_TASK, BTN_JOYSTICK, BTN_TRIGGER, BTN_THUMB, BTN_THUMB2, BTN_TOP, BTN_TOP2, BTN_PINKIE, BTN_BASE, BTN_BASE2, BTN_BASE3, BTN_BASE4, BTN_BASE5, BTN_BASE6, BTN_DEAD, BTN_GAMEPAD, BTN_SOUTH, BTN_A, BTN_EAST, BTN_B, BTN_C, BTN_NORTH, BTN_X, BTN_WEST, BTN_Y, BTN_Z, BTN_TL, BTN_TR, BTN_TL2, BTN_TR2, BTN_SELECT, BTN_START, BTN_MODE, BTN_THUMBL, BTN_THUMBR, BTN_DIGI, BTN_TOOL_PEN, BTN_TOOL_RUBBER, BTN_TOOL_BRUSH, BTN_TOOL_PENCIL, BTN_TOOL_AIRBRUSH, BTN_TOOL_FINGER, BTN_TOOL_MOUSE, BTN_TOOL_LENS, BTN_TOOL_QUINTTAP, BTN_TOUCH, BTN_STYLUS, BTN_STYLUS2, BTN_TOOL_DOUBLETAP, BTN_TOOL_TRIPLETAP, BTN_TOOL_QUADTAP, BTN_WHEEL, BTN_GEAR_DOWN, BTN_GEAR_UP, BTN_DPAD_UP, BTN_DPAD_DOWN, BTN_DPAD_LEFT, BTN_DPAD_RIGHT, BTN_TRIGGER_HAPPY, BTN_TRIGGER_HAPPY1, BTN_TRIGGER_HAPPY2, BTN_TRIGGER_HAPPY3, BTN_TRIGGER_HAPPY4, BTN_TRIGGER_HAPPY5, BTN_TRIGGER_HAPPY6, BTN_TRIGGER_HAPPY7, BTN_TRIGGER_HAPPY8, BTN_TRIGGER_HAPPY9, BTN_TRIGGER_HAPPY10, BTN_TRIGGER_HAPPY11, BTN_TRIGGER_HAPPY12, BTN_TRIGGER_HAPPY13, BTN_TRIGGER_HAPPY14, BTN_TRIGGER_HAPPY15, BTN_TRIGGER_HAPPY16, BTN_TRIGGER_HAPPY17, BTN_TRIGGER_HAPPY18, BTN_TRIGGER_HAPPY19, BTN_TRIGGER_HAPPY20, BTN_TRIGGER_HAPPY21, BTN_TRIGGER_HAPPY22, BTN_TRIGGER_HAPPY23, BTN_TRIGGER_HAPPY24, BTN_TRIGGER_HAPPY25, BTN_TRIGGER_HAPPY26, BTN_TRIGGER_HAPPY27, BTN_TRIGGER_HAPPY28, BTN_TRIGGER_HAPPY29, BTN_TRIGGER_HAPPY30, BTN_TRIGGER_HAPPY31, BTN_TRIGGER_HAPPY32, BTN_TRIGGER_HAPPY33, BTN_TRIGGER_HAPPY34, BTN_TRIGGER_HAPPY35, BTN_TRIGGER_HAPPY36, BTN_TRIGGER_HAPPY37, BTN_TRIGGER_HAPPY38, BTN_TRIGGER_HAPPY39, BTN_TRIGGER_HAPPY40};
    for (int i=0; i<sizeof(key_list)/sizeof(int); i++) {

        check(ioctl(fd, UI_SET_KEYBIT, key_list[i]), "UI_SET_KEYBIT LIST");
    }
    //设置支持滚轮事件
    check(ioctl(fd, UI_SET_EVBIT, EV_REL), "UI_SET_EVBIT, EV_REL");
    check(ioctl(fd, UI_SET_RELBIT, REL_WHEEL), "UI_SET_RELBIT, REL_WHEEL");
    //设置支持绝对坐标事件
    check(ioctl(fd, UI_SET_EVBIT, EV_ABS), "UI_SET_EVBIT EV_ABS");
    check(ioctl(fd, UI_SET_ABSBIT, ABS_X), "UI_SETEVBIT ABS_X");
    check(ioctl(fd, UI_SET_ABSBIT, ABS_Y), "UI_SETEVBIT ABS_Y");
    //设置虚拟设备版本信息
    struct uinput_setup uidev;
    memset(&uidev, 0, sizeof(uidev));
    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "wdotool");
    uidev.id.bustype = BUS_I2C;
    uidev.id.vendor  = 0x04f3; // wacom
    uidev.id.product = 0x2841;
    uidev.id.version = 0x1;
    uidev.ff_effects_max = 0;
    check(ioctl(fd, UI_DEV_SETUP, &uidev), "UI_DEV_SETUP");
    //设置绝对坐标事件依赖的屏幕分辨率信息
    struct uinput_abs_setup xabs;
    xabs.code = ABS_X;
    xabs.absinfo.minimum = 0;
    xabs.absinfo.maximum = info.screen_width;
    xabs.absinfo.fuzz = 0;
    xabs.absinfo.flat = 0;
    xabs.absinfo.resolution = info.resolution;
    xabs.absinfo.value = 0;
    struct uinput_abs_setup yabs;
    yabs.code = ABS_Y;
    yabs.absinfo.minimum = 0;
    yabs.absinfo.maximum = info.screen_height;
    yabs.absinfo.fuzz = 0;
    yabs.absinfo.flat = 0;
    yabs.absinfo.resolution = info.resolution;
    yabs.absinfo.value = 0;
    check(ioctl(fd, UI_ABS_SETUP, &xabs), "ABS_X setup");
    check(ioctl(fd, UI_ABS_SETUP, &yabs), "ABS_Y setup");

    //创建设备
    check(ioctl(fd, UI_DEV_CREATE), "device creation");
    sleep(5);
}

void move_to(int x, int y)
{
    emit(fd, EV_ABS, ABS_X, x);
    emit(fd, EV_ABS, ABS_Y, y);
    emit(fd,EV_SYN,SYN_REPORT,0x0);
    if (get_env_type() == 1)
    {
        get_global_position();
    }else
    {
        global_x = x;
        global_y = y;
    }
}

void key_event(int code, int val)
{
    emit(fd, EV_KEY, code, val);
    emit(fd,EV_SYN,SYN_REPORT,0x0);
}

void scroll(int val)
{
    if(val >= 0)
    {
        while (val--)
        {
            emit(fd, EV_REL, REL_WHEEL, val);
            emit(fd, EV_SYN,SYN_REPORT,0x0);
        }
        
    }else
    {
        while (val++ > 0)
        {
            emit(fd, EV_REL, REL_WHEEL, val);
            emit(fd, EV_SYN,SYN_REPORT,0x0);
        }
    }
}

// __signed__ int get_time_stamp()
// {
//     struct timeval time;
//     return time.tv_sec * 1000000 + time.tv_usec;
// }

//信号处理函数
void handle_signal(int sig) {
    if (sig == SIGINT || sig == SIGTERM) {
        close(fd);
        close(new_socket);
        close(server_fd);
    } 
    exit(0);
}


char* create_json_string(int x, int y) {
    // 创建一个 JSON 对象
    json_object *root = json_object_new_object();
    if (root == NULL) {
        printf("Failed to create JSON object\n");
        return NULL;
    }

    // 向 JSON 对象中添加 x 和 y 两个元素
    json_object_object_add(root, "x", json_object_new_int(x));
    json_object_object_add(root, "y", json_object_new_int(y));

    // 将 JSON 对象转换为 JSON 字符串
    const char *json_str = json_object_to_json_string(root);
    if (json_str == NULL) {
        printf("Failed to convert JSON object to string\n");
        json_object_put(root);
        return NULL;
    }

    // 复制 JSON 字符串到堆上分配的内存中
    char *json_str_copy = strdup(json_str);
    if (json_str_copy == NULL) {
        printf("Failed to copy JSON string\n");
        json_object_put(root);
        return NULL;
    }

    // 释放 JSON 对象的内存
    json_object_put(root);

    // 返回 JSON 字符串的副本
    return json_str_copy;
}

int main(int argc,char*argv[])
{
    //创建信号捕获主要用于回收资源
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);

    // ScreenInfo screenInfo;
    // screenInfo.screen_width=1920;
    // screenInfo.screen_height=1080;
    // screenInfo.resolution=10;
    
    // 获取环境变量HOME的值
    const char *home_dir=NULL;
    home_dir = getenv("HOME");
    if (home_dir == NULL) {
        fprintf(stderr, "The HOME environment variable is not set.\n");
        exit(EXIT_FAILURE);
    }
    char full_path[4096];
    if (snprintf(full_path, 4096, "%s%s", home_dir, "/.config/wdotoold.json") >= 4096) {
        fprintf(stderr, "The config file path is too long.\n");
        return 1;
    }

    Info info = read_config(full_path);
    setup_uinput_device(info);
    move_to(info.screen_width/(info.resolution/10)/2, info.screen_height/(info.resolution/10)/2);

    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char buffer[1024] = {0};

    // 创建套接字
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // 将套接字绑定到端口
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(info.port);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 监听连接
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    printf("Server listening on port 65432...\n");

    while (1) {
        // 接受客户端连接
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            close(server_fd);
            exit(EXIT_FAILURE);
        }
        // 读取客户端发送的数据
        int valread = read(new_socket, buffer, 1024);
        if (valread > 0) {
            buffer[valread] = '\0'; // 确保缓冲区以空字符结尾
            printf("Received: %s\n", buffer);
            // 处理接收到的JSON字符串
            InputEvent inputEvent = processEvent(buffer);
            switch (inputEvent.eventType) {
                case EV_KEY:
                    key_event(inputEvent.event, inputEvent.x);
                    break;
                case EV_REL:
                    if (inputEvent.event == BTN_WHEEL)
                    {
                        scroll(inputEvent.x);
                    }
                    break;
                case EV_ABS:
                    move_to(inputEvent.x, inputEvent.y);
                    break;
                case (EV_MAX+1):
                    {
                        // 向客户端发送应答
                        char* json_str_tmp = create_json_string(global_x, global_y);
                        send(new_socket, json_str_tmp, strlen(json_str_tmp), 0);
                        printf("%s message sent to client\n", json_str_tmp);
                        free(json_str_tmp);
                    }
                    break;
                case (EV_MAX+2):
                    {
                        // 向客户端发送应答
                        char* json_str_tmp = create_json_string(info.screen_width/(info.resolution/10), info.screen_height/(info.resolution/10));
                        send(new_socket, json_str_tmp, strlen(json_str_tmp), 0);
                        printf("%s message sent to client\n", json_str_tmp);
                        free(json_str_tmp);
                    }
                    break;
                default:
                    printf("Unknown event\n");
                    break;
            }       
        }
        // 关闭连接
        close(new_socket);
    }

    return 0;
}

