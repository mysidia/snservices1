<?
#
# Services IPC Library test: preliminary
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
include 'ipclib.php';

$whatForm = $HTTP_POST_VARS['whatForm'];

if (empty($whatForm)) {
	wwwPrintHead("Test");
	echo "<FORM METHOD=POST ACTION=" . $HTTP_SERVER_VARS['PHP_SELF'] . ">";
	echo "<INPUT NAME='whatForm' TYPE=HIDDEN VALUE=1>";
	echo "Nickname: <INPUT TYPE=TEXT NAME=whatNick><br>\n";
	echo "E-mail  : <INPUT TYPE=TEXT NAME=whatEmail><br>\n";
	echo "Hostname: " . $HTTP_SERVER_VARS['REMOTE_ADDR'] . "<br>\n";
	echo "<INPUT TYPE=SUBMIT VALUE=Ok>\n";
	echo "<INPUT TYPE=RESET VALUE=Reset>\n";
	echo "</FORM>";
	wwwPrintFoot();
	exit;
}

$whatNick = $HTTP_POST_VARS['whatNick'];
$whatEmail = $HTTP_POST_VARS['whatEmail'];

if (empty($whatNick)) {
	wwwPrintHead("Error: No nick specified");
	echo 'You must specify a nickname.';
	wwwPrintFoot();
	exit;
}

if (empty($whatEmail)) {
	wwwPrintHead("Error: No e-mail specified");
	echo 'You must specify an e-mail address.';
	wwwPrintFoot();
	exit;
}

$fl = ipcOpen("mysid/test", "test", $err);
if (!empty($fl)) {
	echo "(Connected successfully.)<br>\n";
	$x = isNickRegistered($fl, $whatNick, $err);
	if (empty($x) && !empty($err)) {
		echo "$err<br>\n";
		ipcClose($fl);
		wwwPrintFoot();
		exit;
	}

	if ($x != 1) {
		echo "$whatNick is not registered, attempting to register...<br>\n";
		$y = registerNick($fl, $whatNick, $whatEmail, $HTTP_SERVER_VARS['REMOTE_ADDR'], $err);
		if (!empty($y)) {
			echo "$whatNick registered (pass = $y)<br>\n";
		}
		else
		{
			echo "$err<br>\n";
			ipcClose($fl);
			wwwPrintFoot();
			exit;
		}
	}
	else {
		echo "$whatNick is already registered.<br>\n";
	}

	ipcClose($fl);
}
else {
	echo "Unable to connect: " . $err . "<br>\n";
}
?>

