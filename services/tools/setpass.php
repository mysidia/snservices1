<?
#
# Services IPC: set nick password using authorization code
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
	wwwPrintHead("Change Password");
	$u_nick = $HTTP_GET_VARS['u_nick'];
	$u_chan = $HTTP_GET_VARS['u_chan'];
	$u_ck = $HTTP_GET_VARS['u_ck'];

#	echo "<!DOCTYPE>\n";
	echo "<FORM METHOD=\"POST\" ACTION=\"" . $HTTP_SERVER_VARS['PHP_SELF'] . "?u_nick=" . urlencode($u_nick) . "&amp;u_ck=" . urlencode($u_ck) . "&amp;u_chan=" . urlencode($u_chan) . "\">";
	echo "<TABLE FRAME=\"BOX\"><TR>\n";
	echo "<TH><INPUT NAME='whatForm' TYPE=HIDDEN VALUE=1>";
	if (empty($u_chan)) {
		echo "Nickname:</TH><TD> " . htmlspecialchars($u_nick) . "</TD></TR>\n";
	} else {
		echo "Channel:</TH><TD> " . htmlspecialchars($u_chan) . "</TD></TR>\n";
	}
	echo "<TR><TH>Hostname:</TH><TD> " . $HTTP_SERVER_VARS['REMOTE_ADDR'] . "</TD></TR>\n";
	echo "<TR><TH>New password:</TH><TD> <INPUT TYPE=PASSWORD NAME=\"u_pw_a\"></TD></TR>\n";
	echo "<TR><TH>Again:</TH> <TD><INPUT TYPE=PASSWORD NAME=\"u_pw_b\"></TD></TR>\n";
	echo "<TR><TD ALIGN=\"RIGHT\"><INPUT TYPE=SUBMIT VALUE=\"Ok\"></TD>\n";
	echo "<TD ALIGN=\"LEFT\"><INPUT TYPE=RESET VALUE=\"Reset\"></TD></TR>\n</TABLE>\n";
	echo "</FORM>";
	wwwPrintFoot();
	exit;
}

$u_nick = $HTTP_GET_VARS['u_nick'];
$u_chan = $HTTP_GET_VARS['u_chan'];
$u_ck = $HTTP_GET_VARS['u_ck'];
$u_pw_a = $HTTP_POST_VARS['u_pw_a'];
$u_pw_b = $HTTP_POST_VARS['u_pw_b'];

if (empty($u_nick) && empty($u_chan)) {
	wwwPrintHead("Error: No nick/chan specified");
	echo 'The nickname is not valid.';
	wwwPrintFoot();
	exit;
}

if (empty($u_chan)) {
$typ = "RNICK";
$nm = "nick";
}
else {
$typ = "RCHAN";
$nm = "chan";
}

if (empty($u_pw_a)) {
	wwwPrintHead("Error: No password specified");
	echo 'You did not specify a password.';
	wwwPrintFoot();
	exit;
}

if ($u_pw_a != $u_pw_b) {
	wwwPrintHead("Error: Password mismatch");
	echo 'Your two password entries did not match.';
	wwwPrintFoot();
	exit;
}

if (!valid_pw($u_pw_a)) {
	wwwPrintHead("Error: Password not valid");
	echo 'The password you specified was not valid. ';
	echo 'Passwords must be at least 4 characters long, ';
	echo 'should not be guessable, and may not contain spaces.';
	wwwPrintFoot();
	exit;
}

if (empty($u_pw_a)) {
	wwwPrintHead("No password specified");
	echo 'You must specify a password.';
	wwwPrintFoot();
	exit;
}

$fl = ipcOpen("WWW/setpass", "test", $err);
if (!empty($fl)) {
	echo "(Connected successfully.)<br>\n";
#	$x = isNickRegistered($fl, $whatNick, $err);
#	if (empty($x) && !empty($err)) {
#		echo "$err<br>\n";
#		ipcClose($fl);
#		wwwPrintFoot();
#		exit;
#	}
#
	if (empty($u_chan)) {
		$x = ipcObjOpenSp($fl, $typ, $u_nick, $u_ck, $err);
	} else {
		$x = ipcObjOpenSp($fl, $typ, $u_chan, $u_ck, $err);
	}

	if ($x == 1) {
		if (empty($u_chan)) {
			$x = ipcObjSetpass($fl, $typ, $u_nick, $u_pw_a, $err);
		} else {
			$x = ipcObjSetpass($fl, $typ, $u_chan, $u_pw_a, $err);
		}

		if ($x == 0) {
			wwwPrintHead("Password change failed");
			echo "$err<br>\n";
			wwwPrintFoot();
			exit;
		}

		wwwPrintHead("Request submitted");
		echo "Your password change request has been submitted.<br>\n";
		echo "Write down your new password and memorize it.<br>\n";
		wwwPrintFoot();
	}
	else {
		wwwPrintHead("Access Denied");
		echo "$err<br>\n";
		wwwPrintFoot();
	}

	ipcClose($fl);
}
else {
	echo "Unable to connect: " . $err . "<br>\n";
}
?>

