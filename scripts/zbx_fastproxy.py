#!/usr/bin/env python

import sys
import socket
import os.path
import stat
import time

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
    check_time = time.time()
    s = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    s.connect(sock)
    s.send('show stat\n')
    sock_file = s.makefile()
    keys = sock_file.readline()[:-1].split('\t')
    values = [eval(v) for v in sock_file.readline()[:-1].split('\t')]
    return dict(zip(keys, values) + [('time', check_time)])

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

prev_stats = None

def vmain(combinations):
    global prev_stats
    all_stats = {}
    check_time = time.time()
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

    for key, value in sum_stats.items():
        for stats in all_stats.values():
            if key not in stats:
                stats[key] = 0

    sum_stats['time'] = check_time

    if prev_stats is None:
        prev_stats = all_stats
    results = []
    class Tmp(object):
        pass
    for id,expression in combinations:
        try:
            prev = Tmp()
            for key, value in prev_stats[id].items():
                setattr(prev, key, value)
            try:
                result = eval(expression, {}, dict(all_stats[id].items() + [('prev', prev)]))
            except ZeroDivisionError:
                result = 0
            results.append(0)
        except:
            results.append(None)
    prev_stats = all_stats
    return results

if __name__ == '__main__':
    if len(sys.argv) < 2:
        print_usage()
        sys.exit(1)
    host = sys.argv[1]
    expression = sys.argv[2]
    print(vmain([(host, expression)])[0])
