#!/usr/bin/env python

import sys
import socket
import os.path
import stat

def print_usage():
    print 'Usage: {0} <host> <expression>'.format(sys.argv[0])

def get_stat(sock, expression):
    if not os.path.exists(sock):
        return 0
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.send(expression + '\n')
    resp = s.makefile().readline()[:-1]
    if resp == expression + '?':
        return 0
    return eval(resp)

fastproxy_dir = '/var/run/fastproxy/'
def get_sockets():
    for ent in os.listdir(fastproxy_dir):
        if ent.endswith('.sock') and os.access(fastproxy_dir + ent, os.R_OK | os.W_OK) and stat.S_ISSOCK(os.stat(fastproxy_dir + ent).st_mode):
            yield fastproxy_dir + ent

def main(host, expression):
    if host == '*':
        return sum([get_stat(sock, expression) for sock in get_sockets()])
    else:
        return get_stat(fastproxy_dir + host + '.sock', expression)

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)
    host = sys.argv[1]
    expression = sys.argv[2]
    print(main(host, expression))
