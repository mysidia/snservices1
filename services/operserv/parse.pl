#!/usr/bin/perl
#given an old-style nickserv logfile, convert it

$pfx='OS';
$pfxx="$pfx" . "E";

open(F, "<operserv.log");

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
#    $ll =~ s#^\([0-9].:..\[../../....\]\) (.*) NOT expired, (.*)##g;

    $ll =~ s#^(\([0-9].:..\[../../....\]\)): #\1#g;


    if (length($ll) < 19) {
    }
    else {
        # Read the logtime
        $lt = substr($ll, 0, 19);
        $lt = xform_time($lt);

#        $lt = ~s#^ (.*)$#\1#sg;

        $ll =~ s#^\([0-9].:..\[../../....\]\)*##sg;

        $ll =~ s#(.*) EXPIRE: ([0-9]+) ([0-9]+) ([0-9]+)#$pfxx\_EXPIRE $lt - 0\1 \2#sg;
        $ll =~ s#(.*) EXPIRE#$pfxx\_EXPIRE $lt - 0\1#sg;
        $ll =~ s#^(.*)@(.*): autokill#\1@\2: akill#ig;
        $ll =~ s#^(.*)@(.*): autohurt#\1@\2: ahurt#ig;
#        $ll =~ s#^(.*)@(.*): rehash#\1@\2: reset#ig;

        @cmds = (
          "AKILL","AHURT", "TEMPAKILL", "CLONERULE", "IGNORE",
          "MODE", "RAW", "SHUTDOWN", "RESET", "REHASH",
          "JUPE", "UPTIME", "TIMERS", "SYNC", "TRIGGER",
          "MATCH", "CLONESET", "REMROOT", "SETOP", "GRPOP", "OVERRIDE",
          "STRIKE",

          # Retired/short-lived commands
          "GETKEY", "NICKDUMP", "ALLOCSTAT", "SEX", "PURGE", "ALLUSERS",
          "HASHDUMP",
        );

        while($i <= $#cmds) {
            $thecmd = $cmds[$i];

            $ll =~ s#(.*)!(.*@*): $thecmd#$pfx\_$thecmd $lt \1!\2 0\3#sgi;
            $ll =~ s#(.*)!(.*@*) $thecmd \((.*)\)#$pfx\_$thecmd $lt \1!\2 0 \3#sgi;
            $i++;
        }

        $ll =~ s#^(.*)!(.*@*) GETPASS \#(.*)#CS_GETPASS $lt \1!\2 0 \#\3#gi;
        $ll =~ s#^(.*)!(.*@*) GETPASS (.*)#NS_GETPASS $lt \1!\2 0 \3#gi;
        $ll =~ s#^(.*)!(.*@*) GETREALPASS \#(.*)#CS_GETREALPASS $lt \1!\2 0 \#\3#gi;
        $ll =~ s#^(.*)!(.*@*) GETREALPASS (.*)#NS_GETREALPASS $lt \1!\2 0 \3#gi;
        $ll =~ s#^(.*)!(.*@*) set flag\(s\): (.*) on (.*)#NS_SETFLAG $lt \1!\2 0 \4 \3#gi;
        $ll =~ s#^(.*)!(.*@*) BANISHed (.*)#NS_BANISH $lt \1!\2 0 \3#gi;
        $ll =~ s#^(.*)!(.*@*) REMSRA (.*)#OS_R $lt \1!\2 0 \3#gi;
        $ll =~ s#^(.*)!(.*@*) REMROOT (.*)#OS_R $lt \1!\2 0 \3#gi;
        $ll =~ s#^(.*)!(.*@*) CWHOIS (.*)#CS_WHOIS $lt \1!\2 0 \3#gi;
        $ll =~ s#^(.*)!(.*@*) set opflags (.*) (.*) on user (.*)#NS_SETOP $lt \1!\2 0 \5 \3 \4#gi;
        $ll =~ s#^(.*)!(.*@*) FAILED(R): (.*) on user (.*)#OS_R $lt \1!\2 2 \5 \3 \4#gi;

# Joe-N8XCT!anyone@206.230.157.157 set flag(s): +a on heloise

# Mysidia!mysidia@208.231.103.84 set banish on channel #gameserv
# Mysidia!mysidia@208.231.103.84 set banish on channel #infoserv

        printf("%s", $ll);
    }
}
