#!/bin/bash
../waf --run up1down2
gnuplot ../up1down2.plt
eog ./up1down2.png &
rm ../up1down2*

../waf --run up1down2User
gnuplot ../up1down2User.plt
eog ./up1down2User.png &
rm ../up1down2*
