# Sipc.pm
#
# Services IPC Perl Interface: [INCOMPLETE]
#
# Copyright (c) 2004 James Hess
# 
# All rights reserved.
# 
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
# 3. Neither the name of the authors nor the names of its contributors
#    may be used to endorse or promote products derived from this software
#    without specific prior written permission.
# 
# THIS SOFTWARE IS PROVIDED BY THE AUTHORS AND CONTRIBUTORS ``AS IS'' AND
# ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHORS OR CONTRIBUTORS BE LIABLE
# FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
# DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
# OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
# LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
# OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
# SUCH DAMAGE.
# 

package Sipc;
require 5.004;
use Carp 'croak';
use IO::Socket::INET;
use IO::Select;
use Digest::MD5 q/md5_hex/;

my %nick_flags = (
	'NKILL'         => 0x1       ,  # Nick protection
	'NVACATION'     => 0x2       ,  # Vacation
	'NHOLD'         => 0x4       ,  # No expire
	'NIDENT'        => 0x8       ,  # Ident required
	'NTERSE'        => 0x10      ,  # Terse mode
	'NOADDOP'       => 0x40      ,  # NOOP
	'NEMAIL'        => 0x80      ,  # Hide email
	'NBANISH'       => 0x100     ,  # Banished NICKNAME
	'NGRPOP'        => 0x200     ,  # NO FUNCTION
	'NBYPASS'       => 0x400     ,  # BYPASS+ nick
	'NUSEDPW'       => 0x800     ,  # Has ever ided?
	'NDBISMASK'     => 0x1000    ,  # Is +m ?
	'NMARK'         => 0x2000    ,  # MARKED NICK
	'NDOEDIT'       => 0x4000    ,  # SraEdit: deprecated, 
                                        # use /os override
	'NOGETPASS'     => 0x8000    ,  # NoGetpass
	'NACTIVE'       => 0x10000   ,  # Activated nick (+a)
	'NDEACC'        => 0x20000   ,  # Deactivated, newemail (+d)
	'NFORCEXFER'    => 0x40000   ,  # Suspended for transfer
	'NENCRYPT'      => 0x80000   ,  # Encrypted password
	'NAHURT'        => 0x100000  ,  #???
);

my %oper_flags = (
	'OROOT'         => 0x000001  , # Services Root DONT CHANGE
	'OSERVOP'       => 0x000008  , # is servop (deprecated)
	'OOPER'		=> 0x000010  , # basic oper privs
	'ORAKILL'	=> 0x000020  , # restricted akill priv
	'OAKILL'        => 0x000040  , # full akill priv
	'OINFOPOST'	=> 0x000080  , # high-priority info post
	'OSETOP'	=> 0x000100  , # can use setop/add/del
	'OSETFLAG'	=> 0x000200  , # can use stflag
	'ONBANDEL'	=> 0x000400  , # can banish/del nicks
	'OCBANDEL'	=> 0x000800  , # can banish/del chans
	'OIGNORE'	=> 0x001000  , # can set services ignores
	'OGRP'          => 0x002000  , # can getpass -transfer
	'OLIST'		=> 0x010000  , # can use /cs list & cs whois
	'OCLONE'	=> 0x020000  , # can edit clonerules
	'OPROT'		=> 0x080000  , # record locked
	'OACC'		=> 0x100000  , # override user restrictions
	'OHELPOP'	=> 0x200000  , # user is a helpop
	'ODMOD'		=> 0x400000  , # direct modification
	'OAHURT'	=> 0x800000    # access to AutoHurt commands
);

$Sipc::revision = '$Id$';
$Sipc::VERSION='0.1';
my $ERRORTEXT="_";

sub validNickName
{
	my $x = shift;
	my $name = shift;

	if (index($name, "\n") > -1 || index($name, "\r") > -1)  {
		return 0;
	}

	if ($name =~ /^[0-9]/ || $name =~ / /) {
		return 0;
	}

	if ($name =~ /^[0-9a-zA-Z\[\\\]\^_`\{|\}~]+/) {
		return 1;
	}
	return 0;
}

sub validChanName
{
	my $x = shift;
	my $name = shift;

	if (index($name, "\n") > -1 || index($name, "\r") > -1) {
		return 0;
	}

	if ($name =~ /[ ,\33\t\003\017\026\037\002\001\b]/) {
		return 0;
	}

	if ($name =~ /^[+#&]/) {
		return 1;
	}

	return 0;
}

sub Errmsg
{
	my $self = shift;

	return $self->{ERRORTEXT};
}

sub getPublicChanInfo
{
	my $n = {}, $o;
	my $self = shift;
	my $target = shift;

	(($n->{CHAN} = $self->queryChan($target, "")) ne undef) or return undef;
	(($n->{TIMEREG} = $self->queryChan($target, "TIMEREG")) ne undef) or return undef;
	(($n->{DESC} = $self->queryChan($target, "DESC")) ne undef) or return undef;
	return $n;
}

sub getPublicNickInfo
{
	my $n = {}, $o;
	my $self = shift;
	my $target = shift;
	
	(($n->{NICK} = $self->queryNick($target, "")) ne undef) or return undef;
	(($n->{TIMEREG} = $self->queryNick($target, "TIMEREG")) ne undef) or return undef;
	(($n->{URL} = $self->queryNick($target, "URL")));
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

sub hashPw
{
	my $x = shift;
	my $p = shift;

	return md5_hex($p);
}

sub loginChannel
{
	my $self = shift;
	my $chan = shift;
	my $passw = shift;

	return $self->loginObject('RCHAN', 'PASS', $chan, $passw);
}

sub loginNick
{
	my $self = shift;
	my $nick = shift;
	my $passw = shift;

	return $self->loginObject('RNICK', 'PASS', $nick, $passw);
}

sub loginObject
{
	my $self = shift;
	my $objtype = shift;
	my $logintype = shift;
	my $objname = shift;
	my $passw = shift;
	my $sel = $self->{sel};
	my $sock = $self->{sock};

	# $passw = md5_hex($passw);
	$sock->print("AUTH OBJECT LOGIN " . $objtype . " " . $objname. "\n");

	for(;;) {
		for ($sel->can_read(5)) {
			if ($_ ne $self->{sock}) {
				$self->{ERRORTEXT} = "Wrong socket";
				return undef;
			}
			while ($line = $sock->getline) {
#print $line . "\n";
				if ($line =~ /^OK AUTH OBJECT (\S+) LOGIN/) {
				}
				elsif ($line =~ /^AUTH COOKIE (\S+)/) {
					if ($logintype eq "PASS") {
						$hashcode = md5_hex($1 . ":" . $passw);
						$sock->print("AUTH OBJECT " . $logintype . " " . $hashcode . "\n");
					}
					elsif ($logintype eq "CHPASSKEY") {
						$sock->print("AUTH OBJECT " . $logintype . " " . $passw . "\n");
					}
				}
				elsif ($line =~ /^ERR-BADLOGIN /) {
					$self->{ERRORTEXT} = "Invalid login details";
					return undef;
				}
				      # OK AUTH OBJECT _ PASS
				elsif (($line =~ /^OK AUTH OBJECT (\S+) (\S+)/ && ($2 eq  $logintype)) ||
			                $line =~ /OK AUTH OBJECT (\S+) SETPASS/) {
					return 1;
				}
				elsif ($line =~ /^ERR-(\S+\s)+- (.*)/) {
					$self->{ERRORTEXT} = "Error: $2";
					return undef;
				}
				elsif ($line =~ /\s*\S+/) {
					$self->{ERRORTEXT} = "Invalid login reply";
					return undef;
				}
			}
		}
	}
	return undef;
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
				$self->{ERRORTEXT} = "Wrong socket\n";
				return undef;
			}
			while ($line = $sock->getline) {
				#print "\n" . $line . "\n";
				if ($line =~ /^ERR-BADTARGET QUERY OBJECT RNICK=(\S+) - (.*)/)
				{
					$self->{ERRORTEXT} = "Prob: " . $2 . "\n";
					return undef;
				}
				elsif ($line =~ /^ERR-BADATT QUERY OBJECT RNICK EATTR - (.*)/) 
				{

					$self->{ERRORTEXT} = "Unknown attribute: RNICK " . $qfield;
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
							$self->{ERRORTEXT} = $m_nick . ": Nickname not registered";
							return undef;
						}
					}
				}
				elsif ($line =~ /^OK QUERY OBJECT RNICK=(\S+) (\S+) (.*)/)
				{
					if ($1 ne $qtarget) {
						warn "$1 <> $target" . "\n";
					}
					return $3;
				}
			}
		}
	}
}


sub queryChan
{
	my $self = shift;
	my $qtarget = shift;
	my $qfield = shift;
	my $sock = $self->{sock};
	my $sel = $self->{sel};

	$sock->print("QUERY OBJECT RCHAN " . $qtarget . " " . $qfield . "\n");

	for(;;) {
		for ($sel->can_read(5)) {
			if ($_ ne $self->{sock}) {
				$self->{ERRORTEXT} = "Wrong socket\n";
				return undef;
			}
			while ($line = $sock->getline) {
				#$$$
				#print "\n" . $line . "\n";
				if ($line =~ /^ERR-BADTARGET QUERY OBJECT RCHAN=(\S+) - (.*)/)
				{
					$self->{ERRORTEXT} = "Prob: " . $2 . "\n";
					return undef;
				}
				elsif ($line =~ /^ERR-BADATT QUERY OBJECT RCHAN EATTR - (.*)/) 
				{

					$self->{ERRORTEXT} = "Unknown attribute: RCHAN " . $qfield;
					return undef;
				}
				elsif ($line =~ /^OK QUERY OBJECT RCHAN=(\S+) ISREG=(\S+) - .*/) {
					my $m_chan = $1;
					my $m_isreg = $2;

					if ($qfield ne "") {
						return $m_isreg;
					}
					else {
						if ($m_isreg eq "TRUE") {
							return $m_chan;
						}
						else {
							$self->{ERRORTEXT} = $m_chan . ": Channel not registered.";
							return undef;
						}
					}
				}
				elsif ($line =~ /^OK QUERY OBJECT RCHAN=(\S+) (\S+) (.*)/)
				{
					if ($1 ne $qtarget) {
						warn "$1 <> $target" . "\n";
					}
					return $3;
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

	$ERRORTEXT = "";
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
					$ERRORTEXT = 'Login failed :' . $1 . "\n";
					$sel->remove($sock);
					$sock->close;
					return undef;
				}
				elsif ($line =~ /^ERR-\S+\s(\S\s+)+- (.*)/) {
					$ERRORTEXT = 'Login failed :' . $2 . "\n";
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

sub Close
{
	my $self = shift;
	my $sock = $self->{sock};
	my $sel = $self->{sel};

	$self->remove($sock);
	$sock->close;
}


__END__

=pod

=head1 NAME

Sipc - Services Inter-Process Communication Interface Module

=head1 STATUS

Warning: IPC is still an experimental interface, and a work in progress.

=head1 SYNOPSIS

use Sipc;

=head1

This module provides an interface for communicating directly with an IRC Services process
using the IPC interface and a TCP socket.

=head1 FUNCTIONS

=over 4

=item validNickName(nick)

   Checks if a nickname is legitimate for most IRC networks.   The length is not
   checked, only the set of characters contained.

   Example:  if (validNickName("Guest-1234") == 1) { print "Valid nick!\n"; }

=item validChankName(nick)

   Checks if a channel name is legitimate for most IRC networks.   The length is not
   checked, only the set of characters contained.

   Example:  if (validChanName("#") == 1) { print "Valid chan!\n"; }

=item hashPw(password)

   Converts a plaintext password to a hash suitable for use witht the Login
   method.

   Example:

   my $hashpw = Sipc->hashPw('foopass');

=back
 

=head1 CONSTRUCTION

=over 4

=item Connect(ip_address, port_number, login_info, password)

 For example:
   my $handle = Sipc->Connect('127.0.0.1', 3166, 'WWW/blah', 'xyzpass') || die 'Connection failed';

=back

  Methods:

=over 4

=item Errmsg
  If a Connection, login, or query operation fails, this method returns text providing some
  indication of the cause of the error.

=item getPublicNickInfo(nick_name)
  Queries a nickname for all public information available.

  Returns a hash ref of the contents, or 'undef' if an error occured.

  Example:

     if ($x = $handle->getPublicNickInfo('Joe')) {
     	print "Joe's current access level is " . $x->{ACC} . " and his registration time was " 
                 . $x->{TIMEREG} . "\n";
     }

=item loginNickl(nickl, hashed_password)

=item loginChannel(channel, hashed_password)

     Attempts to login to the specified channel or nick using a hashed version of the 
     owner password (see hashPw).

     Logging into a channel or nickname can be used to authenticate the owner using
     a script outside of services.

     If the script's login has the PRIV_NOWNER_EQUIV for a nickname login or a 
     PRIV_COWNER_EQUIV permission flag for a channel login, then authenticating
     to the nickname grants the script the 'PRIV_OWNER' permission to that object
     for the rest of the IPC session, or until another login command is issued.

     It returns 1 if the login was successful, or undef otherwise.

     Example:
     if () {
     }

=item loginObject(obj_type, auth_type, obj_name, authenticator)

     Attempts to login to the specified object, using the specified authenticator
     type, and hash code.

     Returns 1 if the login was successful or undef otherwise.

     WARNING: this function is under construction, and behavior is undefined for
     auth types other than PASS and object types other than RNICK and RCHAN

     Example:

     $handle->loginObject('RNICK', 'PASS', 'fooNick', Sipc->hashPw('barPass'));

    is the same as
     $handle->loginNick('fooNick', Sipc->hashPw('barPass'));
    
     Example: 
     $handle->loginObject('RNICK', 'CHPASSKEY', '#foo', 'asdfxyz');

=item queryNick(target, field)

=item queryChan(target, field)

     Queries the given nickname or channel (respectively) called 'target' for the value
     of the proprerty 'field'.      

     Returns the information if available; otherwise 'undef' if there was an error 
     looking up the information.
     
=item Close

     Closes the IPC connection.


=cut
