set output "emails.png"
set terminal png color small
set xlabel "Days since Jan 1, 1997"
set ylabel "Number registered nicks"
set grid
set size 0.8,0.8
plot "regnicks.dat" title "Nicks" with lines, "emails.dat" title "Emails" with lines, \
 "validemails.dat" title "Emails (other than 'none')" with lines

set output "len.png"
plot "emaillen_av.dat" with lines

set output "noneemails.png"
plot  "noneemails.dat" title "\"None\" emails" with lines
# "regnicks.dat" title "Nicks" with lines, \
#      "validemails.dat" title "Emails" with lines

set output "percnone.png"
plot   "percnone.dat" title "Percent nicks with \"None\" emails" with lines

