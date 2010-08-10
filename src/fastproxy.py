#!/usr/bin/env python

from subprocess import Popen
import os
import signal
import os.path
import time
import sys
import resource

class daemon(object):
    def __init__(self, name, id):
        self.name = name
        self.id = id
        self.nameid = '{0}{1:03}'.format(self.name, self.id)
        self.pid_file = '/var/run/{0}/{1}.pid'.format(self.name, self.nameid)

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

#        source_ip = '192.168.6.{0}'.format(self.id * 2 + 1)
        source_ip = '0.0.0.0'
        name_server = '213.234.192.8'
#        name_server_source = source_ip
        name_server_source = '0.0.0.0'

        self.executable = '/home/nbryskin/workspace/fastproxy/build/release/src/{0}'.format(self.name)
        self.args=[self.name,
            '--ingoing-http=127.0.0.1:{0}'.format(self.id + 3200),
            '--ingoing-stat=/var/run/{0}/{1}.sock'.format(self.name, self.nameid),
            '--allow-header=Allow',
            '--allow-header=Authorization',
            '--allow-header=WWW-Authenticate',
            '--allow-header=Proxy-Authorization',
            '--allow-header=Proxy-Authenticate',
            '--allow-header=Cache-Control',
            '--allow-header=Content-Encoding',
            '--allow-header=Content-Length',
            '--allow-header=Content-Type',
            '--allow-header=Date',
            '--allow-header=Expires',
            '--allow-header=Host',
            '--allow-header=If-Modified-Since',
            '--allow-header=Last-Modified',
            '--allow-header=Location',
            '--allow-header=Pragma',
            '--allow-header=Accept',
            '--allow-header=Accept-Charset',
            '--allow-header=Accept-Encoding',
            '--allow-header=Accept-Language',
            '--allow-header=Content-Language',
            '--allow-header=Mime-Version',
            '--allow-header=Retry-After',
            '--allow-header=Title',
            '--allow-header=Connection',
            '--allow-header=User-Agent',
            '--allow-header=Referer',
            '--allow-header=Cookie',
            '--allow-header=Set-Cookie',
            '--name-server={0}'.format(name_server),
            '--outgoing-http={0}'.format(source_ip),
            '--outgoing-ns={0}'.format(name_server_source),
            '--receive-timeout=999999'
#                              '--log-level=9',
#                              '--log-channel=statistics',
#                              '--log-channel=proxy',
#                              '--log-channel=chater',
#                              '--log-channel=fastproxy',
        ]

def main():
    commands = ['start', 'stop', 'restart', 'reload', 'status']
    command = ''
    if len(sys.argv) > 1:
        command = sys.argv[1]
    if command not in commands:
        print('Usage: {0} [{1}]'.format(sys.argv[0], '|'.join(commands)))
        return

    workers = []
    for i in range(128):
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
