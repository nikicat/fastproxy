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

class Test(unittest.TestCase):
    port = 32567

    def setUp(self):
        print(os.getcwd())
        self.fastproxy = Popen('../build/debug/src/fastproxy --inbound=127.0.0.1:{0} --receive-timeout=5 --name-server=8.8.8.8'.format(self.port), shell=True, env={'LD_LIBRARY_PATH': '/usr/local/lib64'})
        time.sleep(1)

    def tearDown(self):
        self.fastproxy.terminate()
        self.fastproxy.wait()

    def test_running(self):
        self.assertFalse(self.fastproxy.poll())

    def test_http(self):
        urllib.urlopen('http://ya.ru', proxies={'http': 'http://localhost:{0}'.format(self.port)})

if __name__ == "__main__":
    #sys.argv = ['', 'Test.test_running', 'Test.test_http']
    unittest.main()