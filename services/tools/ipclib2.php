<?

define("NKILL",		0x1);		# Nick protection
define("NVACATION",	0x2);		# Vacation
define("NHOLD", 		0x4);		# No expire
define("NIDENT", 		0x8);		# Ident required
define("NTERSE",		0x10);		# Terse mode
define("NOADDOP",		0x40);		# NOOP
define("NEMAIL",		0x80);		# Hide email
define("NBANISH",		0x100);		# Banished NICKNAME
define("NGRPOP",		0x200);		# NO FUNCTION
define("NBYPASS",		0x400);		# BYPASS+ nick
define("NUSEDPW",		0x800);		# Has ever ided?
define("NDBISMASK",	0x1000);	# Is +m ?
define("NMARK",		0x2000);	# MARKED NICK
define("NDOEDIT",		0x4000);	# SraEdit: deprecated,
					# use /os override
define("NOGETPASS",	0x8000);	# NoGetpass
define("NACTIVE",		0x10000);	# Activated nick (+a)
define("NDEACC",		0x20000);	# Deactivated, newemail (+d)
define("NFORCEXFER",	0x40000);	# Suspended for transfer
define("NENCRYPT",	0x80000);	# Encrypted password
define("NAHURT",		0x100000);	#???

define("OROOT",		0x1);
define("OSERVOP",		0x8);
define("OAKILL",		0x40);
define("OGRP",		0x2000);

class Ipc
{
  var $errstring;
  var $ipcsock;

  function Ipc() {
#  $x=pg_connect("dbname=test");
     $this->ipcsock = NULL;
  }

  function getErr() {
     return $this->errstring;
  }

  function close() {
#echo "-- Socket closed--<br>\n";
     if ($this->ipcsock != NULL) { socket_close($this->ipcsock); }
     $this->ipcsock=NULL;
  }

  function connect($user, $password, $port = 3366) {
  	$sock = socket_create(AF_INET, SOCK_STREAM, SOL_TCP);
	if (!$sock) {
		$this->errstring = "Unable to create socket<br>\n";
		return 0;
	}
	$result = socket_connect($sock, "127.0.0.1", $port);
	$as = 0;

	if (!$result) {
		$this->errstring = "Unable to connect to services ($errno)<br>\n";
		return 0;
	}

#	$result = socket_set_nonblock($sock);

	if (!$result) {
		$this->errstring = "Unable to make socket nonblocking ($errno)";
		socket_close($result);
		return 0;
	}

	while(1) {
		$read = array($sock);
		$n = socket_select($read, $bw=NULL, $bx=NULL, 5);
		if ($n < 1) {
			$this->errstring = "Unable to authenticate.";
			socket_close($sock);
			return 0;
		}
		
		$l = socket_read($sock, 1024, PHP_NORMAL_READ);

		if (!$l) {
		#echo "[ " . socket_strerror(socket_last_error($sock)) . " ]\n";
			$this->errstring = "Unable to authenticate: " .
			socket_strerror(socket_last_error($sock)) . "\n";
			
			socket_close($sock);
			return 0;
		}

		if (strlen($l) < 1) {
			$this->errstring = "Unable to authenticate.";
			socket_close($sock);
			return 0;
		}

		$x = split(' ', trim($l));
		#echo "[" . trim($l) . "]<br>\n";

		if ($x[0] == 'HELO') {
			if (!socket_write($sock, "AUTH SYSTEM LOGIN " . $user .
			"\n"))
			{
				$this->errstring = "Write error";
				return 0;
			}
		}
		if ($x[0] == 'AUTH' && $x[1] == 'COOKIE') {
			$as = 1;
			$mv = md5(trim($x[2]) . ":" . $password);
			if (!socket_write($sock, "AUTH SYSTEM PASS " . $mv .
			"\n")) {
				$this->errstring = "Write error";
				return 0;
			}
		}
		else if ($x[0] == 'ERR-BADLOGIN') {
			$this->errstring = "Incorrect login information.";
			socket_close($sock);
			return 0;
		}
		else if ($x[0] == "OK" && $x[1] == "AUTH" && $x[2] == "SYSTEM"
		         && $x[3] == "PASS") {
			# echo "Authentication successful.\n";
			$kk = socket_read($sock, 1025, PHP_NORMAL_READ);
			
			if ($kk && strlen($kk) < 1) {
				$this->errstring = "Unexpected EOF";
				socket_close($sock);
				return 0;
			}

			break;
		} else if ($x[0] == "OK"
		           || $x[0] == "YOU" || $x[0] == "AUTH" || $x[0] == "HELO") 
		{
		} else if ($x[0]) {
			$this->errstring = "Internnal error\n";
			return 0;
		}
	}

        $this->ipcsock = $sock;
	return 1;
  }

  function getQueryResult() {
    $k = 0;
    $buf = "";
	while(1 && $k < 5) 
	{
		$read = array($this->ipcsock);
		$n = socket_select($read, $bwrite=NULL, $bxcept=NULL, 10);

		if ($n < 0) {
			return NULL;
		}
		else if ($n < 1) {
			continue;
		}
$k++;
		$l = (socket_read($this->ipcsock, 1025, PHP_NORMAL_READ));
		if (!$l) {
			continue;
		#	return NULL;
		}

		if (strlen($l) < 1) {
			$this->close();
			return NULL;
		}

		if (!(strstr($l, "\n"))) {
			$buf = $buf . $l;
			continue;
		}

		$l = $buf . $l;
		$buf = "";

		$x = split(' ', trim($l));
		#$p = ("RNICK=". $nick);

		if ($x[1] != 'QUERY' /*|| $p != $x[3]*/)
			continue;
	
		if ($x[0] != 'OK') {
			echo "ERROR: Unexpected error: " . $l . "<br>\n";
			return $null;
		}

		return $x;

		if (!empty($x[0]))
			break;
	}
	return $null;
  }


  function isRegisteredNick($nick) {
  		if ($this->ipcsock == NULL) {
			echo "ERROR: IPC Not connected.<br>\n";
			return 0;
		}
		if (strchr($nick, " ") || strchr($nick, "\n") || strchr($nick, "\t") || strchr($nick, "\r")) {
			$err = "The nickname you specified is not valid (18 characters or less, should be alphanumeric, shouldn't start with a number, hyphen, or contain any spaces; allowed symbols are: `, [, {, \\, |, }, ], ^, _, and -.).<br>\n";
			return;
		}
	
		if (!socket_write($this->ipcsock, "QUERY OBJECT RNICK " . $nick . " ISREG\n"))
		{
			$this->close();
			$this->errstring = "Write failed : " . socket_last_error($sock) . "\n";
		}

	        $x = $this->getQueryResult();
		if ($x[4] == "ISREG=TRUE") {
			return 1;
		}
		if ($x[4] == "ISREG=FALSE") {
			return 0;
		}
		return -1;
	}

	function postNickQuery($nick, $field) {

	  	if ($this->ipcsock == NULL) {
			echo "ERROR: IPC Not connected.<br>\n";
			return 0;
		}

		if (strchr($nick, " ") || strchr($nick, "\n") || strchr($nick, "\t") || strchr($nick, "\r")) {
			$err = "The nickname you specified is not valid (18 characters or less, should be alphanumeric, shouldn't start with a number, hyphen, or contain any spaces; allowed symbols are: `, [, {, \\, |, }, ], ^, _, and -.).<br>\n";
			return 0;
		}
	
		if (!socket_write($this->ipcsock, "QUERY OBJECT RNICK " . $nick . " " . $field . "\n"))
		{
			$this->close();
			$this->errstring = "Write failed : " . socket_last_error($sock) . "\n";
			return 0;
		}
		return 1;
	}

	function getLastSeenTimeFormatted($nick) {
		#%a %Y-%b-%d %T %Z
		$ts = $this->getLastSeenTime($nick);
		if ($ts == NULL) {
			return NULL;
		}
		return gmdate("D y-M-d H:i:s T", $ts);
	}
	
	function getTimeFormatted($timeval) {
		return gmdate("D y-M-d H:i:s T", $timeval);
	}

	function getLastSeenTime($nick) {
		if ($this->postNickQuery($nick, "LAST-TIME")) {
			if (($str = $this->getQueryResult())) {
				return $str[5];
			}
		}
		return NULL;
	}
	
	function getNickRegTime($nick) {
		if ($this->postNickQuery($nick, "REG-TIME")) {
			if (($str = $this->getQueryResult())) {
				return $this->getTimeFormatted($str[5]);
			}
		}
		return NULL;
	}

	function getLastSeenHost($nick) {
		if ($this->postNickQuery($nick, "LAST-HOST")) {
			if (($str = $this->getQueryResult())) {
				return $str[5];
			}
		}
		return NULL;

	#QUERY OBJECT RNICK nick LAST-HOST
	}
	
	function getNickUrl($nick) {
		if ($this->postNickQuery($nick, "URL")) {
			$str = $this->getQueryResult();
			if ($str != NULL) {
				return $str[5];
			}
		}
		return NULL;
	}
	
	function getNickEmail($nick) {
		if ($this->postNickQuery($nick, "EMAIL")) {
			$str = $this->getQueryResult();
			#echo "::: $str<br>\n";
			if ($str != NULL) {
				return $str[5];
			}
		}
		return NULL;
	}

	function getNickFlags($nick) {
		if ($this->postNickQuery($nick, "FLAGS")) {
			$str = $this->getQueryResult();
			if ($str != NULL) {
				return $str[5];
			}
		}
	}

	function isNickHeld($nick) {
		$str = getNickFlags($nick);
		
		if ($str & NHOLD)
			return 1;
		return 0;
	}

	function getNickOpFlags($nick) {
		if ($this->postNickQuery($nick, "OPFLAGS")) {
			$str = $this->getQueryResult();
			if ($str != NULL) {
				return $str[5];
			}
		}
	}
	
	function getNickAcc($nick) {
		if ($this->postNickQuery($nick, "ACC")) {
			$str = $this->getQueryResult();
			if ($str != NULL) {
				return $str[5];
			}
		}
		return NULL;
	}

	function urlOk($str) {
		if (strlen($str) < 1 || strlen($str) > 512)
			return 0;
		if (substr($str, 10) == "javascript") {
			return 0;
		}
		if (substr($str, 3) == "res:") {
			return 0;
		}

		if (substr($str, 7) == "mailto:") {
			return 1;
		}

		if (!preg_match("/^((.*?):\/\/)?(([^:]*):([^@]*)@)?([^\/:]*)(:([^\/]*))?([^\?]*\/?)?(\?(.*))?$/",$str,$Components)) {			
			return 0;
		}
		list(,,$Protocol,,$Username,$Password,$Host,,$Port,$Path,,$Query) = $Components;

		if (!empty($Password))
			return 0;

		if ($Protocol != "http" && $Protocol != "ftp")
			return 0;
		return 1;
	}

#
#QUERY OBJECT RNICK nick LAST-TIME

#	return -1;	
#  }
}

function valid_pw($pas)
{
	if (strchr($pas, " ") || strchr($pas, "\n") || strchr($pas, "\t")
	    || strchr($pas, "\r") || strlen($pas) < 4 || strlen($pas) > 15)
		return 0;
	return 1;
}


function ipcObjOpen($fp, $type, $user, $password, &$err)
{
	fputs($fp, "AUTH OBJECT LOGIN " . $type . " " . $user . "\n");
	while(1) {
		$l = fgets($fp, 1025);
		if (feof($fp)) {
			$err = "Unable to authenticate.";
			return 0;
		}

		if (empty($l)) {
			sleep(1);
			continue;
		}

		$x = split(' ', trim($l));
		#echo "[" . trim($l) . "]<br>\n";

		if ($x[0] == 'AUTH' && $x[1] == 'COOKIE') {
			$as = 1;
			$mv = md5(trim($x[2]) . ":" . md5($password));
			fputs($fp, "AUTH OBJECT PASS " . $mv . "\n");
		}
		else if ($x[0] == 'ERR-BADLOGIN') {
			$err = "Invalid nickname or password change code.";
			return 0;
		}
		else if ($x[0] == "OK" && $x[1] == "AUTH" && $x[2] == "OBJECT"
		         && $x[3] == "PASS") {
			# echo "Authentication successful.\n";
			fgets($fp, 1025);

			break;
		} else if ($x[0] == "OK" || $x[0] == "YOU") {
		} else if ($x[0]) {
			$err = "Internal error\n";
			return 0;
		}
	}

	return 1;
}

function ipcObjOpenSp($fp, $type, $user, $password, &$err)
{
$z=0;
	fputs($fp, "AUTH OBJECT LOGIN " . $type . " " . $user . "\n");
	sleep(1);
	while(1) {
		$l = fgets($fp, 1025);
		if (feof($fp)) {
			$err = "Unable to authenticate to object.";
			return 0;
		}

		if (empty($l)) {
$z++;
if ($z > 800) {exit;}
			continue;
		}

#echo "[$l]\n";
		$x = split(' ', trim($l));
		#echo "[" . trim($l) . "]<br>\n";

		if ($x[0] == 'AUTH' && $x[1] == 'COOKIE') {
			$as = 1;
			$mv = md5(trim($x[2]) . ":" . md5($password));
			fputs($fp, "AUTH OBJECT CHPASSKEY " . $password . "\n");
			sleep(1);
		}
		else if ($x[0] == 'ERR-BADLOGIN') {
			$err = "Incorrect login information.";
			return 0;
		}
		else if ($x[0] == "OK" && $x[1] == "AUTH" && $x[2] == "OBJECT"
		         && ($x[4] == "SETPASS" || $x[3] == "SETPASS")) {
			# echo "Authentication successful.\n";
			fgets($fp, 1025);

			return 1;
		} else if ($x[0] == "OK" || $x[0] == "YOU") {
		} else if ($x[0]) {
			$err = "Internal error\n";
			return 0;
		}
	}
	return 1;
}

function isNickRegistered($ipc, $nick, &$err)
{
	if (strchr($nick, " ") || strchr($nick, "\n") || strchr($nick, "\t") || strchr($nick, "\r")) {
		$err = "The nickname you specified is not valid (18 characters or less, should be alphanumeric, shouldn't start with a number, hyphen, or contain any spaces; allowed symbols are: `, [, {, \\, |, }, ], ^, _, and -.).<br>\n";
		return;
	}
	fputs($ipc, "QUERY OBJECT RNICK " . $nick . " ISREG\n");

	while(1) 
	{
		$l = trim(fgets($ipc, 1025));
		if (feof($ipc)) {
			return $null;
		}

		if (empty($l)) {
			continue;
		}

		$x = split(' ', trim($l));
		$p = ("RNICK=". $nick);


		if ($x[1] != 'QUERY' || $p != $x[3])
			continue;
	
		if ($x[0] != 'OK') {
			echo "ERROR: Unexpected error: " . $l . "<br>\n";
			return;
		}

		if ($x[4] == "ISREG=TRUE") {
			return 1;
		}

		if ($x[4] == "ISREG=FALSE") {
			return 0;
		}

		if (!empty($x[0]))
			break;
	}

	return $fp;	
}


function registerNick($ipc, $nick, $email, $host, &$err)
{
	if (strchr($nick, " ") || strchr($nick, "\n") || strchr($nick, "\t") || strchr($nick, "\r")) {
		$err = "The nickname you specified is not valid (18 characters or less, should be alphanumeric, shouldn't start with a number, hyphen, or contain any spaces; allowed symbols are: `, [, {, \\, |, }, ], ^, _, and -.).<br>\n";
		return;
	}
	if (strchr($email, " ") || strchr($email, "\n") || strchr($email, "\t") || strchr($nick, "\r")
	    || !strchr($email, "@") || !strchr($email, "."))
	 {
		$err = "The e-mail address specified is not valid.  Should be of the form user@host.com.";
		return;
	}
	if (strchr($host, " ") || strchr($host, "\n") || strchr($host, "\t") || strchr($host, "\r")) {
		$err = "Invalid nickname.";
		return;
	}

	fputs($ipc, "MAKE RNICK " . $nick . " " . $email . " " . $host . "\n");

	while(1) 
	{
		$l = trim(fgets($ipc, 1025));
		if (feof($ipc)) {
			return $null;
		}

		if (empty($l)) {
			continue;
		}

		$x = split(' ', trim($l));
		$p = ("RNICK=". $nick);


		if ($x[1] != 'MAKE' || $p != $x[2])
			continue;
	
		if ($x[0] != 'OK') {
			if ($x[0] == 'ERR-NICKQLINE') {
				$err = "Nickname not allowed (Reserved nickname).<br>\n";
			}
			else if ($x[0] == 'ERR-NICKINUSE') {
				$err = "Another user is holding that nickname.<br>\n";
			}
			else if ($x[0] == 'ERR-HOSTBANNED') {
				$err = "Your site is not allowed.<br>\n";
			}
			else if ($x[0] == 'ERR-TRYAGAINLATER') {
				$err = "Try again later.<br>\n";
			}
			else if ($x[0] == 'ERR-EMAILBANNED') {
				$err = "Your e-mail address is not allowed.<br>\n";
			}
			else if ($x[0] == 'ERR-NICKINVALID') {
				$err = "The nickname you specified is not valid (18 characters or less, should be alphanumeric, shouldn't start with a number, hyphen, or contain any spaces; allowed symbols are: `, [, {, \\, |, }, ], ^, _, and -.).<br>\n";
			}
			else {
				$err = "ERROR: Unexpected error: " . $l . "<br>\n";
			}
			return;
		}

		if ($x[0] == 'OK') {
			return substr($x[3], 5);
		}

		if (!empty($x[0]))
			break;
	}

	return $fp;	
}

function ipcClose($fp)
{
	fclose($fp);
}

function ipcObjSetpass($fp, $type, $user, $pass, &$err)
{
#ALTER OBJECT RCHAN #testb PASS asdf
	fputs($fp, "ALTER OBJECT " . $type . " " . $user . " PASS " . $pass . "\n");
####
	sleep(1);
	while(1) {
		$l = fgets($fp, 1025);
		if (feof($fp)) {
			$err = "Unable to set password.";
			return 0;
		}

		if (empty($l)) {
			continue;
		}

		$x = split(' ', trim($l));
		#echo "[" . trim($l) . "]<br>\n";

		if ($x[0] == "OK" && $x[1] == "ALTER" && $x[2] == "OBJECT") {
			# echo "Authentication successful.\n";
			return 1;
		} else if ($x[0] == "OK" || $x[0] == "YOU") {
		} else if ($x[0]) {
			$err = "Internal error\n";
			return 0;
		}
	}
}

function wwwPrintHead($tit)
{
	echo '<!DOCTYPE html PUBLIC "-//W3C//DTD HTML 4.01 Transitional//EN">';
	echo "\n<HTML><HEAD>\n";
	echo "<TITLE>" . htmlspecialchars($tit) . "</TITLE>\n";
	echo "</HEAD><BODY BGCOLOR='ffffff'>";
}

function wwwPrintFoot()
{
	echo "</BODY></HTML>\n";
}


?>
