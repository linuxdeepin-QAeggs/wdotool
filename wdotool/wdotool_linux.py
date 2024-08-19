__author__ = "zhaoyouzhi@uniontech.com"
__contributor__ = "zhaoyouzhi@uniontech.com"

import os
import sys
import json
import socket
import time
from time import sleep
from funnylog import logger
from funnylog.conf import setting as log_setting


if sys.version_info[0] == 2 or sys.version_info[0:2] in ((3, 1), (3, 7)):
    # Python 2 and 3.1 and 3.2 uses collections.Sequence
    import collections

# In seconds. Any duration less than this is rounded to 0.0 to instantly move
# the mouse.
MINIMUM_DURATION = 0.1
# If sleep_amount is less than MINIMUM_DURATION,
# sleep() will be a no-op and the mouse
# cursor moves there instantly.
MINIMUM_SLEEP = 0.05
STEP_SLEEP = 10

# The number of seconds to pause after EVERY public function call. Useful for debugging:
PAUSE = 0.1  # Tenth-second pause by default.

FAILSAFE = True

Point = collections.namedtuple("Point", "x y")
Size = collections.namedtuple("Size", "width height")

bshift = False

LEFT = "left"
MIDDLE = "middle"
RIGHT = "right"
PRIMARY = "left"

mouse_button_dict = {"left": 0x110,
                     "middle": 0x112,
                     "right": 0x111}

def read_config(file_path):
    """
    读取json文件
    Args:
        file_path: 文件路径
    Returns: json对象
    """
    with open(file_path, 'r') as file:
        config = json.load(file)
    return config

user = os.getenv("USER")
#读取配置文件
wdotool_config = read_config(f"/etc/wdotoold/wdotoold.json")
log_setting.LOG_LEVEL = wdotool_config.get('log_level')
ip = wdotool_config.get('ip')
port = wdotool_config.get('port')


KEY_NAMES = {
    ' ': 57,
    '\'': 40,
    '*': 55,
    '+': 78,
    ',': 51,
    '-': 12,
    '.': 52,
    '/': 53,
    '0': 11,
    '1': 2,
    '2': 3,
    '4': 5,
    '5': 6,
    '3': 4,
    '6': 7,
    '7': 8,
    '8': 9,
    '9': 10,
    ';': 39,
    '=': 13,
    '[': 26,
    '\\': 43,
    ']': 27,
    '`': 41,
    'a': 30,
    'b': 48,
    'c': 46,
    'd': 32,
    'e': 18,
    'f': 33,
    'g': 34,
    'h': 35,
    'i': 23,
    'j': 36,
    'k': 37,
    'l': 38,
    'm': 50,
    'n': 49,
    'o': 24,
    'p': 25,
    'q': 16,
    'r': 19,
    's': 31,
    't': 20,
    'u': 22,
    'v': 47,
    'w': 17,
    'x': 45,
    'y': 21,
    'z': 44,
    'add': 78,
    'alt': 56,
    'altleft': 56,
    'altright': 100,
    'backspace': 14,
    'capslock': 58,
    'ctrl': 29,
    'ctrlleft': 29,
    'ctrlright': 97,
    'del': 111,
    'delete': 111,
    'down': 108,
    'end': 107,
    'enter': 28,
    'esc': 1,
    'escape': 1,
    'f1': 59,
    'f10': 68,
    'f11': 87,
    'f12': 88,
    'f13': 183,
    'f14': 184,
    'f15': 185,
    'f16': 186,
    'f17': 187,
    'f18': 188,
    'f19': 189,
    'f2': 60,
    'f20': 190,
    'f21': 191,
    'f22': 192,
    'f23': 193,
    'f24': 194,
    'f3': 61,
    'f4': 62,
    'f5': 63,
    'f6': 64,
    'f7': 65,
    'f8': 66,
    'f9': 67,
    'home': 172,
    'insert': 110,
    'left': 105,
    'num0': 82,
    'num1': 79,
    'num2': 80,
    'num3': 81,
    'num4': 75,
    'num5': 76,
    'num6': 77,
    'num7': 71,
    'num8': 72,
    'num9': 73,
    'numlock': 69,
    'pagedown': 109,
    'pageup': 104,
    'pgdn': 109,
    'pgup': 104,
    'print': 210,
    'right': 106,
    'scrolllock': 70,
    'printscreen': 210,
    'shift': 42,
    'shiftleft': 42,
    'shiftright': 54,
    'space': 57,
    'tab': 15,
    'up': 103,
    'volumedown': 114,
    'volumeup': 115,
    'win': 125,
    'winleft': 125,
    'winright': 126
}

KEY_NAMES_S = {
    ')': 11,
    '!': 2,
    '@': 3,
    '#': 4,
    '$': 5,
    '%': 6,
    '^': 7,
    '&': 8,
    '(': 10,
    '_': 12,
    '~': 41,
    '{': 26,
    '}': 27,
    '|': 43,
    ':': 39,
    '"': 40,
    '<': 51,
    '>': 52,
    '?': 53,
    'A': 30,
    'B': 48,
    'C': 46,
    'D': 32,
    'E': 18,
    'F': 33,
    'G': 34,
    'H': 35,
    'I': 23,
    'J': 36,
    'K': 37,
    'L': 38,
    'M': 50,
    'N': 49,
    'O': 24,
    'P': 25,
    'Q': 16,
    'R': 19,
    'S': 31,
    'T': 20,
    'U': 22,
    'V': 47,
    'W': 17,
    'X': 45,
    'Y': 21,
    'Z': 44
}

# 定义需要按Shift键的字符集合
shift_chars = set("!@#$%^&*()_+{}|:\"<>?ABCDEFGHIJKLMNOPQRSTUVWXYZ")

# 创建两个字典，一个包含普通按键，一个包含需要按Shift的按键
normal_keys = {}
shift_keys = {}

for key, value in KEY_NAMES.items():
    if key in shift_chars:
        shift_keys[key] = value
    else:
        normal_keys[key] = value


class WdotoolException(Exception):
    """
    Wdotool code will raise this exception class for any invalid actions. If Wdotool raises some other exception,
    you should assume that this is caused by a bug in Wdotool itself. (Including a failure to catch potential
    exceptions raised by Wdotool.)
    """
    pass

def send_event_and_wait_for_reply(event_dict, server_ip, server_port):
    """
    发送事件消息并等待回复
    Args:
        event_dict: 事件消息
        server_ip: 请求ip
        server_port: 请求端口
    Returns:
    """
    event_json = json.dumps(event_dict)

    # 创建一个 socket 对象
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

    try:
        # 连接服务器
        client_socket.connect((server_ip, server_port))

        # 发送消息
        logger.debug(f"send message:{event_dict}")
        client_socket.sendall(event_json.encode('utf-8'))

        # 接收服务器的回复
        reply = client_socket.recv(1024)  # 1024 是缓冲区大小，可以根据需要调整

        logger.debug(f"Received reply from server:{reply.decode('utf-8')}")
        result = json.loads(reply.decode('utf-8'))
        return result
    except socket.error as e:
        logger.error(f"Socket error:{e}")
    finally:
        # 关闭 socket 连接
        client_socket.close()

def send_event(event_dict, server_ip, server_port):
    """
    发送事件消息
    Args:
        event_dict: 事件消息
        server_ip: 请求ip
        server_port: 请求端口
    Returns:
    """
    # 将字典转换为JSON字符串
    event_json = json.dumps(event_dict)
    
    # 创建一个TCP/IP socket
    client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    try:
        # 连接到服务器
        client_socket.connect((server_ip, server_port))
        # 发送数据
        logger.debug(f"send message:{event_dict}")
        client_socket.sendall(event_json.encode('utf-8'))
    except socket.error as e:
        logger.error("Socket error:", e)
    finally:
        # 关闭 socket 连接
        client_socket.close()


def size():
    """
    获取屏幕尺寸
    Returns: Size
    """
    event_dict = {"eventType": 0x1f+2, "x": 0, "y": 0, "event": 0, "text": ""}
    result = send_event_and_wait_for_reply(event_dict, ip, port)
    logger.debug(f"获取屏幕尺寸：{result}")
    return Point(result["x"], result["y"])

def position(x=None, y=None):
    """
    以两个整数元组的形式返回鼠标光标的当前xy坐标
    Args:
      x (int, None, optional) - 如果不是None，此参数将覆盖中的x返回值

      y (int, None, optional) - 如果不是None，此参数将覆盖中的y返回值
    Returns:
      (x, y) 鼠标光标的当前xy坐标的（x，y）元组
    """
    event_dict = {"eventType": 0x1f + 1, "x": 0, "y": 0, "event": 0, "text": ""}
    result = send_event_and_wait_for_reply(event_dict, ip, port)
    logger.debug(f"获取光标位置：{result}")
    if x is not None:  # If set, the x parameter overrides the return value.
        result['x'] = int(x)
    if y is not None:  # If set, the y parameter overrides the return value.
        result['y'] = int(y)
    return Point(result['x'], result['y'])


def getPointOnLine(x1, y1, x2, y2, n):
    """
    返回点的（x，y）元组，该点沿二者定义的线按比例“n”前进``x1``，`y1``和`x2``，` y2``坐标
    """
    x = ((x2 - x1) * n) + x1
    y = ((y2 - y1) * n) + y1
    return (x, y)


def linear(n):
    """
    返回“n”，其中“n”是介于“0.0”和“1.0”之间的浮点参数。此功能用于默认用于鼠标移动功能的线性tween。
    """
    if not 0.0 <= n <= 1.0:
        raise WdotoolException("Argument must be between 0.0 and 1.0.")
    return n

def _normalizeXYArgs(firstArg, secondArg):
    """
    返回基于“firstArg”和“secondArg”的“Point”对象，这是传递给的前两个参数几个PyAutoGUI函数。如果“firstArg”和“secondArg”都是“None”，则返回当前鼠标光标位置

    ``firstArg“”和“secondArg”可以是整数、整数序列或表示图像文件名的字符串在屏幕上查找（并返回的中心坐标）。
    """
    if firstArg is None and secondArg is None:
        return position()
    else:
        return Point(firstArg, secondArg)


def moveTo(x=None, y=None, duration=0.0, tween=linear, _pause=True):
    logger.debug(f"移动至：{x} {y}")
    startx, starty = position()

    x = int(x) if x is not None else startx
    y = int(y) if y is not None else starty

    #width, height = size()

    # Make sure x and y are within the screen bounds.
    # x = max(0, min(x, width - 1))
    # y = max(0, min(y, height - 1))

    # If the duration is small enough, just move the cursor there instantly.
    # steps = [(x, y)]
    #
    # if duration > MINIMUM_DURATION:
    #     # Non-instant moving/dragging involves tweening:
    #     num_steps = max(width, height)
    #     sleep_amount = duration / num_steps
    #     if sleep_amount < MINIMUM_SLEEP:
    #         num_steps = int(duration / MINIMUM_SLEEP)
    #         sleep_amount = duration / num_steps
    #     steps = [getPointOnLine(startx, starty, x, y, tween(n / num_steps)) for n in range(num_steps)]
    #     # Making sure the last position is the actual destination.
    #     # print(steps)
    #     steps.append((x, y))
    # for tweenX, tweenY in steps:
    #     if len(steps) > 1:
    #         # A single step does not require tweening.
    #         sleep(sleep_amount + 0.01)
    #
    #     tweenX = int(round(tweenX))
    #     tweenY = int(round(tweenY))
    #     event_dict = {"eventType": 0x03, "x": tweenX, "y": tweenY, "event": 0, "text": ""}
    #     send_event(event_dict, ip, port)
    event_dict = {"eventType": 0x03, "x": x, "y": y, "event": 0, "text": ""}
    send_event_and_wait_for_reply(event_dict, ip, port)


def moveRel(
        xOffset=0, yOffset=0, duration=0.0, tween=linear, _pause=True,
):
    logger.debug(f"相对移动：{xOffset} {xOffset}")
    if xOffset is None:
        xOffset = 0
    if yOffset is None:
        yOffset = 0

    if type(xOffset) in (tuple, list):
        xOffset, yOffset = xOffset[0], xOffset[1]

    if xOffset == 0 and yOffset == 0:
        return  # no-op case

    mousex, mousey = position()
    mousex = mousex + xOffset
    mousey = mousey + yOffset
    moveTo(mousex, mousey, duration, tween)

move = moveRel

def mouseDown(x=None, y=None, button=PRIMARY, duration=0.0, tween=linear, _pause=True):
    x, y = _normalizeXYArgs(x, y)
    moveTo(x, y, duration, tween, _pause)
    event_dict = {"eventType": 0x01, "x": 1, "y": 0, "event": mouse_button_dict[button], "text": ""}
    send_event(event_dict, ip, port)
    sleep(0.3)


def mouseUp(x=None, y=None, button=PRIMARY, duration=0.0, tween=linear, _pause=True):
    x, y = _normalizeXYArgs(x, y)
    moveTo(x, y, duration, tween, _pause)
    event_dict = {"eventType": 0x01, "x": 0, "y": 0, "event": mouse_button_dict[button], "text": ""}
    send_event(event_dict, ip, port)


def click(
        x=None, y=None, clicks=1, interval=0.0, button=PRIMARY, duration=0.0, tween=linear, _pause=True
):
    x, y = _normalizeXYArgs(x, y)
    moveTo(x, y, duration, tween, _pause)
    for i in range(clicks):
        if button in (LEFT, MIDDLE, RIGHT):
            mouseDown(button=button)
            mouseUp(button=button)
        sleep(interval)


def leftClick(x=None, y=None, interval=0.0, duration=0.0, tween=linear, _pause=True):
    click(x, y, 1, interval, LEFT, duration, tween, _pause=_pause)


def rightClick(x=None, y=None, interval=0.0, duration=0.0, tween=linear, _pause=True):
    click(x, y, 1, interval, RIGHT, duration, tween, _pause=_pause)


def middleClick(x=None, y=None, interval=0.0, duration=0.0, tween=linear, _pause=True):
    click(x, y, 1, interval, MIDDLE, duration, tween, _pause=_pause)


def doubleClick(x=None, y=None, interval=0.0, button=LEFT, duration=0.0, tween=linear, _pause=True):
    click(x, y, 2, interval, button, duration, tween, _pause=False)


def tripleClick(x=None, y=None, interval=0.0, button=LEFT, duration=0.0, tween=linear, _pause=True):
    click(x, y, 3, interval, button, duration, tween, _pause=False)


def scroll(clicks, x=None, y=None, _pause=True):
    if type(x) in (tuple, list):
        x, y = x[0], x[1]
    x, y = position(x, y)
    moveTo(x, y)
    vscroll(clicks, x, y, _pause)


def hscroll(clicks, x=None, y=None, _pause=True):
    if type(x) in (tuple, list):
        x, y = x[0], x[1]
    x, y = position(x, y)
    moveTo(x, y)
    step = clicks / STEP_SLEEP
    for i in range(0, STEP_SLEEP):
        event_dict = {"eventType": 0x02, "x": step, "y": 0, "event": 0x06, "text": ""}
        send_event(event_dict, ip, port)
        sleep(0.05)


def vscroll(clicks, x=None, y=None, _pause=True):
    if type(x) in (tuple, list):
        x, y = x[0], x[1]
    x, y = position(x, y)
    moveTo(x, y)
    step = clicks / STEP_SLEEP
    for i in range(0, STEP_SLEEP):
        #event_dict = {"eventType": 0x02, "x": step, "y": 0, "event": 0x08}
        event_dict = {"eventType": 0x02, "x": step, "y": 0, "event": 0x08, "text": ""}
        send_event(event_dict, ip, port)
        sleep(0.05)

def dragTo(
        x=None, y=None, duration=0.0, tween=linear, button=PRIMARY, _pause=True, mouseDownUp=True
):
    x, y = _normalizeXYArgs(x, y)
    if mouseDownUp:
        mouseDown(button=button, _pause=False)
    moveTo(x, y, duration, tween)
    if mouseDownUp:
        mouseUp(button=button, _pause=False)


def dragRel(
        xOffset=0, yOffset=0, duration=0.0, tween=linear, button=PRIMARY, _pause=True,
        mouseDownUp=True
):
    if xOffset is None:
        xOffset = 0
    if yOffset is None:
        yOffset = 0

    if type(xOffset) in (tuple, list):
        xOffset, yOffset = xOffset[0], xOffset[1]

    if xOffset == 0 and yOffset == 0:
        return  # no-op case

    mousex, mousey = position()
    mousex = mousex + xOffset
    mousey = mousey + yOffset
    if mouseDownUp:
        mouseDown(button=button, _pause=False)
    moveTo(mousex, mousey, duration, tween)
    if mouseDownUp:
        mouseUp(button=button, _pause=False)


def keyDown(key, _pause=True):
    global bshift
    try:
        if len(key) > 1:
            key = key.lower()
        if key in KEY_NAMES_S.keys():
            event_dict = {"eventType": 0x01, "x": 1, "y": 0, "event": KEY_NAMES['shiftleft'], "text": ""}
            send_event(event_dict, ip, port)
            bshift = True
        event_dict = {"eventType": 0x01, "x": 1, "y": 0, "event": KEY_NAMES[key], "text": ""}
        send_event(event_dict, ip, port)
    except Exception:
        if bshift == True:
            event_dict = {"eventType": 0x01, "x": 0, "y": 0, "event": KEY_NAMES['shiftleft'], "text": ""}
            send_event(event_dict, ip, port)
            bshift = False
        pass


def keyUp(key, _pause=True):
    global bshift
    try:
        if len(key) > 1:
            key = key.lower()
        event_dict = {"eventType": 0x01, "x": 0, "y": 0, "event": KEY_NAMES[key], "text": ""}
        send_event(event_dict, ip, port)
        if key in KEY_NAMES_S.keys():
            event_dict = {"eventType": 0x01, "x": 0, "y": 0, "event": KEY_NAMES['shiftleft'], "text": ""}
            send_event(event_dict, ip, port)
            bshift = False
    except Exception:
        if bshift == True:
            event_dict = {"eventType": 0x01, "x": 0, "y": 0, "event": KEY_NAMES['shiftleft'], "text": ""}
            send_event(event_dict, ip, port)
            bshift = False
        pass


def press(keys, presses=1, interval=0.0, _pause=True):
    if type(keys) == str:
        if len(keys) > 1:
            keys = keys.lower()
        keys = [keys]  # If keys is 'enter', convert it to ['enter'].
    else:
        lowerKeys = []
        for s in keys:
            if len(s) > 1:
                lowerKeys.append(s.lower())
            else:
                lowerKeys.append(s)
        keys = lowerKeys
    interval = float(interval)
    for i in range(presses):
        for k in keys:
            keyDown(k)
            keyUp(k)
        sleep(interval)

def set_text(text):
    """
    设置文本至剪贴板
    Args:
        text: 文本
    Returns:
    """
    event_dict = {"eventType": 0x1f + 3, "x": 0, "y": 0, "event": 0, "text": f"{text}"}
    send_event(event_dict, ip, port)


def get_text():
    """
    获取剪贴板上文本内容
    Returns: 文本内容
    """
    event_dict = {"eventType": 0x1f + 4, "x": 0, "y": 0, "event": 0, "text": f""}
    result = send_event_and_wait_for_reply(event_dict, ip, port)
    return result["text"]


if __name__ == "__main__":
    # sleep(2)
    # click(1022, 417)
    sleep(3)
    # moveTo(800, 500)
    # press("a")
    # press("d")
    # press("e")
    # press("f")
    # scroll(-10)
    moveTo(1029, 288)
    # set_text("789")
    # print(get_text())
