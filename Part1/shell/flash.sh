#!/bin/bash
# Short script to toggle a GPIO pin at the highest frequency
# possible from Bash using gpiod
while true; do
  VAL=$(gpioget -c gpiochip0 --numeric 25)
  gpioset -t0 -c gpiochip0 17=$VAL
done