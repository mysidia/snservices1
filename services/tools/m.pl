#!/usr/pkg/bin/perl
use DBI;
use CGI qw/:standard *table *TR/;
use Sipc;
use strict;

sub error {
    my $code    = shift;
    my $message = shift;

    die "#" . $code . ": " . $message;
}

sub makeforum {
    my $forum       = shift;
    my $description = shift;
    my $skipc = 1;

    my $dbh =
      DBI->connect( 'dbi:Pg:dbname=forums', 'phorum', '-', { AutoCommit => 0 } )
      || error( 10, 'Unable to access database' );
    my $nameCheck =
      $dbh->prepare('SELECT forum_id from phpbb_forums where forum_name=?');
    $nameCheck->execute($forum) or error( 12, 'Error accessing database' );
    while ( $nameCheck->fetch ) {
        error( 100, 'Sorry, forum by that name already exists' );
    }

    my $getId =
      $dbh->prepare('SELECT MAX(forum_id)+1 AS next_id from phpbb_forums')
      || error( 22, 'Error accessing database' );
    $getId->execute or error( 26, 'Error accessing database' );

    my $row =
      $getId->fetchrow_hashref;    # || error(20, 'Error accessing database');
    my $forum_id = $row->{next_id};
    $getId->finish;

    if ( $forum_id < 1 ) { 
	    error( 30, 'Error accessing database' ); 
    }

    print( "Next free Forum_Id in the SQL table is: " . $forum_id . "\n" );

    my $insertfh = $dbh->prepare(
        q|
	INSERT INTO phpbb_forums (forum_id, cat_id, forum_name, forum_desc,
			    forum_status, forum_order, forum_posts, 
			    forum_topics, forum_last_post_id,
                            prune_enable, prune_next, auth_view, auth_read,
                            auth_post, auth_reply, auth_edit, auth_delete,
                            auth_announce, auth_sticky, auth_pollcreate,
                            auth_vote, auth_attachments)
        VALUES(?, 6, ?, ?, 0, 10, 0, 0, 0, 1, NULL, 0, 0, 0, 0, 1, 1, 3, 1, 1,1, 0)
      |
      )
      || error( 40, 'Error accessing database' );	      
      
    if (!$skipc && !$insertfh->execute( $forum_id, $forum, $description )) {
	      $dbh->rollback();
	      error( 50, 'Error adding forum' );
    }

    my $insertph = $dbh->prepare(
        q|
        INSERT INTO phpbb_forum_prune (forum_id, prune_days, prune_freq) VALUES(?, 90, 25)
	|
    );

    if (!$skipc && !$insertph->execute($forum_id))
    { 
	    $dbh->rollback();
	    error( 60, 'Error adding forum' );
    }
    $dbh->commit                  || error( 80, 'Error adding forum' );
    $dbh->disconnect;

    if ($skipc) {
	    print p, "SKIPPED executing the two SQL insert statements, but ".
	            "the forum would have just been created.\n";
    }
    print p, "Forum name: " . $forum . "\n";
    print p, "Description: " . $description . "\n";
}

if (!param()) {
	print header, start_html('Create Forum'),
   	start_form,
   	start_table,
     	start_TR, td(p,"Channel name:", textfield('channel')), end_TR,
     	start_TR, td(p,"Founder password:", textfield('password')), end_TR,
     	start_TR, td(p, submit), end_TR,
   	end_table,
   	end_form, hr, end_html;
}
elsif (param('channel'))
{
	my $h = Sipc->Connect("127.0.0.1", 2050, "mysid/test", "test");
	my $chName = param('channel');
	my $passWord = param('password');
	my $rt;

	if (!$h) {
		print header, start_html("Sorry, unable to login to ChanServ"),
			p, "This interface to services is not available right now.",
			p, "Please try again later.", end_html;
	   die $h->Errmsg;
	}

	if (!Sipc->validChanName($chName)) {
		print header, start_html("Unable to login"),
			p, "Sorry, but ", $chName, " is not a valid channel name.", end_html;
	   die $h->Errmsg;
	}

	if (($rt = $h->queryChan($chName, "TIMEREG")) eq undef) {
		print header, start_html("Unable to login"),
			p, "Sorry, please check that you entered the channel name correctly. ".
			   "As far as this page can tell, it is not registered.", end_html;
                   die $h->Errmsg;
	}

	if (time() < ($rt + 30)) {
		print header, start_html("Channel not ready"),
			p, "Please wait 24 hours after registering the channel before " .
			   "attempting to create a forum.", end_html;
		die $h->Errmsg;

	}

	if (!$h->loginChannel($chName, $h->hashPw($passWord))) {
		print header, start_html("Unable to login"),
			p, "Please check the channel name and password you entered for typos.".
			   " There may be a problem with services, if they are correct, ".
			   " (", $h->Errmsg, ") ", end_html;
		   die $h->Errmsg;
	}           

	print header, start_html("Create Forum");
	makeforum($chName, "Channel " . $chName . " forum.");

	print end_html;
}

