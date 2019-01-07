#!/bin/sh

echo "starting iip server on 127.0.0.1:9000"
iipsrv.fcgi --bind 127.0.0.1:9000 &

nginx -g "daemon off;"