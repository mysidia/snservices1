<?
#
# Services IPC Library: preliminary
#
# Copyright C 2002 James Hess, All Rights Reserved.
# 
#   This is free software; you can redistribute it and/or
#   modify it under the terms of the GNU Lesser General Public
#   License as published by the Free Software Foundation;
#   version 2 of the License.
# 
#   This software is distributed in the hope that it will be useful,
#   but WITHOUT ANY WARRANTY; without even the implied warranty of
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#   Lesser General Public License for more details.
# 
#   You should have received a copy of the GNU Lesser General Public
#   License along with this software; if not, write to the Free Software
#   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
#
#

function valid_pw($pas)
{
	if (strchr($pas, " ") || strchr($pas, "\n") || strchr($pas, "\t")
	    || strchr($pas, "\r") || strlen($pas) < 4 || strlen($pas) > 15)
		return 0;
	return 1;
}

function ipcOpen($user, $password, &$err)
{
	$fp = fsockopen("127.0.0.1", 2050, $errno, $errstr, 30);
	$as = 0;

	if (!$fp) {
		$err = "$errstr ($errno)<br>\n";
		return;
	}

	socket_set_blocking($fp, 0);

	while(1) {
		$l = fgets($fp, 1025);
		if (feof($fp)) {
			$err = "Unable to authenticate.";
			fclose($fp);
			return $null;
		}

		if (empty($l)) {
			continue;
		}

		$x = split(' ', trim($l));
		#echo "[" . trim($l) . "]<br>\n";

		if ($x[0] == 'HELO') {
			fputs($fp, "AUTH SYSTEM LOGIN " . $user . "\n");
		}
		if ($x[0] == 'AUTH' && $x[1] == 'COOKIE') {
			$as = 1;
			$mv = md5(trim($x[2]) . ":" . $password);
			fputs($fp, "AUTH SYSTEM PASS " . $mv . "\n");
		}
		else if ($x[0] == 'ERR-BADLOGIN') {
			$err = "Incorrect login information.";
			ipcClose($fp);
			return;
		}
		else if ($x[0] == "OK" && $x[1] == "AUTH" && $x[2] == "SYSTEM"
		         && $x[3] == "PASS") {
			# echo "Authentication successful.\n";
			fgets($fp, 1025);

			break;
		} else if ($x[0] == "OK" || $x[0] == "YOU" || $x[0] == "AUTH" || $x[0] == "HELO") {
		} else if ($x[0]) {
			$err = "Internnal error\n";
			return;
		}
	}

	return $fp;
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
			$err = "Unable to authenticate.";
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
