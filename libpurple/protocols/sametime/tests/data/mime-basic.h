{
/* Just an empty string round-trip. */
.name = "empty",
.html = "",
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
	"\r\n"
	"--related_MWc7f_0aac--\r\n"
},
{
/* A simple string. */
.name = "simple",
.html = "This is a test of the MIME encoding using a short message.",
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
	"This is a test of the MIME encoding using a short message.\r\n"
	"--related_MWc7f_0aac--\r\n"
},
