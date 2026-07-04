FROM ubuntu:18.04
USER root

WORKDIR /microbiome

# add build tools
RUN apt-get update && apt-get install -y make g++ pkg-config curl \
    libulfius-dev libmicrohttpd-dev libjansson-dev

# add source code
COPY . .

# build
RUN make

# the console app (mb_app, the default CMD) doesn't listen on a port; only
# the webapp (mb_webapp, see docker-compose.yml's microbiome-webapp service)
# does, on MICROBIOME_WEB_PORT (default 8080).
EXPOSE 8080

# run
CMD ["./mb_app"]