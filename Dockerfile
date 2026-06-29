ARG ALPINE_VERSION=3.24.1

FROM alpine:${ALPINE_VERSION} AS build

RUN apk add --no-cache build-base

WORKDIR /src
COPY velserv.c velserv-health.c ./
RUN cc -O2 -Wall -Wextra -o velserv velserv.c -lpthread && \
    cc -O2 -Wall -Wextra -o velserv-health velserv-health.c

FROM alpine:${ALPINE_VERSION}

RUN apk add --no-cache libgcc

COPY --from=build /src/velserv /usr/local/bin/velserv
COPY --from=build /src/velserv-health /usr/local/bin/velserv-health

EXPOSE 3788

ENV VELSERV_DEVICE=/dev/ttyACM0 \
    VELSERV_HEALTH_HOST=127.0.0.1 \
    VELSERV_PORT=3788 \
    VELSERV_HEALTH_TIMEOUT=5

HEALTHCHECK --interval=30s --timeout=10s --start-period=20s --retries=3 \
    CMD velserv-health --device "$VELSERV_DEVICE" --host "$VELSERV_HEALTH_HOST" --port "$VELSERV_PORT" --timeout "$VELSERV_HEALTH_TIMEOUT" --require-frame

ENTRYPOINT ["/usr/local/bin/velserv"]
CMD ["-f", "-v"]
