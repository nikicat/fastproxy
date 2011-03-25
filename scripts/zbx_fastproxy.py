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

def get_stats(sock):
    if not os.path.exists(sock):
        return 0
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.send('show stat\n')
    sock_file = s.makefile()
    keys = sock_file.readline()[:-1].split('\t')
    values = [eval(v) for v in sock_file.readline()[:-1].split('\t')]
    return dict(zip(keys, values))

fastproxy_dir = '/var/run/fastproxy/'
def get_sockets():
    for ent in os.listdir(fastproxy_dir):
        if ent.endswith('.sock') and os.access(fastproxy_dir + ent, os.R_OK | os.W_OK) and stat.S_ISSOCK(os.stat(fastproxy_dir + ent).st_mode):
            yield fastproxy_dir + ent, ent[:-len('.sock')]

def main(id, expression):
    if id == '*':
        return sum([get_stat(sock, expression) for sock,id in get_sockets()])
    else:
        return get_stat(fastproxy_dir + id + '.sock', expression)

def vmain(combinations):
    all_stats = {}
    for sock,id in get_sockets():
        stats = get_stats(sock)
        all_stats[id] = stats

    sum_stats = all_stats['*'] = {}
    for stats in all_stats.values():
        for key, value in stats.items():
            if key in sum_stats:
                sum_stats[key] += value
            else:
                sum_stats[key] = value

    results = []
    for id,expression in combinations:
        results.append(eval(expression, {}, all_stats[id]))
    return results

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)
    host = sys.argv[1]
    expression = sys.argv[2]
    print(main(host, expression))
