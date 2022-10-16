FROM ubuntu:18.04
USER root

WORKDIR /microbiome

# add build tools
RUN apt-get update && apt-get install -y make g++

# add source code
COPY . .

# build
RUN make

# run
CMD ["./mb_app"]