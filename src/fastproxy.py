#!/usr/bin/env python

from subprocess import Popen
import os
import signal
import os.path
import time
import sys
import resource
import ConfigParser

class daemon(object):
    def __init__(self, name, id):
        self.name = name
        self.id = id
        self.nameid = '{0}{1:03}'.format(self.name, self.id)
        self.pid_file = '/var/run/{0}/{1}.pid'.format(self.name, self.nameid)
        self.args = [name]
        self.executable = '/usr/local/bin/{0}'.format(name)

    def _daemonize(self):
        # Set maximum CPU time to 1 second in child process, after fork() but before exec()
        #daemonize.createDaemon()
        os.setsid()
        resource.setrlimit(resource.RLIMIT_NOFILE, (65536, 65536))
        try:
            pid = os.fork()
        except OSError, e:
            raise RuntimeError("2nd fork failed: %s [%d]" % (e.strerror, e.errno))
        if pid != 0:
            with open(self.pid_file, 'w+') as f:
                f.write(str(pid))
                f.close()
            # child process is all done
            os._exit(0)
    
    def _daemon(self, cmd, args):
        p = Popen(executable=cmd, args=args, env={'LD_LIBRARY_PATH': '/usr/local/lib:/usr/local/lib64'},
                  preexec_fn=self._daemonize, close_fds=True,
                  stderr=open('/var/log/{0}/{1}.err'.format(self.name, self.nameid), 'a+'),
                  stdout=open('/var/log/{0}/{1}.out'.format(self.name, self.nameid), 'a+'))

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
        except BaseException:
            self.clean_pid()
            self._daemon(self.executable, self.args)
            self.get_pid()
            return True
        raise Exception('already running with pid {0}'.format(pid))
    
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
    def __init__(self, id):
        daemon.__init__(self, self.__class__.__name__, id)
        source_ip = '192.168.6.{0}'.format(self.id * 2 + 1)
        config = ConfigParser.ConfigParser()
        config.read('/etc/{0}.conf'.format(self.name))
        for (name, val) in config.items('DEFAULT'):
            if name == 'source-ip':
                source_ip = val
            elif name == 'allowed-headers':
                self.args += ['--allow-header=' + header for header in val.split(',')]
            else:
                self.args.append('--{0}={1}'.format(name, val))

        self.args += [
            '--ingoing-http={0}:3128'.format(source_ip),
            '--ingoing-stat=/var/run/{0}/{1}.sock'.format(self.name, self.nameid),
            '--outgoing-http={0}'.format(source_ip),
            '--outgoing-ns={0}'.format(source_ip),
        ]

def main():
    commands = ['start', 'stop', 'restart', 'reload', 'status']
    command = ''
    try:
        command = sys.argv[1]
        if command not in commands:
            raise BaseException('invalid command')
        if len(sys.argv) > 2:
            ids = map(int, sys.argv[2].split(','))
        else:
            ids = range(128)
    except:
        print('Usage: {0} [{1} [id(,id)]]'.format(sys.argv[0], '|'.join(commands)))
        return 1

    workers = []
    for i in ids:
        workers.append(fastproxy(i))

    sys.stdout.write('{0}ing.'.format(command))
    sys.stdout.flush()
    
    result = 0

    while workers:
        for w in workers[:]:
            try:
                if getattr(w, command)():
                    workers.remove(w)
            except BaseException, e:
                result = 1
                sys.stderr.write('{0}: {1}\n'.format(w.nameid, e))
                sys.stderr.flush()
                workers.remove(w)
        time.sleep(0.5)
        sys.stdout.write('.')
        sys.stdout.flush()
    sys.stdout.write('\n')

    return result

if __name__ == '__main__':
    sys.exit(main())
