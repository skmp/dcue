/*
    aica adpcm <-> wave converter;

    (c) 2002 BERO <bero@geocities.co.jp>
    under GPL or notify me

    aica adpcm seems same as YMZ280B adpcm
    adpcm->pcm algorithm can found MAME/src/sound/ymz280b.c by Aaron Giles

    this code is for little endian machine

    Modified by Megan Potter to read/write ADPCM WAV files, and to
    handle stereo (though the stereo is very likely KOS specific
    since we make no effort to interleave it). Please see README.GPL
    in the KOS docs dir for more info on the GPL license.

    Modified by skmp for dca3
*/

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <assert.h>

#define MINIMP3_IMPLEMENTATION
#define MINIMP3_ONLY_MP3

#include "minimp3_ex.h"

static int ym_diff_lookup[16] = {
    1, 3, 5, 7, 9, 11, 13, 15,
    -1, -3, -5, -7, -9, -11, -13, -15,
};

static int ym_index_scale[16] = {
    0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266,
    0x0e6, 0x0e6, 0x0e6, 0x0e6, 0x133, 0x199, 0x200, 0x266 /* same value for speedup */
};

static inline int limit(int val, int min, int max) {
    if(val < min) return min;
    else if(val > max) return max;
    else return val;
}

const int ima_step_size_table[89] = {
    7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31,
    34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143,
    157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024,
    3327, 3660, 4026, 4428, 4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899,
    15289, 16818, 18500, 20350, 22385, 24623, 27086, 29794, 32767
};

const int ima_index_table[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8, -1, -1, -1, -1, 2, 4, 6, 8
};

typedef struct {
    int predictor;
    int step_index;
} ImaAdpcmDecoder;

void ima_initialize_decoder(ImaAdpcmDecoder *decoder, int initial_predictor, int initial_step_index) {
    decoder->predictor = initial_predictor;
    decoder->step_index = initial_step_index;
}

int ima_decode_nibble(ImaAdpcmDecoder *decoder, uint8_t nibble) {
    int step = ima_step_size_table[decoder->step_index];
    int diff = step >> 3;

    if (nibble & 1) diff += step >> 2;
    if (nibble & 2) diff += step >> 1;
    if (nibble & 4) diff += step;

    if (nibble & 8)
        decoder->predictor -= diff;
    else
        decoder->predictor += diff;

    // Clamp the predictor to 16-bit signed values
    if (decoder->predictor > 32767)
        decoder->predictor = 32767;
    else if (decoder->predictor < -32768)
        decoder->predictor = -32768;

    // Update step_index
    decoder->step_index += ima_index_table[nibble];
    if (decoder->step_index < 0) decoder->step_index = 0;
    if (decoder->step_index > 88) decoder->step_index = 88;

    return decoder->predictor;
}
void ima_decode_adpcm_blocks(const uint8_t *input, int input_size, int16_t *output, size_t output_size, int channels, int block_size) {
    ImaAdpcmDecoder decoders[2]; // Support for stereo
    int out_index = 0;

    for (int i = 0; i + block_size <= input_size; i += block_size) {
        // Read block header
        int16_t predictor_left = (input[i + 1] << 8) | input[i];  // Predictor for left channel (first 2 bytes)
        int step_index_left = input[i + 2];                       // Step index for left channel (3rd byte)

        output[out_index++] = predictor_left;

        int16_t predictor_right = 0;
        int step_index_right = 0;

        if (channels == 2) {
            predictor_right = (input[i + 5] << 8) | input[i + 4]; // Predictor for right channel (4th and 5th bytes)
            step_index_right = input[i + 6];                      // Step index for right channel (6th byte)

            output[out_index++] = predictor_right;
        }

        // Initialize decoders with block header values
        ima_initialize_decoder(&decoders[0], predictor_left, step_index_left);
        if (channels == 2) ima_initialize_decoder(&decoders[1], predictor_right, step_index_right);

        // Decode nibbles after the header
        int start_offset = channels == 2 ? 8 : 4; // Stereo uses 8 bytes for the header, mono uses 4

        if (channels == 2) {
            for (int j = i + start_offset; j < i + block_size; j+= 8) {
                for(int k=j; k<j+4; k++) {
                    output[out_index++]  = ima_decode_nibble(&decoders[0], input[k + 0] & 0x0F);
                    output[out_index++]  = ima_decode_nibble(&decoders[1], input[k + 4] & 0x0F);
                    if (output_size*2 == out_index) return;
                    output[out_index++]  = ima_decode_nibble(&decoders[0], input[k + 0] >> 4);
                    output[out_index++]  = ima_decode_nibble(&decoders[1], input[k + 4] >> 4);
                    if (output_size*2 == out_index) return;
                }
            }
        } else {
            for (int j = i + start_offset; j < i + block_size; j++) {
                uint8_t byte = input[j];
                // Mono: Decode both nibbles for the single channel
                output[out_index++] = ima_decode_nibble(&decoders[0], byte & 0x0F); // Low nibble
                output[out_index++] = ima_decode_nibble(&decoders[0], byte >> 4);   // High nibble
                if (output_size*2 == out_index) return;
            }
        }
    }
}

#define CLAMP(x, low, high)  (((x) > (high)) ? (high) : (((x) < (low)) ? (low) : (x)))

static inline int16_t ymz_step(uint8_t step, int16_t *history, int16_t *step_size) {
    static const int step_table[8] = {
        230, 230, 230, 230, 307, 409, 512, 614
    };

    int sign = step & 8;
    int delta = step & 7;
    int diff = ((1 + (delta << 1)) * *step_size) >> 3;
    int newval = *history;
    int nstep = (step_table[delta] * *step_size) >> 8;

    /* Only found in the official AICA encoder
       but it's possible all chips (including ADPCM-B) does this. */
    diff = CLAMP(diff, 0, 32767);
    if(sign > 0)
        newval -= diff;
    else
        newval += diff;

    *step_size = CLAMP(nstep, 127, 24576);
    *history = newval = CLAMP(newval, -32768, 32767);
    return newval;
}

void pcm2adpcm(uint8_t *outbuffer, int16_t *buffer, size_t bytes) {
    long i;
    int16_t step_size = 127;
    int16_t history = 0;
    uint8_t buf_sample = 0, nibble = 0;
    uint32_t adpcm_sample;
    size_t num_samples = bytes / 2; /* Divide by 2 to get the number of 16-bit samples */

    for(i = 0;i < num_samples;i++) {
        /* We remove a few bits_per_sample of accuracy to reduce some noise. */
        int step = ((*buffer++) & -8) - history;
        adpcm_sample = (abs(step) << 16) / (step_size << 14);
        adpcm_sample = CLAMP(adpcm_sample, 0, 7);
        if(step < 0)
            adpcm_sample |= 8;
        if(!nibble)
            *outbuffer++ = buf_sample | (adpcm_sample<<4);
        else
            buf_sample = (adpcm_sample & 15);
        nibble ^= 1;
        ymz_step(adpcm_sample, &history, &step_size);
    }
}

size_t  deinterleave(void *buffer, size_t size) {
    short *buf, *buf1, *buf2;
    int i;

    buf = (short *)buffer;
    size_t channel_size = ((size/2) + 8191) & ~8191;
    buf1 = calloc(1, channel_size);
    buf2 = calloc(1, channel_size);

    for (i = 0; i < size / 4; i++) {
        buf1[i] = buf[i * 2 + 0];
        buf2[i] = buf[i * 2 + 1];
    }

    memcpy(buf, buf1, channel_size);
    memcpy(buf + channel_size / sizeof(*buf), buf2, channel_size);

    free(buf1);
    free(buf2);

    return channel_size;
}

// interleaves every 8KB (16K samples)
size_t interleave_adpcm(void *buffer, size_t bytes) {

    uint8_t *buf, *buf1, *buf2;
    int i;

    buf = malloc(bytes + 64 * 1024);
    buf1 = (uint8_t *)buffer;
    buf2 = buf1 + bytes / 2;

    uint8_t *ptr = buf;
    size_t remaining_bytes = bytes/2;

    size_t total_bytes = 0;
    while (remaining_bytes) {
        size_t bytes_to_copy = remaining_bytes > 8 * 1024 ? 8 * 1024 : remaining_bytes;
        memset(ptr, 0, 8 * 1024);
        memcpy(ptr, buf1, bytes_to_copy);
        ptr += 8 * 1024;
        buf1 += bytes_to_copy;
        memset(ptr, 0, 8 * 1024);
        memcpy(ptr, buf2, bytes_to_copy);
        ptr += 8 * 1024;
        buf2 += bytes_to_copy;
        remaining_bytes -= bytes_to_copy;

        total_bytes += 16 * 1024;
    }

    assert(total_bytes <= bytes + 64 * 1024);
    memcpy(buffer, buf, total_bytes);

    free(buf);

    return total_bytes;
}

typedef struct wavhdr_t {
    char hdr1[4];
    int32_t totalsize;

    char hdr2[8];
    int32_t hdrsize;
    short format;
    short channels;
    int32_t freq;
    int32_t byte_per_sec;
    short blocksize;
    short bits;
} wavhdr_t;

typedef struct wavhdr3_t {
    char hdr3[4];
    int32_t datasize;
} wavhdr3_t;

int validate_wav_header(wavhdr_t *wavhdr, wavhdr3_t *wavhdr3, int format, int bits, FILE *in) {
    int result = 0;

    if (memcmp(wavhdr->hdr1, "RIFF", 4)) {
        fprintf(stderr, "Invalid RIFF header.\n");
        result = 1;
    }

    if (memcmp(wavhdr->hdr2, "WAVEfmt ", 8)) {
        fprintf(stderr, "Invalid WAVEfmt header.\n");
        result = 1;
    }

    if (wavhdr->hdrsize < 0x10) {
        fprintf(stderr, "Invalid header size, %d bytes\n", wavhdr->hdrsize);
        result = 1;
    } else if (wavhdr->hdrsize > 0x10) {
        fprintf(stderr, "Unusual header size, seeking %d bytes\n", wavhdr->hdrsize - 0x10);
        fseek(in, wavhdr->hdrsize - 0x10, SEEK_CUR);
    }

    if (wavhdr->format != format) {
        fprintf(stderr, "Unsupported format.\n");
        result = 1;
    }

    if (wavhdr->channels != 1 && wavhdr->channels != 2) {
        fprintf(stderr, "Unsupported number of channels.\n");
        result = 1;
    }

    if (wavhdr->bits != bits) {
        fprintf(stderr, "Unsupported bit depth.\n");
        result = 1;
    }

    for (;;) {
        if (fread(wavhdr3->hdr3, 1, 4, in) != 4) {
            fprintf(stderr, "Failed to read next chunk header!\n");
            result = 1;
            break;
        }

        if (fread(&wavhdr3->datasize, 1, 4, in) != 4) {
            fprintf(stderr, "Failed to read chunk size!\n");
            result = 1;
            break;
        }

        if (memcmp(wavhdr3->hdr3, "data", 4)) {
            fseek(in, wavhdr3->datasize, SEEK_CUR);
        } else {
            break;
        }
    }

    return result;
}

int loadMp3(const char *infile, size_t *pcmsize, short **pcmbuf, int *channels, int *freq) {
    static mp3dec_t mp3d;
    mp3dec_file_info_t info;
    if (mp3dec_load(&mp3d, infile, &info, NULL, NULL) || info.samples == 0) {
        printf("Error: mp3dec_load() failed\n");
        return 0;
    } else {
        printf("MP3 channels: %d, hz: %d, samples: %ld\n", info.channels, info.hz, info.samples);
        *pcmsize = info.samples * sizeof(short);
        *pcmbuf = info.buffer;
        *channels = info.channels;
        *freq = info.hz;
        return 1;
    }
}

int loadWav(const char *infile, size_t *pcmsize, short **pcmbuf, int *channels, int *freq) {
    wavhdr_t wavhdr;
    wavhdr3_t wavhdr3;

    FILE *in = fopen(infile, "rb");

    if (!in) {
        printf("can't open %s\n", infile);
        return 0;
    }

    if (fread(&wavhdr, sizeof(wavhdr), 1, in) != 1) {
        fprintf(stderr, "Cannot read header.\n");
        fclose(in);
        return 0;
    }

    if (validate_wav_header(&wavhdr, &wavhdr3, 1, 16, in)) {
        fclose(in);
        return 0;
    }

    *pcmsize = wavhdr3.datasize;
    *pcmbuf = malloc(*pcmsize);
    if (fread(*pcmbuf, *pcmsize, 1, in) != 1) {
        fprintf(stderr, "Cannot read data.\n");
        fclose(in);
        return 0;
    }

    printf("PCM: channels: %d, hz: %d, samples: %d\n", wavhdr.channels, wavhdr.freq, wavhdr3.datasize);

    *channels = wavhdr.channels;
    *freq = wavhdr.freq;
    fclose(in);
    return 1;
}

int loadWavIMA(const char *infile, size_t *pcmsize, short **pcmbuf, int *channels, int *freq) {
    wavhdr_t wavhdr;
    wavhdr3_t wavhdr3;

    FILE *in = fopen(infile, "rb");

    if (!in) {
        printf("can't open %s\n", infile);
        return 0;
    }

    if (fread(&wavhdr, sizeof(wavhdr), 1, in) != 1) {
        fprintf(stderr, "Cannot read header.\n");
        fclose(in);
        return 0;
    }

    if (validate_wav_header(&wavhdr, &wavhdr3, 0x11, 4, in)) {
        fclose(in);
        return 0;
    }

    uint8_t *adpcmbuf = malloc(wavhdr3.datasize);
    if (fread(adpcmbuf, wavhdr3.datasize, 1, in) != 1) {
        fprintf(stderr, "Cannot read data.\n");
        free(adpcmbuf);
        fclose(in);
        return 0;
    }

    printf("IMA ADPCM: channels: %d, hz: %d, block size: %d, data size: %d\n", 
           wavhdr.channels, wavhdr.freq, wavhdr.blocksize, wavhdr3.datasize);

    // Calculate the number of samples
    int samples_per_block = (wavhdr.blocksize - 4 * wavhdr.channels) * 2 / wavhdr.channels + 1;
    int total_blocks = wavhdr3.datasize / wavhdr.blocksize;
    *pcmsize = total_blocks * samples_per_block * wavhdr.channels * sizeof(short);
    
    *pcmbuf = malloc(*pcmsize);
    *channels = wavhdr.channels;
    *freq = wavhdr.freq;

    ima_decode_adpcm_blocks(adpcmbuf, wavhdr3.datasize, *pcmbuf, *pcmsize, wavhdr.channels, wavhdr.blocksize);

    free(adpcmbuf);
    fclose(in);

    return 1;
}

int aud2adpcm(const char *infile, const char *outfile, int use_hdr, int to_mono, int lq) {
    FILE *in, *out;
    size_t pcmsize;
    short *pcmbuf;
    int channels, freq;

    if (!loadWav(infile, &pcmsize, &pcmbuf, &channels, &freq) && 
        !loadMp3(infile, &pcmsize, &pcmbuf, &channels, &freq) && 
        !loadWavIMA(infile, &pcmsize, &pcmbuf, &channels, &freq)) {
        fprintf(stderr, "Cannot load input file as wav, mp3, or IMA ADPCM.\n");
        return 1;
    }

    if (to_mono && channels == 2) {
        pcmsize /= 2;
        for (int i = 0; i < pcmsize / 2; i++) {
            pcmbuf[i] = (pcmbuf[i * 2 + 0] + pcmbuf[i * 2 + 1]) / 2;
        }
        channels = 1;
    }

    if (lq) {
        // Downsample to quarter sample rate
        freq /= 4;
        short *newbuf = malloc(pcmsize / 4);
        if (channels == 1) {
            for (int i = 0; i < pcmsize / 8; i++) {
                newbuf[i] = (pcmbuf[i * 4 + 0] + pcmbuf[i * 4 + 1] + pcmbuf[i * 4 + 2] + pcmbuf[i * 4 + 3]) / 4;
            }
        } else {
            assert(channels == 2);
            for (int i = 0; i < pcmsize / 16; i++) {
                newbuf[i * 2 + 0 ] = (pcmbuf[i * 8 + 0] + pcmbuf[i * 8 + 2] + pcmbuf[i * 8 + 4] + pcmbuf[i * 8 + 6]) / 4;
                newbuf[i * 2 + 1 ] = (pcmbuf[i * 8 + 1] + pcmbuf[i * 8 + 3] + pcmbuf[i * 8 + 5] + pcmbuf[i * 8 + 7]) / 4;
            }
        }
        memcpy(pcmbuf, newbuf, pcmsize / 4);
        free(newbuf);
        pcmsize /= 4;
    }

    // Round down to the nearest multiple of 4
    // adpcm data is always a multiple of 4 samples
    pcmsize = pcmsize & ~3;

    size_t adpcmsize = pcmsize / 4;
    size_t adpcmsize_data = adpcmsize;
    unsigned char *adpcmbuf = calloc(1, adpcmsize + 64 * 1024);// extra alloc for interleave_adpcm

    if (channels == 1) {
        pcmbuf = realloc(pcmbuf, pcmsize + 8192);
        memset(pcmbuf + pcmsize / sizeof(*pcmbuf), 0, 8192);
        pcm2adpcm(adpcmbuf, pcmbuf, pcmsize + 8192/sizeof(*pcmbuf));
        if (use_hdr) {
            adpcmsize_data = (adpcmsize_data + 8191) & ~8191;
        }
    } else {
        assert(use_hdr == 1);
        pcmbuf = realloc(pcmbuf, pcmsize + 8192*2);
        size_t channel_size = deinterleave(pcmbuf, pcmsize);
        pcm2adpcm(adpcmbuf, pcmbuf, channel_size);
        pcm2adpcm(adpcmbuf + channel_size / 4, pcmbuf + channel_size / sizeof(*pcmbuf), channel_size);
        adpcmsize_data = interleave_adpcm(adpcmbuf, channel_size/2);
    }

    out = fopen(outfile, "wb");
    if (!out) {
        fprintf(stderr, "Cannot write ADPCM data.\n");
        fclose(out);
        free(pcmbuf);
        free(adpcmbuf);
        return -1;
    }

    if (use_hdr) {
        wavhdr_t wavhdr;
        wavhdr3_t wavhdr3;

        memset(&wavhdr, 0, sizeof(wavhdr));
        memcpy(wavhdr.hdr1, "RIFF", 4);
        wavhdr.totalsize = adpcmsize + sizeof(wavhdr) + sizeof(wavhdr3) - 8;
        memcpy(wavhdr.hdr2, "WAVEfmt ", 8);
        wavhdr.hdrsize = 0x10;
        wavhdr.format = 0x20; /* ITU G.723 ADPCM (Yamaha) */
        wavhdr.channels = channels;
        wavhdr.freq = freq;
        wavhdr.byte_per_sec = freq * channels / 2;
        wavhdr.blocksize = 4;
        wavhdr.bits = 4;

        memset(&wavhdr3, 0, sizeof(wavhdr3));
        memcpy(wavhdr3.hdr3, "data", 4);
        wavhdr3.datasize = adpcmsize;

        fwrite(&wavhdr, sizeof(wavhdr), 1, out);
        fwrite(&wavhdr3, sizeof(wavhdr3), 1, out);

        // data starts on the second sector
        fseek(out, 2048, SEEK_SET);

        assert(0 == (adpcmsize_data & 8191));
    }

    if (fwrite(adpcmbuf, adpcmsize_data, 1, out) != 1) {
        fprintf(stderr, "Cannot write ADPCM data.\n");
        fclose(out);
        free(pcmbuf);
        free(adpcmbuf);
        return 1;
    }

    fclose(out);
    free(pcmbuf);
    free(adpcmbuf);

    return 0;
}

void usage() {
    printf("based on wav2adpcm: 16bit mono wav to aica adpcm and vice-versa (c)2002 BERO\n"
           " wav2adpcm -q <infile.wav/mp3/ima adpcm> <outfile.wav>   (To adpcm long stream)\n"
           " wav2adpcm -t <infile.wav/mp3/ima adpcm> <outfile.wav>   (To adpcm long stream)\n"
           " wav2adpcm -m <infile.wav/mp3/ima adpcm> <outfile.wav>   (To adpcm MONO long stream)\n"           
           " wav2adpcm -raw <infile.wav/mp3/ima adpcm> <outfile.raw>   (To adpcm sfx)\n"
           "\n"
           "If you are having trouble with your input wav file you can run it"
           "through ffmpeg first and then run wav2adpcm on output.wav:\n"
           " ffmpeg -i input.wav -ac 1 -acodec pcm_s16le output.wav"
          );
}

int main(int argc, char **argv) {

    if (argc == 4) {
        if (!strcmp(argv[1], "-t")) {
            return aud2adpcm(argv[2], argv[3], 1, 0, 0);
        } else if (!strcmp(argv[1], "-m")) {
            return aud2adpcm(argv[2], argv[3], 1, 1, 0);    
        } else if (!strcmp(argv[1], "-q")) {
            return aud2adpcm(argv[2], argv[3], 1, 1, 1);      
        } else if (!strcmp(argv[1], "-raw")) {
            return aud2adpcm(argv[2], argv[3], 0, 0, 0);
        } else {
            usage();
            return 1;
        }
    } else {
        usage();
        return 1;
    }
}
