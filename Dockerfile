FROM ubuntu:16.04 AS build-preparation

# install base packages, supervisor, cron, logrotate
RUN apt-get -y update && apt-get install -y build-essential wget

# Default variables. To override these variables you'll need to run conrainer with `-e VARIABLE=VALUE`
ENV DEBIAN_FRONTEND=noninteractive \
    GOREPLACE_VERSION=${GOREPLACE_VERSION:-"1.1.2"}

## Install goreplace
RUN wget -O go-replace https://github.com/webdevops/go-replace/releases/download/${GOREPLACE_VERSION}/gr-64-linux \
&& chmod +x go-replace

COPY ./ /
RUN make

FROM ubuntu:16.04 AS build-env
LABEL maintainer="Dmitry Teikovtsev <teikovtsev.dmitry@pdffiller.team>"

ENV UDP_REPEATER_DEBUG_LEVEL=1 \
    UDP_REPEATER_BIND_ADDR=127.0.0.1 \
    UDP_REPEATER_BIND_PORT=30000 \
    UDP_REPEATER_DESTINATION_SERVER=udp.server.example.com \
    UDP_REPEATER_DESTINATION_PORT=30002 \
    UDP_REPEATER_MAX_TTL=60

RUN apt-get -y update && apt-get install -y supervisor iputils-ping dnsutils

# Get artifacts from build phase and configs
COPY --from=build-preparation udprepeater /usr/local/bin/udprepeater
COPY configs/udprepeater.conf.docker /etc/udprepeater.conf
COPY --from=build-preparation go-replace /usr/local/bin/go-replace

# start script
COPY configs/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh
COPY configs/entrypoint.d/* /entrypoint.d/
COPY configs/supervisor/ /etc/supervisor/

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/usr/bin/supervisord", "-n"]
