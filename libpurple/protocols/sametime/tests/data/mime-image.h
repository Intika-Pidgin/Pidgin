{
/* A message with an image in it. */
.name = "image",
.html =
	"This is a test of the MIME encoding with an image: "
	"<img src=\"" PURPLE_IMAGE_STORE_PROTOCOL "%u\">",
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
	"This is a test of the MIME encoding with an image: "
		"<img src=\"cid:cc0@6a675meanwhile\">\r\n"
	"\r\n"
	"--related_MWa2f_0aac\r\n"
	"content-transfer-encoding: base64\r\n"
	"content-disposition: attachment; "
		"filename=\"c6e846b12072e3bdf2c8f768e776deaa94c970ad.png\"\r\n"
	"content-id: <cc0@6a675meanwhile>\r\n"
	"content-type: image/png; "
		"name=\"c6e846b12072e3bdf2c8f768e776deaa94c970ad.png\"\r\n"
	"\r\n"
	"iVBORw0KGgoAAAANSUhEUgAAAAIAAAACCAIAAAD91JpzAAAACXBIWXMAAAsTAAALEwEAm"
	"pwYAAAAB3RJTUUH4AoCFjAiKKTJ3QAAAB1pVFh0Q29tbWVudAAAAAAAQ3JlYXRlZCB3aX"
	"RoIEdJTVBkLmUHAAAAFklEQVQI12P4//8/AwMD47NMtZtOCwAvqQYvitHGswAAAABJRU5"
	"ErkJggg==\r\n"
	"\r\n"
	"--related_MWa2f_0aac--\r\n",
.with_image = TRUE
},
