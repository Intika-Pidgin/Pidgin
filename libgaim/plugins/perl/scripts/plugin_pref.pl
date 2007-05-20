$MODULE_NAME = "Prefs Functions Test";
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
	unload => "plugin_unload", 
	prefs_info => "foo"
); 

	# These names must already exist
	my $GROUP		= "UIUC Buddies";
	my $USERNAME 		= "johnhkelm2";
	
	# We will create these on load then destroy them on unload
	my $TEST_GROUP		= "perlTestGroup";
	my $TEST_NAME	 	= "perlTestName";
	my $TEST_ALIAS	 	= "perlTestAlias";
	my $PROTOCOL_ID 	= "prpl-oscar";

sub foo {
	$frame = Gaim::PluginPref::Frame->new();

	$ppref = Gaim::PluginPref->new_with_label("boolean");
	$frame->add($ppref);
	
	$ppref = Gaim::PluginPref->new_with_name_and_label(
	    "/plugins/core/perl_test/bool", "Boolean Preference");
	$frame->add($ppref);

		
	$ppref = Gaim::PluginPref->new_with_name_and_label(
	    "/plugins/core/perl_test/choice", "Choice Preference");
	$ppref->set_type(1);
	$ppref->add_choice("ch0", $frame);
	$ppref->add_choice("ch1", $frame);
	$frame->add($ppref);
	
	$ppref = Gaim::PluginPref->new_with_name_and_label(
	    "/plugins/core/perl_test/text", "Text Box Preference");
	$ppref->set_max_length(16);
	$frame->add($ppref);
	
	return $frame;
}

sub plugin_init { 
	
	return %PLUGIN_INFO; 
} 


# This is the sub defined in %PLUGIN_INFO to be called when the plugin is loaded
#	Note: The plugin has a reference to itself on top of the argument stack.
sub plugin_load { 
	my $plugin = shift; 
	print "#" x 80 . "\n\n";


	#########  TEST CODE HERE  ##########

	Gaim::Prefs::add_none("/plugins/core/perl_test");
	Gaim::Prefs::add_bool("/plugins/core/perl_test/bool", 1);	
	Gaim::Prefs::add_string("/plugins/core/perl_test/choice", "ch1");	
	Gaim::Prefs::add_string("/plugins/core/perl_test/text", "Foobar");	
	

	print "\n\n" . "#" x 80 . "\n\n";
} 

sub plugin_unload { 
	my $plugin = shift; 

	print "#" x 80 . "\n\n";


	#########  TEST CODE HERE  ##########


	print "\n\n" . "#" x 80 . "\n\n";
}

