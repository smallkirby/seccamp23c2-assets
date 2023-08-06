FROM alpine:latest

RUN apk add gcc musl-dev linux-headers liburing-dev libcap-dev
