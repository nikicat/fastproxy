#!/usr/bin/env python
### BEGIN INIT INFO
# Provides:          fastproxy
# Required-Start:    $local_fs $network $remote_fs
# Required-Stop:     $local_fs $remote_fs
# Default-Start:     2 3 4 5
# Default-Stop:      0 1 6
# Short-Description: fast http(s) proxy
# Description:       This file should be used to start and stop fastproxy.
### END INIT INFO

# Author: Nikolay Bryskin <devel.niks@gmail.com>

from subprocess import Popen
import os
import signal
import os.path
import time
import sys
import resource
import ConfigParser
import traceback
from itertools import chain

class daemon(object):
    def __init__(self, name, nameid):
        self.name = name
        self.nameid = nameid
        self.pid_file = '/var/run/{0}/{1}.pid'.format(self.name, self.nameid)
        self.args = [name]
        self.executable = '/usr/bin/{0}'.format(name)

    def _daemonize(self):
        if os.fork():   # launch child and...
            return True
        os.setsid()
        if os.fork():   # launch child and...
            os._exit(0) # kill off parent again.
        os.umask(022)   # Don't allow others to write
        null=os.open('/dev/null', os.O_RDWR)
        for i in range(3):
            try:
                os.dup2(null, i)
            except OSError, e:
                if e.errno != errno.EBADF:
                    raise
        os.close(null)
        return False

    def _daemon(self, cmd, args):
        sys.stderr.write('daemonizing with args {0}'.format(args))
        sys.stderr.flush()
        if self._daemonize():
            return
        resource.setrlimit(resource.RLIMIT_NOFILE, (65536, 65536))
        p = Popen(executable=cmd, args=args, env={'LD_LIBRARY_PATH': '/usr/local/lib:/usr/local/lib64'},
                  close_fds=True,
                  stderr=open('/var/log/{0}/{1}.err'.format(self.name, self.nameid), 'a+'),
                  stdout=open('/var/log/{0}/{1}.out'.format(self.name, self.nameid), 'a+'))
        if not os.path.isdir(os.path.dirname(self.pid_file)):
            os.mkdir(os.path.dirname(self.pid_file))
        with open(self.pid_file, 'w+') as f:
            f.write(str(p.pid))
        os._exit(0)

    def get_pid(self):
        pid = open(self.pid_file).read()
        if pid is None:
            raise Exception('corrupted pid file')
        if not os.path.isdir('/proc/{0}'.format(pid)):
            raise Exception('stale pid file')
        return int(pid)

    def clean_pid(self):
        try:
            os.unlink(self.pid_file)
        except:
            pass

    def stop(self):
        try:
            os.kill(self.get_pid(), signal.SIGTERM)
            self.stop = self.check_stopped
            return self.stop()
        except:
            self.clean_pid()
            raise

    def check_stopped(self):
        try:
            self.get_pid()
        except Exception, e:
            if e.args[0] == 'stale pid file':
                self.clean_pid()
                return True
        return False

    def start(self):
        try:
            pid = self.get_pid()
        except Exception:
            sys.stderr.write('{0} starting..\n'.format(self.nameid))
            sys.stderr.flush()
            self.clean_pid()
            self._daemon(self.executable, self.args)
            self.start = self.check_started
        else:
            raise Exception('already running with pid {0}'.format(pid))

    def check_started(self):
        try:
            pid = self.get_pid()
            sys.stderr.write('{0} started with pid {1}\n'.format(self.nameid, pid))
            sys.stderr.flush()
            return True
        except:
            return False

    def restart(self):
        if self.stop():
            return self.start()
        else:
            return False

    def reload(self):
        os.kill(self.get_pid(), signal.SIGHUP)
        sys.stderr.write('{0} HUPed\n'.format(self.nameid))
        sys.stderr.flush()
        return True

    def status(self):
        sys.stderr.write('{0} running [{1}]\n'.format(self.nameid, self.get_pid()))
        sys.stderr.flush()
        return True

class fastproxy(daemon):
    def __init__(self, nameid, source_ip, options):
        daemon.__init__(self, self.__class__.__name__, nameid)
        for name, val in options.items():
            if name == 'listen-port':
                listen_port = val
            else:
                self.args += ['--{0}={1}'.format(name, value) for value in val.split(',')]

        self.args += [
            '--ingoing-http={0}:{1}'.format(source_ip, listen_port),
            '--ingoing-stat=/var/run/{0}/{1}.sock'.format(self.name, self.nameid),
            '--outgoing-http={0}'.format(source_ip),
        ]

def numbers_to_ids(homers):
    for i in homers:
        yield ('homer{0:03}'.format(int(i)), '192.168.6.{0}'.format(int(i) * 2 + 1))

def main():
    commands = ['start', 'stop', 'restart', 'reload', 'status']
    command = ''
    name = 'fastproxy'
    ids = []
    try:
        command = sys.argv[1]
        if command not in commands:
            raise BaseException('invalid command')
    except:
        print('Usage: {0} [{1} [id(,id)]]'.format(sys.argv[0], '|'.join(commands)))
        return 1

    config = ConfigParser.ConfigParser()
    config.read('/etc/{0}.conf'.format(name))
    options = dict(config.items('DEFAULT'))
    if 'ldap' in options:
        if len(sys.argv) > 2:
            ids = get_ldap_ids(options['ldap'], sys.argv[2])
        else:
            ids = chain(get_ldap_ids(options['ldap'], 'homer*'), get_ldap_ids(options['ldap'], 'tunnel*'))
    elif 'instance-id-list' in options:
        ids = numbers_to_ids(map(int, options['instance-id-list'].split(',')))
    else:
        ids = numbers_to_ids(range(128))

    if 'instance-id-list' in options:
        del options['instance-id-list']
    if 'ldap' in options:
        del options['ldap']

    workers = []
    for id, ip in ids:
        workers.append(fastproxy(id, ip, options))

    sys.stdout.write('{0}ing.'.format(command))
    sys.stdout.flush()

    result = 0

    while workers:
        for w in workers[:]:
            try:
                if getattr(w, command)():
                    workers.remove(w)
            except Exception:
                result = 1
                sys.stderr.write('{0}:\n'.format(w.nameid))
                traceback.print_exc(file=sys.stderr)
                sys.stderr.flush()
                workers.remove(w)
        time.sleep(0.5)
        sys.stdout.write('.')
        sys.stdout.flush()
    sys.stdout.write('\n')

    return result

def get_ldap_ids(host, mask):
    import ldap
    l = ldap.initialize(host)
    for dn, entry in l.search_s('dc=local,dc=net', ldap.SCOPE_SUBTREE, 'cn={0}'.format(mask)):
        ip = entry['ipHostNumber'][0].split('.')
        ip[3] = str(int(ip[3]) + 1)
        yield (entry['cn'][0], '.'.join(ip))

if __name__ == '__main__':
    sys.exit(main())
