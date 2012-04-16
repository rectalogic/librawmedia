FFMPEG = /Users/aw/Projects/snapfish/encoder/foundation/ffmpeg/installed
PKG_CONFIG_CMD=PKG_CONFIG_PATH=${FFMPEG}/lib/pkgconfig pkg-config
FFMPEG_CFLAGS = $(shell ${PKG_CONFIG_CMD} --cflags libavcodec libavformat libavutil libswscale libswresample)
FFMPEG_LDFLAGS = $(shell ${PKG_CONFIG_CMD} --libs libavcodec libavformat libavutil libswscale libswresample)

CC = gcc
CFLAGS = -Wall -std=c99 -ggdb3 ${FFMPEG_CFLAGS}
LDFLAGS = ${FFMPEG_LDFLAGS}

OBJS = decoder.o main.o packet_queue.o rawmedia.o

rawmedia: ${OBJS}
	${CC} -o rawmedia ${OBJS} ${LDFLAGS}

clean:
	rm -f rawmedia ${OBJS}
