#
# /cs info test  perl [incomplete]
#
use Sipc;

print "Logging in to services...\n";
$h = Sipc->Connect("127.0.0.1", 2050, "mysid/test", "test") || die 'Unable to connect';

print "Enter a chan to look up: ";
my $chan = <>;
chomp($chan);

if (!($x = $h->getPublicChanInfo($chan))) {
   die "Error: " . $h->Errmsg() . "\n";
}

print "Information for " . $x->{CHAN} . " : \n";


for (keys %$x) {
print $x->{$_} . "\n";
}
