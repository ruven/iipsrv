# IIPImage Dockerfile based on Alpine Linux (stable)
#
# iipsrv: build for development version on Github:
# - compiles and installs iipsrv and lighttpd
# - starts HTTP and FCGI services
# - reads images from /images/
# - provides /fcgi-bin/iipsrv.fcgi and /iiif/ endpoints
#

FROM alpine:latest

LABEL org.opencontainers.image.authors="ruven@users.sourceforge.net"
LABEL url "https://iipimage.sourceforge.io"
LABEL author "Ruven <ruven@users.sourceforge.net>"
EXPOSE 80 9000

# Install build dependencies
RUN apk add --no-cache \
	git \
	autoconf \
	automake \
	libtool \
	g++ \
	make \
	fcgi-dev \
	tiff-dev \
	libjpeg-turbo-dev \
	zlib-dev \
	openjpeg-dev \
	libpng-dev \
	libwebp-dev \
	libavif-dev \
	libmemcached-dev \
	lighttpd

# Get latest source code
WORKDIR /usr/local/src/
RUN git clone https://github.com/ruven/iipsrv.git

# Configure and compile
WORKDIR /usr/local/src/iipsrv/
RUN ./autogen.sh && \
    ./configure && \
    make && \
    cp src/iipsrv.fcgi /usr/local/bin/iipsrv

# Append lighttpd configuration to enable FCGI proxy forwarding to iipsrv
RUN echo -e 'server.modules += ("mod_fastcgi")\n\
fastcgi.server = (\n\
  "/fcgi-bin/iipsrv.fcgi" => (("host"=>"0.0.0.0","port"=>9000,"check-local"=>"disable")),\n\
  "/iiif/" => (("host"=>"0.0.0.0","port"=>9000,"check-local"=>"disable"))\n\
)' >> /etc/lighttpd/lighttpd.conf

# Create startup script which starts both lighttpd and iipsrv
RUN echo -e "#!/bin/sh\n\
lighttpd -f /etc/lighttpd/lighttpd.conf \n\
iipsrv --bind \${HOST}:\${PORT}" > /usr/local/bin/run &&\
chmod u+x /usr/local/bin/run

# Set iipsrv environment defaults and execute startup script
ENV HOST="0.0.0.0" PORT="9000" LOGFILE="/dev/stdout" URI_MAP="iiif=>IIIF" VERBOSITY=6 FILESYSTEM_PREFIX="/images/"
ENTRYPOINT run
