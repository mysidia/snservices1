#!/usr/bin/perl
#given an old-style nickserv logfile, convert it

$pfx='CS';
$pfxx="$pfx" . "E";

open(F, "<chanserv.log");

use POSIX qw(mktime);

sub xform_time {
   my ($v) = @_;

   $v =~ s#\(##;
   $v =~ s#\)##;
   $v =~ s#\]##;
   @y = split(/[:\/\[]/, $v, 5);

   my ($hours, $mins, $day, $month, $year) = @y;

   if ($year > 1000) {
       $year = ($year - 1900);
   }

   return mktime (0, $mins, $hours, $day, $month, $year);
}

while ($ll = <F>) {
    # Just scrap the not-expired logs
    $i = 0;
    $ll =~ s#^\([0-9].:..\[../../....\]\) (.*) NOT expired, (.*)##g;

    if (length($ll) < 19) {
    }
    else {
        # Read the logtime
        $lt = substr($ll, 0, 19);
        $lt = xform_time($lt);

#        $lt = ~s#^ (.*)$#\1#sg;

        $ll =~ s#^\([0-9].:..\[../../....\]\)##sg;

        $ll =~ s#(.*) EXPIRE: ([0-9]+) ([0-9]+) ([0-9]+)#$pfxx\_EXPIRE $lt - 0\1 \2#sg;
        $ll =~ s#(.*) EXPIRE#$pfxx\_EXPIRE $lt - 0\1#sg;

        @cmds = (
          "REGISTER",
          "DROP",
        );

        while($i <= $#cmds) {
            $thecmd = $cmds[$i];

            $ll =~ s#(.*)!(.*@*): $thecmd#$pfx\_$thecmd $lt \1!\2 0\1#sg;
            $ll =~ s#(.*)!(.*@*) \(override\) $thecmd \((.*)\)#$pfx\_$thecmd $lt \1!\2 0 \3 (override)#sg;
            $ll =~ s#(.*)!(.*@*) $thecmd \((.*)\)#$pfx\_$thecmd $lt \1!\2 0 \3#sg;
            $i++;
        }

        $ll =~ s#^(.*)!(.*@*): failed DROP on (.*)#$pfx\_DROP $lt \1!\2 2 \3#g;
        $ll =~ s#^(.*) failed DROP on (.*)#$pfx\_DROP $lt \1 2 \2#g;
        $ll =~ s#^(.*)!(.*@*) set hold on channel (.*)#$pfx\_HOLD $lt \1!\2 8 \3#g;
        $ll =~ s#^(.*)!(.*@*) removed hold on channel (.*)#$pfx\_HOLD $lt \1!\2 4 \3#g;
        $ll =~ s#^(.*)!(.*@*) DROPPED (.*)#$pfx\_DROP $lt \1!\2 0 \3#g;
        $ll =~ s#^(.*) DROPPED (.*)#$pfx\_DROP $lt \1!\?@\? 0\3#g;
        $ll =~ s#^(.*)!(.*@*) set banish on channel (.*)#$pfx\_BANISH $lt \1!\2 0 \3#g;
        $ll =~ s#^(.*)!(.*@*) set mark on channel (.*)#$pfx\_BANISH $lt \1!\2 8 \3#g;
        $ll =~ s#^(.*)!(.*@*) removed mark on channel (.*)#$pfx\_BANISH $lt \1!\2 4 \3#g;
        $ll =~ s#^(.*)!(.*@*) set banished on channel (.*)#$pfx\_BANISH $lt \1!\2 8 \3#g;
        $ll =~ s#^(.*)!(.*@*) removed banish on channel (.*)#$pfx\_BANISH $lt \1!\2 4 \3#g;
        $ll =~ s#^(.*)!(.*@*) removed banished on channel (.*)#$pfx\_BANISH $lt \1!\2 4 \3#g;
        $ll =~ s#^(.*)!(.*@*) removed banish from channel (.*)#$pfx\_BANISH $lt \1!\2 4 \3#g;
        $ll =~ s#^(.*)!(.*@*) closed channel (.*)#$pfx\_BANISH $lt \1!\2 8 \3#g;

# Mysidia!mysidia@208.231.103.84 set banish on channel #gameserv
# Mysidia!mysidia@208.231.103.84 set banish on channel #infoserv

        printf("%s", $ll);
    }
}
