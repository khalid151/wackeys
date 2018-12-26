#!/usr/bin/env python3
import re
import socket
import subprocess
import sys

digits_re = re.compile(r'\d+')
key_map = {0: {0: 'a',
               1: 'b',
               2: 'c',
               3: 'd',
               4: 'e',
               5: 'f',
               6: 'g',
               7: 'h'},
           1: {0: 'i',
               1: 'j',
               2: 'k',
               3: 'l',
               4: 'm',
               5: 'n',
               6: 'o',
               7: 'p'},
           2: {0: 'q',
               1: 'r',
               2: 's',
               3: 't',
               4: 'u',
               5: 'v',
               6: 'w',
               7: 'x'},
           3: {0: 'y',
               1: 'z',
               2: '1',
               3: '2',
               4: '3',
               5: '4',
               6: '5',
               7: '6'}}

def press(key):
    subprocess.run(['xdotool', 'keydown', key])

def release(key):
    subprocess.run(['xdotool', 'keyup', key])

def get_digits(text):
    d = digits_re.findall(text)
    return (int(d[0]), int(d[1]))

def main():
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect('/tmp/WacKeys.sock')

    while True:
        data = sock.recv(8).decode()
        digits = get_digits(data)

        if data.startswith('BP'):
            try:
                press(key_map[digits[1]][digits[0]])
            except KeyError:
                pass
        elif data.startswith('BR'):
            try:
                release(key_map[digits[1]][digits[0]])
            except KeyError:
                pass

if __name__ == '__main__':
    try:
        main()
    except KeyboardInterrupt:
        sys.exit(0)
