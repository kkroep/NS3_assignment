#!/bin/bash
../waf --run up2down1User
gnuplot ../up2down1User.plt
eog ./up2down1User.png &
rm ../up2down1*
