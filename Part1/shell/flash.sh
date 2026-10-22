#!/bin/bash
# Short script to toggle a GPIO pin at the highest frequency
# possible from Bash using gpiod
while true; do
 VAL=$(gpioget 0 25)
 gpioset 0 17=$VAL
done