# Echo client
# Modified from: https://docs.python.org/2/library/socket.html
import socket

HOST = '127.0.0.1'    # Change me to host ip
PORT = 5000           # The same port as used by the server

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.connect((HOST, PORT))
s.sendall('Hello, world 1')
data = s.recv(1024)
print 'Received', repr(data)
s.sendall('Hello, world 2')
data = s.recv(1024)
print 'Received', repr(data)
s.sendall('Hello, world 3')
data = s.recv(1024)
print 'Received', repr(data)
s.close()
