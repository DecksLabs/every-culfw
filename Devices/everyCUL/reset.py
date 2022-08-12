import serial
import time
import sys

com=serial.Serial(sys.argv[1],1200,dsrdtr=True)
com.dtr=True
com.write('0000'.encode())
time.sleep(2)
com.dtr=False
time.sleep(2)
com.close()

