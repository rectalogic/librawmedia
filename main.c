#include "transcoder.h"

int main(int argc, const char *argv[]) {
    init_transcoder();
    DecoderContext* ctx = create_decoder_context(argv[1]);
    decode_video_frame(ctx);
    destroy_decoder_context(ctx);
    return 0;
}
