###############################################################################
# Tschenggins Lämpli sample config
###############################################################################

# set to the SSID of the wireless network
# e.g. CONFIG_STASSID = "myssid"
CONFIG_STASSID = ""

# set to the password of the wireless network
# e.g. CONFIG_STAPASS = "t0ps3cr3t"
CONFIG_STAPASS = ""

# set to the backend URL where your tschenggins-status.pl is living
# the general format is: "http://user:pass@server.com:port/path/", where
# "http" could be "https", "user:pass@" and ":port" are optional
# e.g. CONFIG_BACKENDURL = "https://oinkzwurgl.org/tschenggins-laempli/"
CONFIG_BACKENDURL = ""

# the server certificate file if the backend lives on a https server,
# not required and used if using unencrypted http, to create:
# $ openssl s_client -showcerts -servername oinkzwurgl.org -connect oinkzwurgl.org:443 < /dev/null \
#   | openssl x509 -outform pem > server.crt"
# e.g. CONFIG_CRTFILE = "server.crt"
CONFIG_CRTFILE = ""

# eof
