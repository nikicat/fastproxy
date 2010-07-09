'''
Created on Jul 8, 2010

@author: nbryskin
'''
import unittest
from subprocess import Popen
import urllib
import os
import time
import sys
import socket

class Test(unittest.TestCase):
    port = 32567
    timeout = 5
    allowed_header = 'AllowedHeader'

    def setUp(self):
        self.fastproxy = Popen('../build/debug/src/fastproxy \
            --inbound=127.0.0.1:{0} --receive-timeout={1} \
            --name-server=8.8.8.8 --allowed-header={2}'.format(self.port, self.timeout, self.allowed_header),
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
        
    def _send_request(self, headers=None):
        l = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        l.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        l.bind(('localhost', self.port + 1))
        l.listen(5)
        self.c = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        request = 'GET http://localhost:{0} HTTP/1.0\r\n{1}\r\n'.format(self.port + 1, headers or '')
        self.c.connect(('localhost', self.port))
        self.c.send(request)
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

if __name__ == "__main__":
    #sys.argv = ['', 'Test.test_header_filter']
    unittest.main()