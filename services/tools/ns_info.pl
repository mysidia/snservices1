#
# /ns info test  perl [incomplete]
#
use Sipc;

print "Logging in to services...\n";
$h = Sipc->Connect("127.0.0.1", 2050, "mysid/test", "test") || die 'Unable to connect';
print "\nLogged in.\n";
