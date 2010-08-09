'''
Created on Jul 8, 2010

@author: nbryskin
'''
import unittest
from subprocess import Popen
import urllib
import time
import socket

class Test(unittest.TestCase):
    port = 32567
    timeout = 5
    allowed_header = 'AllowedHeader'
    stat_sock = '/tmp/stat.sock'

    def setUp(self):
        self.fastproxy = Popen('../build/debug/src/fastproxy \
            --ingoing-http=127.0.0.1:{0} --receive-timeout={1}\
            --name-server=95.108.198.4 --allow-header={2} --ingoing-stat={3}'.format(self.port, self.timeout, self.allowed_header, self.stat_sock),
            shell=True, env={'LD_LIBRARY_PATH': '/usr/local/lib64'})
        time.sleep(1)

    def tearDown(self):
        #pass
        self.fastproxy.terminate()
        self.fastproxy.wait()

    def test_running(self):
        self.assertFalse(self.fastproxy.poll())

    def test_simple(self):
        urllib.urlopen('http://ya.ru', proxies={'http': 'http://localhost:{0}'.format(self.port)})

    def test_simple_ip(self):
        urllib.urlopen('http://77.88.21.3', proxies={'http': 'http://localhost:{0}'.format(self.port)})
        
    def _send_request(self, headers=None, method='GET', host='localhost'):
        l = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        l.bind(('localhost', self.port + 1))
        l.listen(5)
        self.c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        request = '{2} http://{3}:{0} HTTP/1.0\r\n{1}\r\n'.format(self.port + 1, headers or '', method, host)
        self.c.connect(('localhost', self.port))
        self.c.send(request)
        l.settimeout(1)
        s, addr = l.accept()
        return s.recv(len(request))
        
    def test_http(self):
        request = self._send_request()
        self.assertEqual(request, 'GET / HTTP/1.0\r\n\r\n')

    def test_timeout(self):
        self._send_request()
        time.sleep(self.timeout)
        self.assertRaises(BaseException, self.c.send('a'))

    def test_timeout_crash(self):
        for i in xrange(100):
            self._send_request()
        time.sleep(self.timeout)
        self.assertFalse(self.fastproxy.poll())
        self.assertRaises(BaseException, self.c.send('a'))
        
    def test_header_filter(self):
        allowed_header = '{0}: test\r\n'.format(self.allowed_header)
        disallowed_header = 'DisAllowedHeader: test\r\n'
        request = self._send_request(allowed_header)
        self.assertEqual(request, 'GET / HTTP/1.0\r\n{0}\r\n'.format(allowed_header))

        request = self._send_request(disallowed_header)
        self.assertEqual(request, 'GET / HTTP/1.0\r\n\r\n')

        header = '{1}{1}{0}{0}{1}{1}'.format(allowed_header, disallowed_header)
        request = self._send_request(header)
        self.assertEqual(request, 'GET / HTTP/1.0\r\n{0}{0}\r\n'.format(allowed_header))

        header = '{0}{1}{0}{1}{0}'.format(allowed_header, disallowed_header)
        request = self._send_request(header)
        self.assertEqual(request, 'GET / HTTP/1.0\r\n{0}{0}{0}\r\n'.format(allowed_header))

    def test_http_methods(self):
        method='DELETE'
        request = self._send_request(method=method)
        self.assertEqual(request, '{0} / HTTP/1.0\r\n\r\n'.format(method))

    def test_resolve_self_ip(self):
        host = '127.0.0.1'
        request = self._send_request(host=host)
        self.assertEqual(request, 'GET / HTTP/1.0\r\n\r\n')

    def test_statistics(self):
        self.stat = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        self.stat.connect(self.stat_sock)
        
        self.test_http()
        self.c.close()
        self.stat.send('total_sessions current_sessions total_stat_sessions current_stat_sessions unexisting_stat\n')
        self.assertEqual(self.stat.recv(64), '1\t0\t1\t1\tunexisting_stat?\n')
        
        self.test_http()
        self.c.close()
        self.stat.send('total_sessions current_sessions total_stat_sessions current_stat_sessions unexisting_stat\n')
        self.assertEqual(self.stat.recv(64), '2\t0\t1\t1\tunexisting_stat?\n')

if __name__ == "__main__":
    import sys
    #sys.argv = ['', 'Test.test_simple_ip']
    unittest.main()