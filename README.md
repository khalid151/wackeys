# WacKeys
**Wac**(om Express)**Keys** is a backend for key events using `libinput`.
It writes these events to a UNIX domain socket that can be read by an external process.

The aim of this project is to offer a way to change mapping of a key depending
on the currently active window and the active mode layer of tablet.

## Dependencies
- `libinput`
- `libudev`

## Build and Install
```
$ git clone https://github.com/khalid151/WacKeys
$ cd WacKeys
$ make
# make install
```
## Run
Since **WacKeys** uses `libinput` to read from raw input, it needs to be run as root.
Use `-d` option to run as a daemon.
```
$ sudo wackeys -d
```
Check the man page (`$ man wackeys`) and the example to see how to use the socket.
