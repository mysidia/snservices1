set output "NickExpire.pbm"
set terminal pbm color small
set xlabel "Days Until Expiration"
set ylabel "Number of Nicknames"
set grid
set size 0.5,0.5
plot "NickExpire.dat" with lines

set output "NickLength.pbm"
set terminal pbm color small
set xlabel "Length of Nickname"
set ylabel "Number of Nicknames"
set grid
set size 0.5,0.5
plot "NickLength.dat" with lines
