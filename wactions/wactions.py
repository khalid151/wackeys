#!/usr/bin/env python3
import argparse
import configparser
import math
import re
import socket
import sys
from enum import Enum
from os import environ as env
from os.path import exists
from subprocess import check_output as output
from subprocess import run
from subprocess import CalledProcessError


class Helper:
    def __init__(self):
        self.digits_re = re.compile(r'\d+')
        self.class_re = re.compile(r'"(.*?)"')

    def get_window_class(self):
        try:
            window_id = output(['xdotool', 'getactivewindow']).decode()
        except CalledProcessError:
            # Didn't find any window - return
            return
        xprop = output(['xprop', 'WM_CLASS', '-id', window_id]).decode()
        return self.class_re.search(xprop).group(1)

    def get_digits(self, msg):
        digits = self.digits_re.findall(msg)
        # Return: button, layer
        return int(digits[0]), int(digits[1])

    def rotating_cw(self, start_angle, end_angle):
        # Cross product of two vectors
        start_point = self.vector(start_angle)
        end_point = self.vector(end_angle)
        direction = (end_point[0] * start_point[1]) - (end_point[1] * start_point[0])
        if direction > 0:
            return True
        return False

    def vector(self, theta):
        x = math.sin(math.radians(theta))
        y = math.cos(math.radians(theta))
        return x, y

class Actions(Enum):
    Press = 'keydown'
    Release = 'keyup'
    Key = 'key'

class Keys:
    def __init__(self, socket_path, config):
        self.socket = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.helper = Helper()
        self.config = config
        try:
            self.socket.connect(socket_path)
        except FileNotFoundError:
            sys.stderr.write(f"Could not connect to {socket_path}. Are you sure wackeys is running?\n")
            sys.exit(1)

    def action(self, action, cmd):
        # TODO: find a way to get keycode
        try:
            cmd, r = cmd.split('run:')[-1], True if 'run:' in cmd else False
        except AttributeError:
            return
        if r:
            run(cmd.split(' '))
        else:
            run(['xdotool', action.value, cmd])

    def get_button_mapping(self, button, layer, win_class, ring=False, cw=False):
        layer = f"layer_{layer+1}"
        button = f"ring_{'cw' if cw else 'ccw'}" if ring else f"button_{button+1}"
        keymap = None

        # Check keys by order: [class/layer] -> [class] -> [layer] -> default
        for section in (f"{win_class}/{layer}", win_class, layer, "default"):
            keys = dict(self.config).get(section)
            if keys is not None:
                keymap = keys.get(button)
            if keymap is not None:
                break

        if keymap is not None:
            if "run:" not in keymap:
                keymap = keymap.replace(' ', '')

        return keymap

    def get_ring_mapping(self, cw, layer, win_class):
        return self.get_button_mapping(0, layer, win_class, True, cw)

    def run(self):
        rotating = False
        start_angle = 0

        while True:
            data = self.socket.recv(8).decode()
            digits, layer = self.helper.get_digits(data)
            window_class = self.helper.get_window_class()

            # Handle button presses
            if data.startswith("BP"):
                self.action(Actions.Press, self.get_button_mapping(digits, layer, window_class))
            elif data.startswith("BR"):
                self.action(Actions.Release, self.get_button_mapping(digits, layer, window_class))

            # Handle ring rotation
            if data.startswith("RR"):
                if not rotating:
                    rotating = True
                    start_angle = digits
                # Check if it rotated 10 degress
                if abs(digits - start_angle) >= 10:
                    # TODO: add key down + ring rotation action
                    rotating = False
                    cw = self.helper.rotating_cw(start_angle, digits)
                    self.action(Actions.Key, self.get_ring_mapping(cw, layer, window_class))

            elif data.startswith("RD"):
                rotating = False

def main():
    # Check for xdotool and xprop
    both = 0
    for path in env['PATH'].split(':'):
        if exists(f'{path}/xdotool'):
            both += 1
        if exists(f'{path}/xprop'):
            both += 1
        if both == 2:
            break
    else:
        sys.stderr.write("Make sure both of xdotool and xprop are installed.\n")
        sys.exit(1)

    parser = argparse.ArgumentParser()
    parser.add_argument('-s', '--socket', default="/tmp/WacKeys.sock", help="path to socket")
    parser.add_argument('-c', '--config', default=f"{env['HOME']}/.config/wactions/config", help="path to config")
    args = parser.parse_args()

    if not exists(args.config):
        sys.stderr.write("Could not find configuration file.\n")
        sys.exit(1)

    config = configparser.ConfigParser()
    config.read(args.config)
    keymap = Keys(args.socket, config)

    try:
        keymap.run()
    except KeyboardInterrupt:
        keymap.socket.close()
        sys.exit(0)

if __name__ == '__main__':
    main()
