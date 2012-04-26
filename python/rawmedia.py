import ctypes as c
from fractions import Fraction

class _RawMediaDecoder(c.Structure):
    pass

class _RawMediaDecoderConfig(c.Structure):
    _fields_ = [("framerate_num", c.c_int),
                ("framerate_den", c.c_int),
                ("start_frame", c.c_int),
                ("discard_video", c.c_bool),
                ("discard_audio", c.c_bool)]

class _RawMediaDecoderInfo(c.Structure):
    _fields_ = [("duration", c.c_int),
                ("has_video", c.c_bool),
                ("video_framebuffer_size", c.c_int),
                ("width", c.c_int),
                ("height", c.c_int),
                ("has_audio", c.c_bool),
                ("audio_framebuffer_size", c.c_int)]

class _RawMediaEncoder(c.Structure):
    pass

class _RawMediaEncoderConfig(c.Structure):
    _fields_ = [("framerate_num", c.c_int),
                ("framerate_den", c.c_int),
                ("width", c.c_int),
                ("height", c.c_int),
                ("has_video", c.c_bool),
                ("has_audio", c.c_bool)]

class _RawMediaEncoderInfo(c.Structure):
    _fields_ = [("video_framebuffer_size", c.c_int),
                ("audio_framebuffer_size", c.c_int)]

class RawMediaError(Exception): pass

def _errcheck(result):
    if result < 0:
        raise RawMediaError
    return result

#XXX dylib for mac, so for linux
_librawmedia = c.cdll.LoadLibrary("librawmedia.dylib")
_librawmedia.rawmedia_init.restype = _errcheck
_librawmedia.rawmedia_create_decoder.argtypes = [c.c_char_p, c.POINTER(_RawMediaDecoderConfig)]
_librawmedia.rawmedia_create_decoder.restype = c.POINTER(_RawMediaDecoder)
_librawmedia.rawmedia_get_decoder_info.argtypes = [c.POINTER(_RawMediaDecoder)]
_librawmedia.rawmedia_get_decoder_info.restype = c.POINTER(_RawMediaDecoderInfo)
_librawmedia.rawmedia_decode_video.argtypes = [c.POINTER(_RawMediaDecoder), c.POINTER(c.c_uint8)]
_librawmedia.rawmedia_decode_video.restype = _errcheck
_librawmedia.rawmedia_decode_audio.argtypes = [c.POINTER(_RawMediaDecoder), c.POINTER(c.c_uint8)]
_librawmedia.rawmedia_decode_audio.restype = _errcheck
_librawmedia.rawmedia_destroy_decoder.argtypes = [c.POINTER(_RawMediaDecoder)]
_librawmedia.rawmedia_destroy_decoder.restype = None

_librawmedia.rawmedia_create_encoder.argtypes = [c.c_char_p, c.POINTER(_RawMediaEncoderConfig)]
_librawmedia.rawmedia_create_encoder.restype = c.POINTER(_RawMediaEncoder)
_librawmedia.rawmedia_get_encoder_info.argtypes = [c.POINTER(_RawMediaEncoder)]
_librawmedia.rawmedia_get_encoder_info.restype = c.POINTER(_RawMediaEncoderInfo)
_librawmedia.rawmedia_encode_video.argtypes = [c.POINTER(_RawMediaEncoder), c.POINTER(c.c_uint8)]
_librawmedia.rawmedia_encode_video.restype = _errcheck
_librawmedia.rawmedia_encode_audio.argtypes = [c.POINTER(_RawMediaEncoder), c.POINTER(c.c_uint8)]
_librawmedia.rawmedia_encode_audio.restype = _errcheck
_librawmedia.rawmedia_destroy_encoder.argtypes = [c.POINTER(_RawMediaEncoder)]
_librawmedia.rawmedia_destroy_encoder.restype = None

_librawmedia.rawmedia_init()


class Decoder:
    def __init__(self, filename, framerate=Fraction(30),
                 start_frame=0, discard_video=False, discard_audio=False):
        """filename - File to decode
        framerate - Rational specifying framerate to decode to
        start_frame - Starting frame to decode from
        discard_video - Ignore video stream
        discard_audio - Ignore audio stream
        """
        config = _RawMediaDecoderConfig(framerate_num=framerate.numerator,
                                        framerate_den=framerate.denominator,
                                        start_frame=start_frame,
                                        discard_video=discard_video,
                                        discard_audio=discard_audio)
        self._decoder = _librawmedia.rawmedia_create_decoder(filename, config)
        if not self._decoder:
            raise RawMediaError, "Failed to create Decoder for %s" % filename
        self._info = _librawmedia.rawmedia_get_decoder_info(self._decoder).contents

    def create_video_buffer(self):
        return c.cast(c.create_string_buffer(self._info.video_framebuffer_size),
                      c.POINTER(c.c_uint8))

    def create_audio_buffer(self):
        return c.cast(c.create_string_buffer(self._info.audio_framebuffer_size),
                      c.POINTER(c.c_uint8))

    def decode_video(self, buffer):
        _librawmedia.rawmedia_decode_video(self._decoder, buffer)

    def decode_audio(self, buffer):
        _librawmedia.rawmedia_decode_audio(self._decoder, buffer)

    @property
    def duration(self):
        return self._info.duration
#XXX need to expose width/height and whether has audio/video

    def destroy(self):
        if self._decoder:
            _librawmedia.rawmedia_destroy_decoder(self._decoder)
            self._decoder = None

class Encoder:
    def __init__(self, filename, width, height, framerate=Fraction(30),
                 has_video=True, has_audio=True):
        """filename - File to encode into
        width - Width of video
        height - Height of video
        framerate - Rational specifying framerate to encode at
        has_video - True if encoding video
        has_audio - True if encoding audio
        """
        config = _RawMediaEncoderConfig(framerate_num=framerate.numerator,
                                        framerate_den=framerate.denominator,
                                        width=width,
                                        height=height,
                                        has_video=has_video,
                                        has_audio=has_audio)
        self._encoder = _librawmedia.rawmedia_create_encoder(filename, config)
        if not self._encoder:
            raise RawMediaError, "Failed to create Encoder for %s" % filename
        self._info = _librawmedia.rawmedia_get_encoder_info(self._encoder).contents

    def create_video_buffer(self):
        return c.cast(c.create_string_buffer(self._info.video_framebuffer_size),
                      c.POINTER(c.c_uint8))

    def create_audio_buffer(self):
        return c.cast(c.create_string_buffer(self._info.audio_framebuffer_size),
                      c.POINTER(c.c_uint8))

    def encode_video(self, buffer):
        _librawmedia.rawmedia_encode_video(self._encoder, buffer)

    def encode_audio(self, buffer):
        _librawmedia.rawmedia_encode_audio(self._encoder, buffer)

    def destroy(self):
        if self._encoder:
            _librawmedia.rawmedia_destroy_encoder(self._encoder)
            self._encoder = None

