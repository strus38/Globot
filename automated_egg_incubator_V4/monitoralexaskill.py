#! /usr/bin/python
import RPi.GPIO as GPIO
# Import required libraries
from flask import Flask
from flask_ask import Ask, statement
import logging
import Adafruit_DHT

# Use BCM GPIO references
# instead of physical pin numbers
GPIO.setmode(GPIO.BCM)

app = Flask(__name__)
ask = Ask(app, '/')

logging.getLogger("flask_ask").setLevel(logging.DEBUG)


def getSensorData():
    RH, T = Adafruit_DHT.read_retry(Adafruit_DHT.DHT22, 22)
    return (str(RH), str(T))


@ask.intent('GPIOControlIntent', mapping={'status': 'status', 'pin': 'pin'})
def gpio_control(status, pin):

    try:
        pinNum = int(pin)
    except Exception as e:
        return statement('Pin number not valid.')

    GPIO.setup(pinNum, GPIO.OUT)

    if status in ['on', 'high']:
        GPIO.output(pinNum, GPIO.HIGH)
    if status in ['off', 'low']:
        GPIO.output(pinNum, GPIO.LOW)

    return statement('Turning pin {} {}'.format(pin, status))


@ask.intent(
    'LocationControlIntent',
    mapping={
        'status': 'status',
        'location': 'location'
    })
def location_control(status, location):

    locationDict = {'motor': 24, 'light': 25}

    targetPin = locationDict[location]

    GPIO.setup(targetPin, GPIO.OUT)

    if status in ['on', 'high']:
        GPIO.output(targetPin, GPIO.HIGH)
    if status in ['off', 'low']:
        GPIO.output(targetPin, GPIO.LOW)

    GPIO.cleanup()
    return statement('Turning {} {}!'.format(location, status))


@ask.intent('EnvironmentIntent', mapping={'environment': 'environment'})
def location_control(environment):

    hum, temp = getSensorData()
    h = float(hum)
    t = float(temp)
    if environment in ['temperature', 'temp']:
        return statement('Temperature is {0:.2f}'.format(t))
    elif environment in ['humidity', 'hum']:
        return statement('Humidity is {0:.2f}'.format(h))
    elif environment in ['all']:
        return statement(
            'Temperature is {0:.2f} and Humidity is {0:.2f}'.format(t, h))
    else:
        return statement('I cannot find the data you want')


if __name__ == '__main__':
    port = 5000  #the custom port you want
    app.run(host='0.0.0.0', port=port)
