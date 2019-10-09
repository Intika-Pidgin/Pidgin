{
/* A message with multiple lines. */
.name = "multiline",
.html =
	"This is a test of the MIME encoding using a multiline message.\n"
	"This is the second line of the message.",
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
	"This is a test of the MIME encoding using a multiline message.\r\n"
	"This is the second line of the message.\r\n"
	"--related_MWc7f_0aac--\r\n"
},
{
/* A message that ends with a newline. */
.name = "trailing-newline",
.html = "This is a test of the MIME encoding using a multiline message.\n",
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
	"This is a test of the MIME encoding using a multiline message.\r\n"
	"\r\n"
	"--related_MWc7f_0aac--\r\n"
},
