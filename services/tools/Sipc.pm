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


sub getPublicNickInfo
{
	my $n = {}, $o;
	my $self = shift;
	my $target = shift;
	
	(($n->{NICK} = $self->queryNick($target, "")) ne undef) or return undef;
	(($n->{REGTIME} = $self->queryNick($target, "TIMEREG")) ne undef) or return undef;
	(($n->{URL} = $self->queryNick($target, "URL")) ne undef) or return undef;
	(($n->{FLAGS} = $self->queryNick($target, "FLAGS")) ne undef) or return undef;
	(($n->{BADPWS} = $self->queryNick($target, "BADPWS")) ne undef) or return undef;
	(($n->{'LAST-TIME'} = $self->queryNick($target, "LAST-TIME")) ne undef) or return undef;
	(($n->{ACC} = $self->queryNick($target, "ACC")) ne undef) or return undef;
	(($n->{MARK} = $self->queryNick($target, "MARK")) ne undef) or return undef;
	(($n->{'LAST-HOST'} = $self->queryNick($target, "LAST-HOST")) ne undef) or return undef;
	return $n;
}

sub getPrivateNickInfo
{
        my $n = {};
        my $self = shift;
        my $target = shift;

	$n->{'LAST-HOST-UNMASK'} = $self->queryNick($target, "LAST-HOST-UNMASK") or return undef;
	$n->{EMAIL} = $self->queryNick($target, "EMAIL") or return undef;
	(($n->{OPFLAGS} = $self->queryNick($target, "OPFLAGS")) ne undef) or return undef;
	(($n->{NUM_MASKS} = $self->queryNick($target, "NUM-MASKS")) ne undef) or return undef;
	$n->{'IS-READTIME'} = $self->queryNick($target, "IS-READTIME") or return undef;

}

sub queryNick
{
	my $self = shift;
	my $qtarget = shift;
	my $qfield = shift;
	my $sock = $self->{sock};
	my $sel = $self->{sel};

	$sock->print("QUERY OBJECT RNICK " . $qtarget . " " . $qfield . "\n");

	for(;;) {
		for ($sel->can_read(5)) {
			if ($_ ne $self->{sock}) {
				$SIPC::ERRORTEXT = "Wrong socket\n";
				return undef;
			}
			while ($line = $sock->getline) {
				#print "\n" . $line . "\n";
				if ($line =~ /^ERR-BADTARGET QUERY OBJECT RNICK=(\S+) - (.*)/)
				{
					$SIPC::ERRORTEXT = "Prob: " . $2 . "\n";
					return undef;
				}
				elsif ($line =~ /^ERR-BADATT QUERY OBJECT RNICK EATTR - (.*)/) 
				{

					$SIPC::ERRORTEXT = "Unknown attribute: RNICK " . $qfield;
					return undef;
				}
				elsif ($line =~ /^OK QUERY OBJECT RNICK=(\S+) ISREG=(\S+) - .*/) {
					my $m_nick = $1;
					my $m_isreg = $2;

					if ($qfield ne "") {
						return $m_isreg;
					}
					else {
						if ($m_isreg eq "TRUE") {
							return $m_nick;
						}
						else {
							return undef;
						}
					}
				}
				elsif ($line =~ /^OK QUERY OBJECT RNICK=(\S+) (\S+) (.*)/)
				{
					if ($1 ne $qtarget) {
						warn "$1 <> $target" . "\n";
					}
					return ($1, $3);
				}
			}
		}
	}
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
				#print $line;

				if (grep(/^AUTH SYSTEM LOGIN irc\/services/, $line)) {
					$sock->print("AUTH SYSTEM LOGIN " . $user . "\n");
				}
				elsif ($line =~ /^AUTH COOKIE ([0-9A-F]+)/) {
					$hashcode = md5_hex($1 . ":" . $password);
					$sock->print("AUTH SYSTEM PASS " . $hashcode . "\n");
				}
				elsif ($line =~ /^ERR-BADLOGIN AUTH SYSTEM LOGIN - (.*)/) {
					$SIPC::ERRORTEXT = 'Login failed :' . $1 . "\n";
					$sel->remove($sock);
					$sock->close;
					return undef;
				}
				elsif ($line =~ /^ERR-\S+\s(\S\s+)+- (.*)/) {
					$SIPC::ERRORTEXT = 'Login failed :' . $2 . "\n";
					$sel->remove($sock);
					$sock->close;
					return undef;
				}
				# $line =~ /^OK AUTH SYSTEM PASS/
				elsif ($line =~ /YOU ARE (\S+)\/(\S+)/) {
                                       while ($line = $sock->getline) {
					}
					$self->{'sock'}=$sock;
					$self->{'sel'}=$sel;
					return (bless $self, $x);
				}
			}
		}
	}

	$sel->remove($sock);
	$sock->close;
	return undef;
}
