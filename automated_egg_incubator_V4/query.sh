#!/bin/bash

curl -G 'http://192.168.1.20:8086/query?pretty=true' --data-urlencode "db=couveuse" --data-urlencode "max-row-limit=1" --data-urlencode "q=SELECT \"value\" FROM \"temp\" WHERE \"couv\"='couv1' ORDER BY DESC LIMIT 1;SELECT \"value\" FROM \"hum\" WHERE \"couv\"='couv1' ORDER BY DESC LIMIT 1;SELECT \"value\" FROM \"left\" WHERE \"couv\"='couv1' ORDER BY DESC LIMIT 1" > .last

exit $?
