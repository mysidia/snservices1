set output "regexp.png"
set terminal png color small
set xlabel "Days since Jan 1, 1997"
set ylabel "Number nicknames"
set grid
set size 1.0,0.8

plot "regstats.dat" title "Registered" with lines lt 1, \
     "expstats.dat" title "Expired" with lines lt 9;

set output "reg.png"
plot "regstats.dat" title "Registered" with lines lt 1;

set output "exp.png"
plot "expstats.dat" title "Expired" with lines lt 9;

set output "dailynicks.png"

set yrange [0:13000]
set xrange [0:2000]

set yrange [0:18000]
set xrange [0:2000]

#set yrange [0:5]
#set xrange [430.9:431.2]
plot "maxnicks.dat" title "Most registered nicks today" with points, \
     "minnicks.dat" title "Least registered nicks today" with lines, \
     "noneemails.dat" title "'None' as email" with lines;

set output "regexp.gif"
set terminal gif

plot "regstats.dat" title "Registered" with lines, \
     "expstats.dat" title "Expired" with lines;

#plot "regstats.dat" title "Registered" with lines lt 1;

