#! /usr/bin/python
import RPi.GPIO as GPIO ## Import GPIO library
import sys
import time
import logging
import RPi.GPIO as GPIO
import Adafruit_DHT

# Use BCM GPIO references
# instead of physical pin numbers
GPIO.setmode(GPIO.BCM)

def getSensorData():
    RH, T = Adafruit_DHT.read_retry(Adafruit_DHT.DHT22, 22)
    return (str(RH), str(T))

def gpio_control(status, pin):

    try:
        pinNum = int(pin)
    except Exception as e:
        print('Pin number not valid.')
        return -1

    GPIO.setup(pinNum, GPIO.OUT)

    if status in ['on', 'high']:    GPIO.output(pinNum, GPIO.HIGH)
    if status in ['off', 'low']:    GPIO.output(pinNum, GPIO.LOW)

    print('Turning pin {} {}'.format(pin, status))
    return 0

def main():
    gpio_control(sys.argv[1], sys.argv[2])  


if __name__ == "__main__":
    main()
