Summary: IRC Services software for use with ircd-sor1.3.2+
Name: sorserv
Version: 1.4.2
Release: 1 
Copyright: BSD
Vendor: http://www.sorcery.net
#
Source: ftp://ftp.sorcery.net/pub/services/sorserv-1.4.tar.gz
Group: Applications/Internet/Irc

%description
 This is the IRC services software created for sorcery.net.
It is an addon for sorircd that allows an IRC network to offer
Nick (NickServ, Channel registration (ChanServ), IRC operator
services (OperServ), IRC Network news services (InfoServ), and
(optional) dice-rolling services (GameServ).

%prep
%setup

%build
./configure --with-cfgpath=/usr/lib/sorserv \
            --prefix=$RPM_BUILD_ROOT%{_prefix} \
            --program-suffix='.irc' \
            --sysconfdir=$RPM_BUILD_ROOT%{_prefix}/etc \
            --with-help=/usr/lib/sorserv/help \
            --with-network='IRC' \
            --without-klinemail \
            --with-md5 \
            --with-sendmail='/usr/sbin/sendmail -T' \
            --disable-grpops
make RPM_OPT_FLAGS="$RPM_OPT_FLAGS" 

%install
mkdir -p "$RPM_BUILD_ROOT/usr/lib/sorserv"
#if test "$RPM_BUILD_ROOT" = "/usr/src/rpm/BUILD" ; then
#rm -rf $RPM_BUILD_ROOT
#fi
make install
if test -f services-help.tar.gz ; then
  gunzip < services-help.tar.gz | tar xvf - -C $RPM_BUILD_ROOT/usr/lib/sorserv
fi
mkdir -p -m 700 "$RPM_BUILD_ROOT/usr/lib/sorserv/chanserv"
mkdir -p -m 700 "$RPM_BUILD_ROOT/usr/lib/sorserv/nickserv"
mkdir -p -m 700 "$RPM_BUILD_ROOT/usr/lib/sorserv/memoserv"
mkdir -p -m 700 "$RPM_BUILD_ROOT/usr/lib/sorserv/operserv"
mkdir -p -m 700 "$RPM_BUILD_ROOT/usr/lib/sorserv/helperv"
mkdir -p -m 700 "$RPM_BUILD_ROOT/usr/lib/sorserv/infoserv"

if ! test -f $RPM_BUILD_ROOT/usr/lib/sorserv/chanserv/chanserv.db ; then
   echo 'done' > $RPM_BUILD_ROOT/usr/lib/sorserv/chanserv/chanserv.db
fi

if ! test -f $RPM_BUILD_ROOT/usr/lib/sorserv/nickserv/nickserv.db ; then
   echo 'done' > $RPM_BUILD_ROOT/usr/lib/sorserv/nickserv/nickserv.db
fi

if ! test -f $RPM_BUILD_ROOT/usr/lib/sorserv/infoserv/infoserv.db ; then
   echo 'done' > $RPM_BUILD_ROOT/usr/lib/sorserv/infoserv/infoserv.db
fi

if ! test -f $RPM_BUILD_ROOT/usr/lib/sorserv/memoserv/memoserv.db ; then
   echo 'done' > $RPM_BUILD_ROOT/usr/lib/sorserv/memoserv/memoserv.db
fi

if ! test -f $RPM_BUILD_ROOT/usr/lib/sorserv/operserv/akill.db ; then
   echo 'done' > $RPM_BUILD_ROOT/usr/lib/sorserv/operserv/akill.db
fi

if ! test -f $RPM_BUILD_ROOT/usr/lib/sorserv/operserv/trigger.db ; then
   echo 'done' > $RPM_BUILD_ROOT/usr/lib/sorserv/operserv/trigger.db
fi

if ! test -f $RPM_BUILD_ROOT/usr/lib/sorserv/services.conf ; then
   cp docs/services.conf.example $RPM_BUILD_ROOT/usr/lib/sorserv/services.conf
   chmod 600 $RPM_BUILD_ROOT/usr/lib/sorserv/services.conf
fi

%files
%doc LICENSE README
%doc docs/services.conf.example
%doc docs/services.xml
%doc docs/manual.html docs/intro.html docs/overview.html
%doc docs/copy.html docs/preinstall.html
%doc docs/install.html docs/config.html docs/confman.html docs/confrec.html
%doc docs/confopt.html
%doc docs/adms.html docs/admin2.html docs/fdl.html
%defattr(600,root,root)
%config /usr/lib/sorserv/services.conf
%config /usr/lib/sorserv/*serv/*.db
%defattr(644,root,root)
%dir /usr/lib/sorserv
%defattr(600,root,root)
%dir /usr/lib/sorserv/*serv
%defattr(644,root,root)
/usr/lib/sorserv/help
/usr/bin/services.irc

