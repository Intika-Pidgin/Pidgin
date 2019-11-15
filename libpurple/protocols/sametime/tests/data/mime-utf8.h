{
/* Hello world in UTF-8. */
.name = "utf8-hello",
.html = "Hello world, Καλημέρα κόσμε, コンニチハ.",
.mime =
	"Content-Type: multipart/related; boundary=related_MWc7f_0aac\r\n"
	"Mime-Version: 1.0\r\n"
	"Content-Disposition: inline\r\n"
	"\r\n"
	"--related_MWc7f_0aac\r\n"
	"Content-Type: text/html; charset=us-ascii\r\n"
	"Content-Disposition: inline\r\n"
	"Content-Transfer-Encoding: 7bit\r\n"
	"\r\n"
	"Hello world, "
		"&#922;&#945;&#955;&#951;&#956;&#8051;&#961;&#945; "
		"&#954;&#8057;&#963;&#956;&#949;, "
		"&#12467;&#12531;&#12491;&#12481;&#12495;.\r\n"
	"--related_MWc7f_0aac--\r\n"
},
{
/* Hello world in UTF-8. */
.name = "utf8-various",
.html =
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ /0123456789 "
	"abcdefghijklmnopqrstuvwxyz £©µÀÆÖÞßéöÿ "
	"–—‘“”„†•…‰™œŠŸž€ ΑΒΓΔΩαβγδω АБВГДабвгд "
	"∀∂∈ℝ∧∪≡∞ ↑↗↨↻⇣ ┐┼╔╘░►☺♀ ﬁ�⑀₂ἠḂӥẄɐː⍎אԱა",
.mime =
	"Content-Type: multipart/related; boundary=related_MWc7f_0aac\r\n"
	"Mime-Version: 1.0\r\n"
	"Content-Disposition: inline\r\n"
	"\r\n"
	"--related_MWc7f_0aac\r\n"
	"Content-Type: text/html; charset=us-ascii\r\n"
	"Content-Disposition: inline\r\n"
	"Content-Transfer-Encoding: 7bit\r\n"
	"\r\n"
	"ABCDEFGHIJKLMNOPQRSTUVWXYZ /0123456789 abcdefghijklmnopqrstuvwxyz "
	"&#163;&#169;&#181;&#192;&#198;&#214;&#222;&#223;&#233;&#246;&#255; "
	"&#8211;&#8212;&#8216;&#8220;&#8221;&#8222;&#8224;&#8226;&#8230;&#8240;&#8482;&#339;&#352;&#376;&#382;&#8364; "
	"&#913;&#914;&#915;&#916;&#937;&#945;&#946;&#947;&#948;&#969; "
	"&#1040;&#1041;&#1042;&#1043;&#1044;&#1072;&#1073;&#1074;&#1075;&#1076; "
	"&#8704;&#8706;&#8712;&#8477;&#8743;&#8746;&#8801;&#8734; "
	"&#8593;&#8599;&#8616;&#8635;&#8675; "
	"&#9488;&#9532;&#9556;&#9560;&#9617;&#9658;&#9786;&#9792; "
	"&#64257;&#65533;&#9280;&#8322;&#7968;&#7682;&#1253;&#7812;&#592;&#720;&#9038;&#1488;&#1329;&#4304;\r\n"
	"--related_MWc7f_0aac--\r\n"
},
