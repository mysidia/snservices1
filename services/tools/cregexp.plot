set output "cregexp.png"
set terminal png color small
set xlabel "Days since Jan 1, 1997"
set ylabel "Number channels"
set grid
set size 1.0,0.8

plot "cregstats.dat" title "Registered" with lines lt 2, \
     "cexpstats.dat" title "Expired" with lines lt 3;

set output "creg.png"
plot "cregstats.dat" title "Registered" with lines lt 2;

set output "cexp.png"
plot "cexpstats.dat" title "Expired" with lines lt 3;

###########################

set yrange [0:18000]
set xrange [0:1800]

set output "dailychans.png"
plot "maxchans.dat" title "Most registered chans today" with points, \
     "minchans.dat" title "Least registered chans today" with lines;


###########################

set output "cnexp.png"
set ylabel "Number Expired"
plot "cexpstats.dat" title "Channels" with lines lt 3, \
     "expstats.dat" title "Nicks" with lines lt 9;

set output "cnreg.png"
set ylabel "Number Registered"
plot "cregstats.dat" title "Channels" with lines lt 2, \
     "regstats.dat" title "Nicks" with lines lt 1;

set output "cregexp.gif"
set terminal gif

plot "cregstats.dat" title "Registered" with lines lt 2, \
     "cexpstats.dat" title "Expired" with lines lt 3;

#plot "regstats.dat" title "Registered" with lines lt XX;

