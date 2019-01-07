FROM ubuntu:16.04 AS builder

LABEL MAINTAINER="KML VISION, devops@kmlvision.com"

# install build deps
RUN apt-get update -qq && \
    apt-get -qq -y install \
      build-essential \
      autoconf \
      autoconf-archive \
      automake \
      openssl \
      libssl-dev \
      net-tools \
      nano \
      cmake \
      git \
      libjpeg8-dev \
      libmemcached-dev \
      libopenjpeg-dev \
      libssl-dev \
      libtiff5-dev \
      nginx && \
    apt-get -y build-dep iipimage-server && \
    rm -rf /var/lib/apt/lists/*

WORKDIR /usr/src/iipsrv
# copy the source
COPY ../ .
RUN cp ./src/iipsrv.fcgi /usr/local/bin/iipsrv.fcgi
RUN ldconfig -v

# set nginx config
COPY nginx.conf /etc/nginx/nginx.conf
# copy entrypoint
COPY start_iipsrv.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

WORKDIR /
EXPOSE 80
ENTRYPOINT ["/entrypoint.sh"]