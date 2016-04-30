#!/bin/bash
../waf --run up2down1
gnuplot ../up2down1.plt
eog ./up2down1.png &
rm ../up2down1*

../waf --run up2down1User
gnuplot ../up2down1User.plt
eog ./up2down1User.png &
rm ../up2down1*
