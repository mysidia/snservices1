#
# Services IPC Perl Interface: [INCOMPLETE]
#
package Sipc;
require 5.004;
use Carp 'croak';
use IO::Socket::INET;
use IO::Select;

$SIPC::revision = '$Id$';
$SIPC::VERSION='0.1';

sub Connect 
{
	my ($x, $host, $port, $user, $password) = @_;
	my $self = {};
	my $sock;


	$sock = IO::Socket::INET->new(PeerAddr => $host,
                                      PeerPort => $port,
			              Proto => 'tcp') || return undef;
        $sel = new IO::Select;
        $sel->add($sock);

	for(;;) {
		for ($sel->can_read()) {
			$line = $sock->getline;
			print $line . "\n";
		}
	}
	
	bless $self, $x;
#	return $self;
}
