{
/* A message with multiple lines. */
.name = "multiline",
.html =
	"This is a test of the MIME encoding using a multiline message.\n"
	"This is the second line of the message.",
.mime =
	"content-disposition: inline\r\n"
	"mime-version: 1.0\r\n"
	"content-type: multipart/related; boundary=related_MWa2f_0aac\r\n"
	"\r\n"
	"--related_MWa2f_0aac\r\n"
	"content-transfer-encoding: 7bit\r\n"
	"content-disposition: inline\r\n"
	"content-type: text/html\r\n"
	"\r\n"
	"This is a test of the MIME encoding using a multiline message.\n"
	"This is the second line of the message.\r\n"
	"\r\n"
	"--related_MWa2f_0aac--\r\n"
},
{
/* A message that ends with a newline. */
.name = "trailing-newline",
.html = "This is a test of the MIME encoding using a multiline message.\n",
.mime =
	"content-disposition: inline\r\n"
	"mime-version: 1.0\r\n"
	"content-type: multipart/related; boundary=related_MWa2f_0aac\r\n"
	"\r\n"
	"--related_MWa2f_0aac\r\n"
	"content-transfer-encoding: 7bit\r\n"
	"content-disposition: inline\r\n"
	"content-type: text/html\r\n"
	"\r\n"
	"This is a test of the MIME encoding using a multiline message.\n"
	"\r\n"
	"\r\n"
	"--related_MWa2f_0aac--\r\n"
},
