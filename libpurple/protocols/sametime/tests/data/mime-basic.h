{
/* Just an empty string round-trip. */
.name = "empty",
.html = "",
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
	"\r\n"
	"\r\n"
	"--related_MWa2f_0aac--\r\n"
},
{
/* A simple string. */
.name = "simple",
.html = "This is a test of the MIME encoding using a short message.",
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
	"This is a test of the MIME encoding using a short message.\r\n"
	"\r\n"
	"--related_MWa2f_0aac--\r\n"
},
