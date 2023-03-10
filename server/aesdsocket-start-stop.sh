#!/bin/sh

case "$1" in
    start)
        echo "Starting aesdsocket"
        # start-stop-daemon -S -n aesdsocket -a /usr/bin/aesdsocket -- -d
        start-stop-daemon --start --exec /usr/bin/aesdsocket -- -d
        ;;
    stop)
        echo "Stopping aesdsocket"
        # start-stop-daemon -K -n aesdsocket
        start-stop-daemon --stop --name aesdsocket
        ;;
    *)
        echo "Usage: $0 {start|stop}"
        exit 1
esac

exit 0
