#
# /ns info test  perl [incomplete]
#
use Sipc;

Sipc->Connect("127.0.0.1", 2050, "WWW/test", "test") || die 'Unable to connect';

print "\n" . $errno . "\n";

