#
# /cs identify test   perl [INCOMPLETE]
#
use Sipc;

print "Logging in to services...\n";
$h = Sipc->Connect("127.0.0.1", 2050, "mysid/test", "test") || die 'Unable to connect';

print "Enter a chan to authenticate to: ";
my $chan = <>;
chomp($chan);

print "Enter a password: ";
my $pass = <>;
chomp($pass);

if (!($x = $h->loginChannel($chan, $h->hashPw($pass)))) {
   die "Error: " . $h->Errmsg() . "\n";
}

print "Logged into " . $x->{CHAN} . " : \n";


for (keys %$x) {
print $x->{$_} . "\n";
}
