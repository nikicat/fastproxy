#!/usr/bin/env python

import sys
import socket

def print_usage():
    print 'Usage: {0} <host> <expression>'.format(sys.argv[0])

def get_stat(sock, expression):
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.send(expression + '\n')
    return float(s.makefile().readline()[:-1])

def main(host, expression):
    return get_stat('/var/run/fastproxy/{0}.sock'.format(host.replace('tunnel', 'homer')), expression)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)
    host = sys.argv[1]
    expression = sys.argv[2]
    print(main(host, expression))
