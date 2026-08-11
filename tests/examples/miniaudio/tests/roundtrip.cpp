// compat.miniaudio — encode a WAV, decode it back, and assert the samples
// survive the round trip.
//
// Deliberately DEVICE-FREE. A CI runner has no sound card, so ma_device_init
// would fail for reasons that say nothing about the package. The encoder,
// decoder and the WAV codec behind them are the parts that can be asserted
// anywhere, and they still prove the implementation TU (miniaudio.c) is
// compiled and linked -- none of these symbols exist without it.
#include <miniaudio.h>
import std;

int main() {
    bool ok = true;
    auto check = [&](bool cond, std::string_view what) {
        if (!cond) {
            std::println("FAIL: {}", what);
            ok = false;
        }
    };

    constexpr ma_uint32 RATE     = 48000;
    constexpr ma_uint32 CHANNELS = 1;
    constexpr ma_uint64 FRAMES   = 4800;      // 100 ms
    const std::string   path     = "miniaudio_roundtrip.wav";

    // A 480 Hz sine: 10 whole cycles in 4800 frames, so the signal is
    // continuous and its extremes are hit exactly.
    std::vector<float> written(FRAMES);
    for (ma_uint64 i = 0; i < FRAMES; ++i) {
        written[i] = 0.5f * std::sin(2.0f * std::numbers::pi_v<float> *
                                     480.0f * static_cast<float>(i) / RATE);
    }

    // ---- encode ----------------------------------------------------------
    {
        ma_encoder_config cfg =
            ma_encoder_config_init(ma_encoding_format_wav, ma_format_f32, CHANNELS, RATE);
        ma_encoder enc;
        check(ma_encoder_init_file(path.c_str(), &cfg, &enc) == MA_SUCCESS,
              "encoder opened the output file");

        ma_uint64 put = 0;
        check(ma_encoder_write_pcm_frames(&enc, written.data(), FRAMES, &put) == MA_SUCCESS,
              "encoder accepted the frames");
        check(put == FRAMES, "encoder wrote every frame");
        ma_encoder_uninit(&enc);
    }

    // ---- decode ----------------------------------------------------------
    {
        ma_decoder_config cfg = ma_decoder_config_init(ma_format_f32, CHANNELS, RATE);
        ma_decoder dec;
        check(ma_decoder_init_file(path.c_str(), &cfg, &dec) == MA_SUCCESS,
              "decoder opened the file just written");

        check(dec.outputSampleRate == RATE, "decoded sample rate matches");
        check(dec.outputChannels == CHANNELS, "decoded channel count matches");

        ma_uint64 total = 0;
        check(ma_decoder_get_length_in_pcm_frames(&dec, &total) == MA_SUCCESS,
              "decoder reported a length");
        check(total == FRAMES, "decoded length matches what was encoded");

        std::vector<float> read(FRAMES, 0.0f);
        ma_uint64 got = 0;
        check(ma_decoder_read_pcm_frames(&dec, read.data(), FRAMES, &got) == MA_SUCCESS,
              "decoder returned frames");
        check(got == FRAMES, "decoder returned every frame");

        // f32 WAV is lossless, so this is an equality check within float noise.
        float worst = 0.0f;
        for (ma_uint64 i = 0; i < got; ++i) {
            worst = std::max(worst, std::abs(read[i] - written[i]));
        }
        check(worst < 1e-5f, "samples survive the round trip");

        // Guard against the degenerate pass: silence would also round-trip.
        float peak = 0.0f;
        for (float v : read) peak = std::max(peak, std::abs(v));
        check(peak > 0.45f, "decoded signal is the sine, not silence");

        ma_decoder_uninit(&dec);
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);

    if (ok) std::println("miniaudio OK");
    return ok ? 0 : 1;
}
