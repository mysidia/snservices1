#
# /ns info test  perl [incomplete]
#
use Sipc;

print "Logging in to services...\n";
$h = Sipc->Connect("127.0.0.1", 2050, "mysid/test", "test") || die 'Unable to connect';

print "Enter a nick to look up: ";
my $nick = <>;
chomp($nick); 

if (!($x = $h->getPublicNickInfo($nick))) {
   die "Error: " . $h->Errmsg() . "\n";
}

print "Information for " . $x->{NICK} . " : \n";


for (keys %$x) {
print $x->{$_} . "\n";
}
