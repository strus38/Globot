#!/usr/bin/env python

import time
import requests
import sys
from time import sleep
import Adafruit_DHT
import RPi.GPIO as GPIO
import ConfigParser
import datetime

hum_pin = 0
motor_pin = 0
db_uri = ""
incubation_date = ""
birth_date = ""


def init_GPIO():
    global hum_pin, motor_pin
    print("init_GPIO: Starting init...")
    try:
        GPIO.setmode(GPIO.BCM)
        hum_pin = int(hum_pin)
        motor_pin = int(motor_pin)
    except Exception as e:
        print('Pin number not valid.')
        return -1

    GPIO.setup(hum_pin, GPIO.OUT)
    GPIO.setup(motor_pin, GPIO.OUT)
    print("init_GPIO: Done.")


def getSensorData():
    RH, T = Adafruit_DHT.read_retry(Adafruit_DHT.DHT22, 18)
    return (str(RH), str(T))


def gpio_control(pin, status):
    print("gpio_control start...")
    try:
        pinNum = int(pin)
    except Exception as e:
        print('Pin number not valid.')
        return -1

    if status in ['on', 'high']:
        GPIO.output(pinNum, GPIO.HIGH)
    if status in ['off', 'low']:
        GPIO.output(pinNum, GPIO.LOW)

    print('Turning pin {} {}'.format(pin, status))
    return 0


def writeDB(rh, t, time_left):
    try:
        print("Hum=%s - Temp=%s" % (rh, t))
        tstamp = int(time.time()) * 10 ** 9
        headers = {'Content-Type': 'application/x-www-form-urlencoded'}
        data = "temp,couv=couv1 value=%.2f %d\n" % (float(t), tstamp)
        requests.post(db_uri, data=data, headers=headers)
        data = "hum,couv=couv1 value=%.2f %d\n" % (float(rh), tstamp)
        requests.post(db_uri, data=data, headers=headers)
        data = "left,couv=couv1 value=%d %d\n" % (int(time_left), tstamp)
        requests.post(db_uri, data=data, headers=headers)
    except Exception as e:
        print("Error: %s (%s)" % (e.message, sys.exc_info()[0]))
        pass


def getTimeLeft():
    dt = datetime.datetime
    now = dt.now()
    # This gives timedelta in days
    return (birth_date - dt(year=now.year, month=now.month, day=now.day)).days


def egg_rotate():
    print("Turning the eggs ... Starting.")
    gpio_control(motor_pin, 'on')
    sleep(10)
    gpio_control(motor_pin, 'off')
    print("Turning the eggs ... Done.")


def main():
    i = 0
    try:
        print('Welcome in the automatic egg incubator V3...')
        init_GPIO()
        while True:
            time_left = getTimeLeft()
            RH, T = getSensorData()
            writeDB(RH, T, time_left)
            # Make sure the humidity is within range.
            if time_left == 0:
                break
            else:
                if float(RH) < 25:
                    gpio_control(hum_pin, 'on')
                elif time_left < 4 and float(RH) < 70:
                    gpio_control(hum_pin, 'on')
                else:
                    gpio_control(hum_pin, 'off')
            i += 1
            if i == 3600:
                egg_rotate()
                i = 0
            sleep(10)
    except KeyboardInterrupt:
        # here you put any code you want to run before the program
        # exits when you press CTRL+C
        print("Keyboard interruption")
    except Exception as e:
        # this catches ALL other exceptions including errors.
        # You won't get any error messages for debugging
        # so only use it once your code is working
        print("Other error or exception occurred: %s" % e.message)
        pass
    finally:
        i = 0
        GPIO.cleanup() # this ensures a clean exit


# call main
if __name__ == '__main__':
    config = ConfigParser.ConfigParser()
    config.readfp(open(r'config.conf'))
    incubation_date = datetime.datetime.strptime(config.get('Params', 'incubation_date'), "%d/%m/%y")
    birth_date = incubation_date + datetime.timedelta(days=21)
    db_uri = config.get('Params', 'db_uri')
    hum_pin = config.get('Params', 'hum_pin')
    motor_pin = config.get('Params', 'motor_pin')
    main()
