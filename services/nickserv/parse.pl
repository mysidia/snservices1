#!/usr/bin/perl
#given an old-style nickserv logfile, convert it

$pfx='NS';
$pfxx="$pfx" . "E";

open(F, "<nickserv.log");

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
    # Just scrap these
    $i = 0;
    $ll =~ s#^\([0-9].:..\[../../....\]\) Not expiring\ (.*), held/banished##g;
    $ll =~ s#^\([0-9].:..\[../../....\]\) Not expiring\ (.*), held##g;
    $ll =~ s#^\([0-9].:..\[../../....\]\) Not expiring\ (.*), banished##g;

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
            $i++;
        }

        $ll =~ s#^(.*)!(.*@*): Failed to identify remotely to (.*) \(incorrect password\)#$pfx\_IDENTIFY $lt \1!\2 1 \3#g;
        $ll =~ s#^(.*)!(.*@*): Failed to identify remotely to (.*) \(invalid MD5 hash\)#$pfx\_IDENTIFY $lt \1!\2 1 \3 (MD5)#g;
        $ll =~ s#^(.*)!(.*@*): Bypassed AHURT ban with registered nick (.*)#$pfx\_IDENTIFY $lt \1!\2 0 \3 AHURT#g;
        $ll =~ s#^(.*)!(.*@*): lists autohurt bypasses matching (.*)#$pfx\_BYPASS $lt \1!\2 0 \3 LIST#g;
        $ll =~ s#^(.*)!(.*@*): sets autohurt bypasses on all matching (.*)#$pfx\_BYPASS $lt \1!\2 8 \3#g;
        $ll =~ s#^(.*)!(.*@*): set mark on nick (.*)#$pfx\_MARK $lt \1!\2 8 \3#g;

# Mysidia_!mysidia@130.70.63.25: Failed to identify remotely to Mysidia (invalid
#MD5 hash)
# Mysidia_!mysidia@130.70.63.25: Failed to identify remotely to Mysidia (invalid
#MD5 hash)

# |Gredlen|!.....@a204b210n113client93.hawaii.rr.com: Failed to identify remotely to id (incorrect password)
# |Gredlen|!.....@a204b210n113client93.hawaii.rr.com: Failed to identify remotely to id (incorrect password)
# Vinny!Node0@156.46.16.31: Failed to identify remotely to Qwerty (incorrect password)
# Allie!_@dialup-64.154.96.207.Cincinnati1.Level3.net: Bypassed AHURT ban with registered nick Allie


# blah!anyone@206.230.157.139: DROP
#foo!root@a.server: REGISTER
#(10:35[10/05/1997]) blah!anyone@206.230.157.139: DROP
#(10:43[10/05/1997]) Shadow!vertical@slip119.vianet.net.au: REGISTER



#mysid EXPIRE: 974695840 968191199 6504641

        printf("%s", $ll);
    }
}
