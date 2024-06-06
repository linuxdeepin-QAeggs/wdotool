__author__ = "zhaoyouzhi@uniontech.com"
__contributor__ = "zhaoyouzhi@uniontech.com"

import os
import sys
import json
import socket
from funnylog import log
from funnylog import logger
from funnylog.conf import setting as log_setting


if sys.version_info[0] == 2 or sys.version_info[0:2] in ((3, 1), (3, 7)):
    # Python 2 and 3.1 and 3.2 uses collections.Sequence
    import collections

# In seconds. Any duration less than this is rounded to 0.0 to instantly move
# the mouse.
MINIMUM_DURATION = 0.1
# If sleep_amount is less than MINIMUM_DURATION, sleep() will be a no-op and the mouse cursor moves there instantly.
MINIMUM_SLEEP = 0.05
STEP_SLEEP = 10

# The number of seconds to pause after EVERY public function call. Useful for debugging:
PAUSE = 0.1  # Tenth-second pause by default.

FAILSAFE = True

Point = collections.namedtuple("Point", "x y")
Size = collections.namedtuple("Size", "width height")

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

#读取配置文件
wdotool_config = read_config(os.path.dirname(__file__)+"/wdotool.json")
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
    'winright': 126,
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
    'Z': 44,
    '。': 52
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
        return reply.decode('utf-8')
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
    logger.debug("获取屏幕尺寸")
    event_dict = {"eventType": 0x1f+2, "x": 0, "y": 0, "event": 0}
    return send_event_and_wait_for_reply(event_dict, ip, port)

if __name__ == "__main__":
    print(size())