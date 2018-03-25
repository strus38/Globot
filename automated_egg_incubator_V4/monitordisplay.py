#!/usr/bin/env python

import time

import Adafruit_SSD1306

from PIL import Image
from PIL import ImageDraw
from PIL import ImageFont

import subprocess

# Raspberry Pi pin configuration:
RST = None  # on the PiOLED this pin isnt used
# Note the following are only used with SPI:
DC = 23
SPI_PORT = 0
SPI_DEVICE = 0


def main():
    # 128x32 display with hardware I2C:
    #disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST)

    # 128x64 display with hardware I2C:
    disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST)

    # Note you can change the I2C address by passing an i2c_address parameter like:
    # disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST, i2c_address=0x3C)

    # Alternatively you can specify an explicit I2C bus number, for example
    # with the 128x32 display you would use:
    # disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, i2c_bus=2)

    # 128x32 display with hardware SPI:
    # disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, dc=DC, spi=SPI.SpiDev(SPI_PORT, SPI_DEVICE, max_speed_hz=8000000))

    # 128x64 display with hardware SPI:
    # disp = Adafruit_SSD1306.SSD1306_128_64(rst=RST, dc=DC, spi=SPI.SpiDev(SPI_PORT, SPI_DEVICE, max_speed_hz=8000000))

    # Alternatively you can specify a software SPI implementation by providing
    # digital GPIO pin numbers for all the required display pins.  For example
    # on a Raspberry Pi with the 128x32 display you might use:
    # disp = Adafruit_SSD1306.SSD1306_128_32(rst=RST, dc=DC, sclk=18, din=25, cs=22)

    # Initialize library.
    disp.begin()

    # Clear display.
    disp.clear()
    disp.display()

    # Create blank image for drawing.
    # Make sure to create image with mode '1' for 1-bit color.
    width = disp.width
    height = disp.height
    image = Image.new('1', (width, height))

    # Get drawing object to draw on image.
    draw = ImageDraw.Draw(image)

    # Draw a black filled box to clear the image.
    draw.rectangle((0, 0, width, height), outline=0, fill=0)

    # Draw some shapes.
    # First define some constants to allow easy resizing of shapes.
    padding = -2
    top = padding
    bottom = height - padding
    # Move left to right keeping track of the current x position for drawing shapes.
    x = 10

    # Load default font.
    #font = ImageFont.load_default()

    # Alternatively load a TTF font.  Make sure the .ttf font file is in the same directory as the python script!
    # Some other nice fonts to try: http://www.dafont.com/bitmap.php
    font = ImageFont.truetype('monkey.ttf', 10)

    while True:
        try:
            # Draw a black filled box to clear the image.
            draw.rectangle((0, 0, width, height), outline=0, fill=0)

            cmd = "hostname -I | cut -d\' \' -f1"
            IP = subprocess.check_output(cmd, shell=True)
            cmd = "/home/pi/query.sh"
            Q = subprocess.check_output(cmd, shell=True)
            cmd = "cat .last | ./jq  '.results[0].series[0].values[0][1]'"
            T = subprocess.check_output(cmd, shell=True)
            cmd = "cat .last | ./jq  '.results[1].series[0].values[0][1]'"
            H = subprocess.check_output(cmd, shell=True)
            cmd = "cat .last | ./jq  '.results[2].series[0].values[0][1]'"
            LEFT = subprocess.check_output(cmd, shell=True)

            date = subprocess.check_output("date '+%d/%m/%y %H:%M:%S'", shell=True)
            # Write two lines of text.

            draw.text((x, top + 2), str(date), font=font, fill=255)
            draw.text((x, top + 12), "IP: " + str(IP), font=font, fill=255)
            draw.text((x, top + 22), "Temp °C: " + str(T), font=font, fill=255)
            draw.text((x, top + 32), "Hum %: " + str(H), font=font, fill=255)
            draw.text((x, top + 42), "Birth in "+str(LEFT)+" days", font=font, fill=255)
        except Exception as e:
            pass

        # Display image.
        disp.image(image)
        disp.display()
        time.sleep(5)


# call main
if __name__ == '__main__':
    main()
