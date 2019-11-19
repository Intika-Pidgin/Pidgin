{
/* A message with an image in it. */
.name = "image",
.html =
	"This is a test of the MIME encoding with an image: "
	"<img src=\"" PURPLE_IMAGE_STORE_PROTOCOL "%u\">",
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
	"This is a test of the MIME encoding with an image: "
		"<img src=\"cid:97c@4aa2fmeanwhile\">\r\n"
	"--related_MWc7f_0aac\r\n"
	"Content-Type: image/png; "
		"name=c6e846b12072e3bdf2c8f768e776deaa94c970ad.png\r\n"
	"Content-Disposition: attachment;\r\n"
	"\tfilename=c6e846b12072e3bdf2c8f768e776deaa94c970ad.png\r\n"
	"Content-Transfer-Encoding: base64\r\n"
	"Content-Id: <97c@4aa2fmeanwhile>\r\n"
	"\r\n"
	"iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAACXBIWXMAAAsTAAALEwEAmpwYAAAA\r\n"
	"B3RJTUUH4AoCFjAiKKTJ3QAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aXRoIEdJTVBkLmUH\r\n"
	"AAAAFklEQVQI12P4//8/AwMD47NMtZtOCwAvqQYvitHGswAAAABJRU5ErkJggg==\r\n"
	"\r\n"
	"--related_MWc7f_0aac--\r\n",
.with_image = TRUE
},
