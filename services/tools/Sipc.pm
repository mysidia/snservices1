#
# Services IPC Perl Interface: [INCOMPLETE]
#
package Sipc;
require 5.004;
use Carp 'croak';
use IO::Socket::INET;
use IO::Select;
use Digest::MD5 q/md5_hex/;

$SIPC::revision = '$Id$';
$SIPC::VERSION='0.1';
$SIPC::ERRORTEXT=" ";

sub Errmsg
{
	return $SIPC::ERRORTEXT;
}

sub Connect 
{
	my ($x, $host, $port, $user, $password) = @_;
	my $self = {};
	my $sock;

	$SIPC::ERRORTEXT = "";
	$sock = IO::Socket::INET->new(PeerAddr => $host,
                                      PeerPort => $port,
			              Proto => 'tcp') || return undef;
	$sock->blocking(0);
        $sel = new IO::Select;
        $sel->add($sock);

	for(;;) {
		for ($sel->can_read(5)) {
			if ($_ ne $sock) {
				$sel->remove($sock);
				$sock->close;
				return undef;
			}
			while ($line = $sock->getline) {
				print $line;

				if (grep(/AUTH SYSTEM LOGIN irc\/services/, $line)) {
					$sock->print("AUTH SYSTEM LOGIN " . $user . "\n");
				}
				elsif ($line =~ /AUTH COOKIE ([0-9A-F]+)/) {
					$hashcode = md5_hex($1 . ":" . $password);
					$sock->print("AUTH SYSTEM PASS " . $hashcode . "\n");
				}
				elsif ($line =~ /ERR-BADLOGIN AUTH SYSTEM LOGIN - (.*)/) {
					$SIPC::ERRORTEXT = 'Login failed :' . $1 . "\n";
					$sel->remove($sock);
					$sock->close;
					return undeF;
				}
				elsif ($line =~ /OK AUTH SYSTEM PASS/) {
					return (bless $self, $x);
				}
			}
		}
	}

	$sel->remove($sock);
	$sock->close;
	return undef;
}
