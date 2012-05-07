import ctypes as c
from fractions import Fraction

class _RawMediaSession(c.Structure):
    _fields_ = [("framerate_num", c.c_int),
                ("framerate_den", c.c_int),
                ("width", c.c_int),
                ("height", c.c_int),
                ("audio_framebuffer_size", c.c_int)]

class _RawMediaDecoder(c.Structure):
    pass

class _RawMediaDecoderConfig(c.Structure):
    _fields_ = [("start_frame", c.c_int),
                ("volume", c.c_float),
                ("discard_video", c.c_bool),
                ("discard_audio", c.c_bool)]

class _RawMediaDecoderInfo(c.Structure):
    _fields_ = [("duration", c.c_int),
                ("has_video", c.c_bool),
                ("has_audio", c.c_bool)]

class _RawMediaEncoder(c.Structure):
    pass

class _RawMediaEncoderConfig(c.Structure):
    _fields_ = [("has_video", c.c_bool),
                ("has_audio", c.c_bool)]

class RawMediaError(Exception): pass

def _errcheck(result):
    if result < 0:
        raise RawMediaError
    return result

#XXX dylib for mac, so for linux
_librawmedia = c.cdll.LoadLibrary("librawmedia.dylib")

_librawmedia.rawmedia_init_session.argtypes = [c.POINTER(_RawMediaSession)]
_librawmedia.rawmedia_init_session.restype = _errcheck

_librawmedia.rawmedia_create_decoder.argtypes = [c.c_char_p,
                                                 c.POINTER(_RawMediaSession),
                                                 c.POINTER(_RawMediaDecoderConfig)]
_librawmedia.rawmedia_create_decoder.restype = c.POINTER(_RawMediaDecoder)

_librawmedia.rawmedia_get_decoder_info.argtypes = [c.POINTER(_RawMediaDecoder)]
_librawmedia.rawmedia_get_decoder_info.restype = c.POINTER(_RawMediaDecoderInfo)

_librawmedia.rawmedia_decode_video.argtypes = [c.POINTER(_RawMediaDecoder),
                                               c.POINTER(c.POINTER(c.c_uint8)),
                                               c.POINTER(c.c_int)]
_librawmedia.rawmedia_decode_video.restype = _errcheck

_librawmedia.rawmedia_decode_audio.argtypes = [c.POINTER(_RawMediaDecoder),
                                               c.POINTER(c.c_uint8)]
_librawmedia.rawmedia_decode_audio.restype = _errcheck

_librawmedia.rawmedia_destroy_decoder.argtypes = [c.POINTER(_RawMediaDecoder)]
_librawmedia.rawmedia_destroy_decoder.restype = c.c_int

_librawmedia.rawmedia_create_encoder.argtypes = [c.c_char_p,
                                                 c.POINTER(_RawMediaSession),
                                                 c.POINTER(_RawMediaEncoderConfig)]
_librawmedia.rawmedia_create_encoder.restype = c.POINTER(_RawMediaEncoder)

_librawmedia.rawmedia_encode_video.argtypes = [c.POINTER(_RawMediaEncoder),
                                               c.POINTER(c.c_uint8),
                                               c.c_int]
_librawmedia.rawmedia_encode_video.restype = _errcheck

_librawmedia.rawmedia_encode_audio.argtypes = [c.POINTER(_RawMediaEncoder),
                                               c.POINTER(c.c_uint8)]
_librawmedia.rawmedia_encode_audio.restype = _errcheck

_librawmedia.rawmedia_destroy_encoder.argtypes = [c.POINTER(_RawMediaEncoder)]
_librawmedia.rawmedia_destroy_encoder.restype = c.c_int

class Session:
    def __init__(self, width, height, framerate=Fraction(30)):
        """width - video width to decode to and encode from
        height - video height to decode to and encode from
        framerate - Rational specifying framerate to decode to
        """
        self._session = _RawMediaSession(framerate_num=framerate.numerator,
                                         framerate_den=framerate.denominator,
                                         width=width, height=height)
        _librawmedia.rawmedia_init_session(self._session)

    @property
    def audio_framebuffer_size(self):
        return self._session.audio_framebuffer_size

class Decoder:
    def __init__(self, filename, session, volume=1,
                 start_frame=0, discard_video=False, discard_audio=False):
        """filename - File to decode
        session - Transcoding Session
        volume - Volume, 0 is silence, 1 is full (exponential)
        start_frame - Starting frame to decode from
        discard_video - Ignore video stream
        discard_audio - Ignore audio stream
        """
        # Use an exponential curve for volume
        # See http://www.dr-lex.be/info-stuff/volumecontrols.html
        if volume > 0 and volume < 1:
            volume = exp(6.908 * volume) / 1000
            if volume < 0.1:
                volume *= volume * 10

        config = _RawMediaDecoderConfig(start_frame=start_frame,
                                        volume=volume,
                                        discard_video=discard_video,
                                        discard_audio=discard_audio)
        self._decoder = _librawmedia.rawmedia_create_decoder(filename,
                                                             session._session,
                                                             config)
        if not self._decoder:
            raise RawMediaError, "Failed to create Decoder for %s" % filename
        self._info = _librawmedia.rawmedia_get_decoder_info(self._decoder).contents

        self._audio_buffer_type = c.c_uint8 * session.audio_framebuffer_size
        self._buffer = c.POINTER(c.c_uint8)()
        self._linesize = c.c_int(0)

    def create_audio_buffer(self):
        return self._audio_buffer_type()

    def decode_video(self):
        """Returns a ctypes c_uint8 array buffer, valid until the next decode, and linesize"""
        _librawmedia.rawmedia_decode_video(self._decoder, self._buffer,
                                           c.byref(self._linesize))
        return self._buffer, self._linesize.value

    def decode_audio(self, buffer):
        _librawmedia.rawmedia_decode_audio(self._decoder,
                                           c.cast(buffer, c.POINTER(c.c_uint8)))

    @property
    def duration(self):
        return self._info.duration

    def has_video(self):
        return self._info.has_video

    def has_audio(self):
        return self._info.has_audio

    def destroy(self):
        if self._decoder:
            _librawmedia.rawmedia_destroy_decoder(self._decoder)
            self._decoder = None

class Encoder:
    def __init__(self, filename, session, has_video=True, has_audio=True):
        """filename - File to encode into
        session - Transcoding Session
        has_video - True if encoding video
        has_audio - True if encoding audio
        """
        config = _RawMediaEncoderConfig(has_video=has_video,
                                        has_audio=has_audio)
        self._encoder = _librawmedia.rawmedia_create_encoder(filename,
                                                             session._session,
                                                             config)
        if not self._encoder:
            raise RawMediaError, "Failed to create Encoder for %s" % filename

    def encode_video(self, buffer, linesize):
        _librawmedia.rawmedia_encode_video(self._encoder,
                                           c.cast(buffer, c.POINTER(c.c_uint8)),
                                           linesize)

    def encode_audio(self, buffer):
        _librawmedia.rawmedia_encode_audio(self._encoder,
                                           c.cast(buffer, c.POINTER(c.c_uint8)))

    def destroy(self):
        if self._encoder:
            _librawmedia.rawmedia_destroy_encoder(self._encoder)
            self._encoder = None

