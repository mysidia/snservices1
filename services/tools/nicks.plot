set output "regnicks.png"
set terminal png color small
set xlabel "Days since Jan 1, 1997"
set ylabel "Number registered nicks Expiring"
set grid
set size 1.0,0.8

#set output "regnicks.gif"
#set terminal gif
# jpg color small
plot "regnicks.dat" title "Nicks" with lines, "validemails.dat" title "E-mails" with lines, "accitems.dat" title "Access List Entries" with lines

set terminal png color small

set output "nicks-old.png"
#plot "nicks-old.dat" title "Nicks" with lines
plot "nicks-old.dat" title "Nicks" with lines, "validemails.dat" title "E-mails" with lines, "accitems.dat" with lines

set output "expire.png"
set size 1.0,0.5
set ylabel "Registered nicks expiring"
plot "nexp0.dat" title "Today" with lines, \
"nexp1.dat" title "Tomorrow" with lines, \
"nexp2.dat" title "in 2" with lines, \
"nexp3.dat" title "in 3" with lines, \
"nexp4.dat" title "in 4" with lines, \
"nexp5.dat" title "in 5" with lines

set output "expire2.png"
plot "nexp6.dat" title "in 6" with lines, \
"nexp7.dat" title "in 7" with lines, \
"nexp8.dat" title "in 8" with lines, \
"nexp9.dat" title "in 9" with lines, \
"nexp10.dat" title "in 10" with lines, \
"nexp11.dat" title "in 11" with lines

set output "expire3.png"
plot "nexp12.dat" title "in 12" with lines, \
"nexp13.dat" title "in 13" with lines, \
"nexp14.dat" title "in 14" with lines, \
"nexp15.dat" title "in 15" with lines, \
"nexp16.dat" title "in 16" with lines, \
"nexp16.dat" title "in 17" with lines

set output "expire4.png"
plot "nexp18.dat" title "in 18" with lines, \
"nexp19.dat" title "in 19" with lines, \
"nexp20.dat" title "in 20" with lines, \
"nexp21.dat" title "in 21" with lines, \
"nexp22.dat" title "in 22" with lines, \
"nexp23.dat" title "in 23" with lines


set output "expire5.png"
plot "nexp24.dat" title "in 24" with lines
