# Stage 1: Compile inside a Linux environment
FROM ubuntu:22.04 AS builder
RUN apt-get update && apt-get install -y build-essential
COPY . /app
WORKDIR /app
RUN make

# Stage 2: Create the final lean image
FROM ubuntu:22.04
COPY --from=builder /app/collector /usr/local/bin/collector
CMD ["/usr/local/bin/collector"]