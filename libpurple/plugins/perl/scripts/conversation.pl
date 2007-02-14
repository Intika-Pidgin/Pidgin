$MODULE_NAME = "Conversation Test";

use Gaim;

# All the information Gaim gets about our nifty plugin
%PLUGIN_INFO = ( 
	perl_api_version => 2, 
	name => "Perl: $MODULE_NAME", 
	version => "0.1", 
	summary => "Test plugin for the Perl interpreter.", 
	description => "Implements a set of test proccedures to ensure all " .
	               "functions that work in the C API still work in the " .
		       "Perl plugin interface.  As XSUBs are added, this " .
		       "*should* be updated to test the changes.  " .
		       "Furthermore, this will function as the tutorial perl " .
		       "plugin.", 
	author => "John H. Kelm <johnhkelm\@gmail.com>", 
	url => "http://sourceforge.net/users/johnhkelm/", 
	
	load => "plugin_load", 
	unload => "plugin_unload" 
); 


	# These names must already exist
	my $GROUP		= "UIUC Buddies";
	my $USERNAME 		= "johnhkelm2";
	
	# We will create these on load then destroy them on unload
	my $TEST_GROUP		= "UConn Buddies";
	my $TEST_NAME	 	= "johnhkelm";
	my $TEST_ALIAS	 	= "John Kelm";
	my $PROTOCOL_ID 	= "prpl-oscar";


sub plugin_init { 
	return %PLUGIN_INFO; 
} 


# This is the sub defined in %PLUGIN_INFO to be called when the plugin is loaded
#	Note: The plugin has a reference to itself on top of the argument stack.
sub plugin_load { 
	my $plugin = shift; 
	print "#" x 80 . "\n\n";

	print "PERL: Finding account.\n";
	$account = Gaim::Accounts::find($USERNAME, $PROTOCOL_ID);
	
	#########  TEST CODE HERE  ##########
	# First we create two new conversations.
	print "Testing Gaim::Conversation::new()...";
	$conv1 = Gaim::Conversation->new(1, $account, "Test Conversation 1");
	if ($conv1) { print "ok.\n"; } else { print "fail.\n"; }

	print "Testing Gaim::Conversation::new()...";
	$conv2 = Gaim::Conversation->new(1, $account, "Test Conversation 2");
	if ($conv2) { print "ok.\n"; } else { print "fail.\n"; }
	
	# Second we create a window to display the conversations in.
	#  Note that the package here is Gaim::Conversation::Window
	print "Testing Gaim::Conversation::Window::new()...\n";
	$win = Gaim::Conversation::Window::new();

	# The third thing to do is to add the two conversations to the windows.
	# The subroutine add_conversation() returns the number of conversations
	# present in the window.
	print "Testing Gaim::Conversation::Window::add_conversation()...";
	$conv_count = $conv1->add_conversation();
	if ($conv_count) { 
		print "ok..." . $conv_count . " conversations...\n";
	} else {
		print "fail.\n";
	}

	print "Testing Gaim::Conversation::Window::add_conversation()...";
	$conv_count = $win->add_conversation($conv2);
	if ($conv_count) {
		print "ok..." . $conv_count . " conversations...\n";
	} else {
		print "fail.\n";
	}

	# Now the window is displayed to the user.
	print "Testing Gaim::Conversation::Window::show()...\n";
	$win->show();

	# Use get_im_data() to get a handle for the conversation	
	print "Testing Gaim::Conversation::get_im_data()...\n";
	$im = $conv1->get_im_data();
	if ($im) { print "ok.\n"; } else { print "fail.\n"; }

	# Here we send messages to the conversation
	print "Testing Gaim::Conversation::IM::send()...\n";
	$im->send("Message Test.");

	print "Testing Gaim::Conversation::IM::write()...\n";
	$im->write("SENDER", "<b>Message</b> Test.", 0, 0);
	
	print "#" x 80 . "\n\n";
} 

sub plugin_unload { 
	my $plugin = shift; 

	print "#" x 80 . "\n\n";
	#########  TEST CODE HERE  ##########

	print "Testing Gaim::Conversation::Window::get_conversation_count()...\n";
	$conv_count = $win->get_conversation_count();
	print "...and it returned $conv_count.\n";
	if ($conv_count > 0) {
	        print "Testing Gaim::Conversation::Window::destroy()...\n";
	        $win->destroy();
	}
	
	print "\n\n" . "#" x 80 . "\n\n";
}

