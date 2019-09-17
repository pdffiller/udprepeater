FROM ubuntu:16.04 AS build-preparation

# install base packages, supervisor, cron, logrotate
RUN apt-get -y update && apt-get install -y build-essential

COPY ./ /
RUN make

FROM ubuntu:16.04 AS build-env
LABEL maintainer="Dmitry Teikovtsev <teikovtsev.dmitry@pdffiller.team>"

ENV DEBIAN_FRONTEND=noninteractive \
    GOREPLACE_VERSION=${GOREPLACE_VERSION:-"1.1.2"} \
    UDP_REPEATER_DEBUG_LEVEL=1 \
    UDP_REPEATER_BIND_ADDR=127.0.0.1 \
    UDP_REPEATER_BIND_PORT=30000 \
    UDP_REPEATER_DESTINATION_SERVER=udp.server.example.com \
    UDP_REPEATER_DESTINATION_PORT=30002 \
    UDP_REPEATER_MAX_TTL=60

RUN apt-get -y update && apt-get install -y supervisor iputils-ping dnsutils \
		      && apt-get install -y build-essential wget

RUN wget --quiet -O /usr/local/bin/go-replace https://github.com/webdevops/go-replace/releases/download/${GOREPLACE_VERSION}/gr-64-linux \
&& chmod +x /usr/local/bin/go-replace

# Get artifacts from build phase and configs
COPY --from=build-preparation udprepeater /usr/local/bin/udprepeater
COPY configs/udprepeater.conf.docker /etc/udprepeater.conf

# start script
COPY configs/entrypoint.sh /entrypoint.sh
COPY configs/entrypoint.d /entrypoint.d
COPY configs/supervisor/ /etc/supervisor/
RUN chmod +x /entrypoint.sh

ENTRYPOINT ["/entrypoint.sh"]
CMD ["/usr/bin/supervisord", "-n"]
