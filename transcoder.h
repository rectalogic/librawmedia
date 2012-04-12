#ifndef TRANSCODER_H
#define TRANSCODER_H

typedef struct DecoderContext DecoderContext;

void init_transcoder();
DecoderContext* create_decoder_context(const char* filename);
void destroy_decoder_context(DecoderContext* dc);
int decode_video_frame(DecoderContext* dc);

#endif
