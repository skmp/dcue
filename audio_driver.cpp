#include "components/audio_clips.h"
#include "components/audio_sources.h"

#include <mutex>
#include <cassert>
#include "vendor/dca3/thread.h"
#include <iostream>
#include <queue>

#if defined(DC_SH4)
#include <dc/sound/sound.h>
#include <dc/sound/sfxmgr.h>
#include <dc/sound/stream.h>
#include <dc/spu.h>
#include <dc/g2bus.h>
#include <dc/sound/aica_comm.h>
#include <kos/dbglog.h>

#define syncf(...) // dbglog(DBG_CRITICAL, __VA_ARGS__)
#define streamf(...) // dbglog(DBG_CRITICAL, __VA_ARGS__)
#define verbosef(...) // dbglog(DBG_CRITICAL, __VA_ARGS__)
#define debugf(...)  // dbglog(DBG_CRITICAL, __VA_ARGS__)
#define infof(...) dbglog(DBG_CRITICAL, __VA_ARGS__)

#define STREAM_STAGING_BUFFER_SIZE 16384
#define STREAM_STAGING_READ_SIZE_STEREO 16384
#define STREAM_STAGING_READ_SIZE_MONO (STREAM_STAGING_READ_SIZE_STEREO / 2)
#define STREAM_CHANNEL_BUFFER_SIZE (STREAM_STAGING_READ_SIZE_MONO * 2)	// lower and upper halves
#define STREAM_CHANNEL_SAMPLE_COUNT (STREAM_CHANNEL_BUFFER_SIZE * 2)		// 4 bit adpcm

#define SPU_RAM_UNCACHED_BASE_U8 ((uint8_t *)SPU_RAM_UNCACHED_BASE)
// ************************************************************************************************
// Begin AICA Driver stuff

#define AICA_MEM_CHANNELS   0x020000    /* 64 * 16*4 = 4K */

/* Quick access to the AICA channels */
#define AICA_CHANNEL(x)     (AICA_MEM_CHANNELS + (x) * sizeof(aica_channel_t))

static uint32_t chn_version[64];

int aica_play_chn(int chn, int size, uint32_t aica_buffer, int fmt, int vol, int pan, int loop, int freq) {
	// assert(size <= 65534);
	// We gotta fix this at some point
	if (size >= 65535) {
		debugf("aica_play_chn: size too large for %p, %d, truncating to 65534\n", (void*)aica_buffer, size);
		size = 65534;
	}

    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);
    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_START;
    chan->base = aica_buffer;
    chan->type = fmt;
    chan->length = size;
    chan->loop = loop;
    chan->loopstart = 0;
    chan->loopend = size;
    chan->freq = freq;
    chan->vol = vol;
    chan->pan = pan;
	chan->version = ++chn_version[chn];
	snd_sh4_to_aica(tmp, cmd->size);
    return chn;
}

void aica_stop_chn(int chn) {
	AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_STOP;
    snd_sh4_to_aica(tmp, cmd->size);
}

void aica_volpan_chn(int chn, int vol, int pan) {
	AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_PAN | AICA_CH_UPDATE_SET_VOL;
    chan->vol = vol;
	chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}


void aica_snd_sfx_volume(int chn, int vol) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_VOL;
    chan->vol = vol;
    snd_sh4_to_aica(tmp, cmd->size);
}

void aica_snd_sfx_pan(int chn, int pan) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_PAN;
    chan->pan = pan;
    snd_sh4_to_aica(tmp, cmd->size);
}

void aica_snd_sfx_freq(int chn, int freq) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ;
    chan->freq = freq;
    snd_sh4_to_aica(tmp, cmd->size);
}


void aica_snd_sfx_freq_vol(int chn, int freq, int vol) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_FREQ | AICA_CH_UPDATE_SET_VOL;
    chan->freq = freq;
	chan->vol = vol;
    snd_sh4_to_aica(tmp, cmd->size);
}

// End of aica Driver stuff
// ************************************************************************************************

#define BANK_STAGE_SIZE 16 * 2048
static  uint8_t stagingBufferBank[BANK_STAGE_SIZE] __attribute__((aligned(32)));

#define MAX_STREAMS 16
struct alignas(32) stream_info {
	uint8_t buffer[STREAM_STAGING_BUFFER_SIZE];
	std::mutex mtx;
	file_t fd;
	uint32_t aica_buffers[2]; // left, right
	int mapped_ch[2];	// left, right
	int rate;
	int total_samples;
	int played_samples;
	int file_offset;
	int vol;
	uint8_t nPan;
	uint8_t pan[2];
	audio_source_t* source; // if non null it is playing

	bool stereo;
	bool next_is_upper_half;
	bool first_refill;
	bool paused;
}; 

static stream_info streams[MAX_STREAMS];

#define MAX_SFX_CHANNELS (64 - (MAX_STREAMS*2))

static struct {
    audio_source_t* source;
    int mapped_ch;
} sfx_channel[MAX_SFX_CHANNELS];

static dc::Thread snd_thread;
static dc::Thread io_thread;

struct io_request_t {
    int fd;
    size_t offset;
    void* buffer;
    size_t size;
};
std::queue<io_request_t> requests;
static semaphore_t requests_sema = SEM_INITIALIZER(0);

void queue_read(int fd, size_t offset, void* buffer, size_t size) {
    auto mask = irq_disable();
    requests.emplace(fd, offset, buffer, size);
    irq_restore(mask);
    sem_signal(&requests_sema);
}

void* audio_io_thread(void*) {
    for (;;) {
        sem_wait(&requests_sema);

        try_again:
        auto mask = irq_disable();
        if (requests.size() == 0) {
            irq_restore(mask);
            continue;
        }
        auto request = requests.front();
        requests.pop();
        irq_restore(mask);
        fs_seek(request.fd, request.offset, SEEK_SET);
        fs_read(request.fd, request.buffer, request.size);
        goto try_again;
    }
}

void* audio_periodical(void*) {
    for(;;) {
        auto mask = irq_disable();

        {
            for (int i = 0; i < MAX_SFX_CHANNELS; i++) {
                if (sfx_channel[i].source != nullptr) {
                    int mapped_ch = sfx_channel[i].mapped_ch;

                    uint32_t channel_version = g2_read_32(SPU_RAM_UNCACHED_BASE + AICA_CHANNEL(mapped_ch) + offsetof(aica_channel_t, version));

                    if (chn_version[mapped_ch] != channel_version) {
                        syncf("SFX version missmatch, skipping update. expected %d got %d\n", chn_version[mapped_ch], channel_version);
                        continue;
                    }
                    // uint16_t channel_pos = (g2_read_32(SPU_RAM_UNCACHED_BASE + AICA_CHANNEL(mapped_ch) + offsetof(aica_channel_t, pos)) & 0xffff);
                    // verbosef("Channel %d pos: %d\n", i, channel_pos);
                    if (!sfx_channel[i].source->loop) {
                        auto channel_looped = g2_read_32(SPU_RAM_UNCACHED_BASE + AICA_CHANNEL(mapped_ch) + offsetof(aica_channel_t, looped));
                        // the looped flag is set even for one shots and is a reliable way to know if the channel has finished playing
                        if (channel_looped) {
                            debugf("Auto stopping channel: %d -> %d\n", i, mapped_ch);
                            sfx_channel[i].source->playingChannel = -1;
                            sfx_channel[i].source = nullptr;
                        }
                    }
                }
            }
        }

        for (int i = 0; i< MAX_STREAMS; i++) {
            size_t do_read = 0;
            {
                if (streams[i].source != nullptr) {
                    uint32_t channel_version = g2_read_32(SPU_RAM_UNCACHED_BASE + AICA_CHANNEL(streams[i].mapped_ch[0]) + offsetof(aica_channel_t, version));

                    if (chn_version[streams[i].mapped_ch[0]] != channel_version) {
                        syncf("Stream version missmatch, skipping update. expected %d got %d\n", chn_version[streams[i].mapped_ch[0]], channel_version);
                        continue;
                    }
                    // get channel pos
                    uint32_t channel_pos = g2_read_32(SPU_RAM_UNCACHED_BASE + AICA_CHANNEL(streams[i].mapped_ch[0]) + offsetof(aica_channel_t, pos)) & 0xffff;
                    uint32_t logical_pos = channel_pos;
                    if (logical_pos > STREAM_CHANNEL_SAMPLE_COUNT/2) {
                        logical_pos -= STREAM_CHANNEL_SAMPLE_COUNT/2;
                    }
                    streamf("Stream %d pos: %d, log: %d, played: %d\n", i, channel_pos, logical_pos, streams[i].played_samples);
        
                    bool can_refill = (streams[i].played_samples + STREAM_CHANNEL_SAMPLE_COUNT/2 + (!streams[i].first_refill)*STREAM_CHANNEL_SAMPLE_COUNT/2) < streams[i].total_samples;
                    bool can_fetch = (streams[i].played_samples + STREAM_CHANNEL_SAMPLE_COUNT/2 + STREAM_CHANNEL_SAMPLE_COUNT/2 + (!streams[i].first_refill)*STREAM_CHANNEL_SAMPLE_COUNT/2) < streams[i].total_samples;
                    // copy over data if needed from staging
                    if (channel_pos >= STREAM_CHANNEL_SAMPLE_COUNT/2 && !streams[i].next_is_upper_half) {
                        streams[i].next_is_upper_half = true;
                        if (can_refill) { // could we need a refill?
                            streamf("Filling channel %d with lower half\n", i);
                            // fill lower half
                            spu_memload(streams[i].aica_buffers[0], streams[i].buffer, STREAM_CHANNEL_BUFFER_SIZE/2);
                            if (streams[i].stereo) {
                                spu_memload(streams[i].aica_buffers[1], streams[i].buffer + STREAM_STAGING_READ_SIZE_MONO, STREAM_CHANNEL_BUFFER_SIZE/2);
                            }
                            // queue next read to staging if any
                            if (can_fetch) {
                                do_read = streams[i].stereo ? STREAM_STAGING_READ_SIZE_STEREO : STREAM_STAGING_READ_SIZE_MONO;
                            }
                        }
                        assert(streams[i].first_refill == false);
                        streams[i].played_samples += STREAM_CHANNEL_SAMPLE_COUNT/2;
                    } else if (channel_pos < STREAM_CHANNEL_SAMPLE_COUNT/2 && streams[i].next_is_upper_half) {
                        streams[i].next_is_upper_half = false;
                        if (can_refill) { // could we need a refill?
                            streamf("Filling channel %d with upper half\n", i);
                            // fill upper half
                            spu_memload(streams[i].aica_buffers[0] + STREAM_CHANNEL_BUFFER_SIZE/2, streams[i].buffer, STREAM_CHANNEL_BUFFER_SIZE/2);
                            if (streams[i].stereo) {
                                spu_memload(streams[i].aica_buffers[1] + STREAM_CHANNEL_BUFFER_SIZE/2, streams[i].buffer + STREAM_STAGING_READ_SIZE_MONO, STREAM_CHANNEL_BUFFER_SIZE/2);
                            }
                            // queue next read to staging, if any
                            if (can_fetch) {
                                do_read = streams[i].stereo ? STREAM_STAGING_READ_SIZE_STEREO : STREAM_STAGING_READ_SIZE_MONO;
                            }
                        }
                        if (streams[i].first_refill) {
                            streams[i].first_refill = false;
                        } else {
                            streams[i].played_samples += STREAM_CHANNEL_SAMPLE_COUNT/2;
                        }
                    }
                    // if end of file, stop
                    if ((streams[i].played_samples + logical_pos) > streams[i].total_samples) {
                        if (streams[i].source->loop) {
                            streams[i].file_offset = 0;
                            streams[i].played_samples = 0;
                        } else {
                            // stop channel
                            streamf("Auto stopping stream: %d -> {%d, %d}, %d total\n", i, streams[i].mapped_ch[0], streams[i].mapped_ch[1], streams[i].total_samples);
                            aica_stop_chn(streams[i].mapped_ch[0]);
                            aica_stop_chn(streams[i].mapped_ch[1]);
                            streams[i].source->playingChannel = -1;
                            streams[i].source = nullptr;
                            fs_close(streams[i].fd);
                            streams[i].fd = -1;
    
                            assert(do_read == 0);
                        }
                    }
                }
                
                if (do_read) {
                    streamf("Queueing stream read: %d, file: %d, buffer: %p, size: %d, file_offset: %d\n", i, streams[i].fd, streams[i].buffer, do_read, streams[i].file_offset);
                    queue_read(streams[i].fd, streams[i].file_offset, streams[i].buffer, do_read);
                    streams[i].file_offset += do_read;
                }
            }
        }
        irq_restore(mask);
        thd_sleep(50);
    }

    return nullptr;
}

void InitializeAudioClips() {
	auto init = snd_init();
	assert(init >= 0);

    for (int i = 0; i< MAX_STREAMS; i++) {
		streams[i].mapped_ch[0] = snd_sfx_chn_alloc();
		streams[i].mapped_ch[1] = snd_sfx_chn_alloc();
		streams[i].aica_buffers[0] = snd_mem_malloc(STREAM_CHANNEL_BUFFER_SIZE);
		streams[i].aica_buffers[1] = snd_mem_malloc(STREAM_CHANNEL_BUFFER_SIZE);
		debugf("Stream %d mapped to: %d, %d\n", i, streams[i].mapped_ch[0], streams[i].mapped_ch[1]);
		debugf("Stream %d buffers: %p, %p\n", i, (void*)streams[i].aica_buffers[0], (void*)streams[i].aica_buffers[1]);
		assert(streams[i].mapped_ch[0] != -1);
		assert(streams[i].mapped_ch[1] != -1);
		streams[i].fd = -1;

		streams[i].vol = 255;
		streams[i].nPan = 63;
		streams[i].pan[0] = 128;
		streams[i].pan[1] = 128;
	}

    for (int i = 0; i < MAX_SFX_CHANNELS; i++) {
		sfx_channel[i].mapped_ch = snd_sfx_chn_alloc();
		debugf("Channel %d mapped to %d\n", i, sfx_channel[i].mapped_ch);
		assert(sfx_channel[i].mapped_ch != -1);
	}

    auto audio_clip = audio_clips;

    while (*audio_clip) {
        (*audio_clip)->totalSamples = (*audio_clip)->totalSamples & ~3; // adpcm limitation

        if ((*audio_clip)->isSfx) {
            (*audio_clip)->sfxData = snd_mem_malloc((*audio_clip)->totalSamples/2); // each sample is 4 bits
            assert((*audio_clip)->sfxData != 0);

            void* stagingBuffer = stagingBufferBank;

            uintptr_t loadOffset = (*audio_clip)->sfxData;
            unsigned fileSize = (*audio_clip)->totalSamples/2;

            infof("Loading %s, %ld samples\n", (*audio_clip)->file, (*audio_clip)->totalSamples);

            int fd = fs_open((*audio_clip)->file, O_RDONLY);
            assert(fd >= 0);
			while (fileSize > 0) {
				size_t readSize = fileSize > sizeof(stagingBufferBank) ? sizeof(stagingBufferBank) : fileSize;
				int rs = fs_read(fd, stagingBuffer, readSize);
				debugf("Read %d bytes, expected %d\n", rs, readSize);
				assert(rs == readSize);
				spu_memload(loadOffset, stagingBuffer, readSize);
				loadOffset += readSize;
				fileSize -= readSize;
				debugf("Loaded %d bytes, %d remaining\n", readSize, fileSize);
			}
            fs_close(fd);
        }
        audio_clip++;
    }

    snd_thread.spawn("Audio Streamer", 1024 * 2, true, &audio_periodical);
    io_thread.spawn("IO Thread", 1024, true, &audio_io_thread);
}

int find_audio_source_num(audio_source_t* ptr) {
    auto audio_source = audio_sources;
    int num = 0;
    while (*audio_source) {
        if (*audio_source == ptr) {
            return num;
        }
        num++;
        audio_source++;
    }
    return -1;
}

void audio_source_t::play() {
    assert(enabled && "audio_source_t must be enabled before play");
    if (clip->isSfx) {
        auto mask = irq_disable();
        {
            for (unsigned i = 0; i < MAX_SFX_CHANNELS; i++) {
                if (sfx_channel[i].source == 0) {
                    sfx_channel[i].source = this;
                    this->playingChannel = i;

                    aica_play_chn(
                        sfx_channel[i].mapped_ch,
                        this->clip->totalSamples,
                        this->clip->sfxData,
                        2 /* ADPCM */,
                        int(this->volume * 255),
                        128,
                        this->loop,
                        (int)(this->clip->sampleRate * this->pitch)
                    );

                    break;
                }
            }
        }
        irq_restore(mask);
    } else {
        auto mask = irq_disable();
        {
            for (unsigned i = 0; i < MAX_STREAMS; i++) {
                if (streams[i].source == nullptr) {
                    int f = fs_open(this->clip->file, O_RDONLY);
                    assert(f >= 0);

                    auto nStream = i;
                    streams[nStream].rate = (int)(this->clip->sampleRate * this->pitch);
                    streams[nStream].stereo = false;
                    streams[nStream].pan[0] = 0;
                    streams[nStream].pan[1] = 255;
                    streams[nStream].vol = (int)(this->volume * 255);

                    assert(streams[nStream].fd == -1);
                    // if (streams[nStream].fd >= 0) {
                    //     CdStreamDiscardAudioRead(streams[nStream].fd);
                    //     fs_close(streams[nStream].fd);
                    // }
                    streams[nStream].fd = f;
                    streams[nStream].total_samples = this->clip->totalSamples;
                    streams[nStream].played_samples = 0;
                    streams[nStream].next_is_upper_half = true;
                    streams[nStream].first_refill = true;

                    // streamf("PreloadStreamedFile: %s: stream: %d, freq: %d, chans: %d, byte size: %d, played samples: %d\n", DCStreamedNameTable[nFile], nStream, hdr.samplesPerSec, hdr.numOfChan, hdr.dataSize, streams[nStream].played_samples);
                
                    irq_restore(mask);
                    #if 0
                    // Read directly in the future
                    fs_read(f, SPU_RAM_UNCACHED_BASE_U8 + streams[nStream].aica_buffers[0], STREAM_STAGING_READ_SIZE_MONO);
                    if (streams[nStream].stereo) {
                        fs_read(f, SPU_RAM_UNCACHED_BASE_U8 + streams[nStream].aica_buffers[1], STREAM_STAGING_READ_SIZE_MONO);
                    }
                    #else
                    // Stage to memory
                    fs_read(f, streams[nStream].buffer, streams[nStream].stereo ? STREAM_STAGING_READ_SIZE_STEREO : STREAM_STAGING_READ_SIZE_MONO);
                    spu_memload(streams[nStream].aica_buffers[0], streams[nStream].buffer, STREAM_CHANNEL_BUFFER_SIZE/2);
                    if (streams[nStream].stereo) {
                        spu_memload(streams[nStream].aica_buffers[1], streams[nStream].buffer + STREAM_STAGING_READ_SIZE_MONO, STREAM_CHANNEL_BUFFER_SIZE/2);
                    }
                    #endif

                    if (streams[nStream].total_samples > STREAM_CHANNEL_SAMPLE_COUNT/2) {
                        // If more than one buffer, prefetch the next one
                        fs_read(f, streams[nStream].buffer, streams[nStream].stereo ? STREAM_STAGING_READ_SIZE_STEREO : STREAM_STAGING_READ_SIZE_MONO);
                    }

                    mask = irq_disable();
                    streams[nStream].file_offset = fs_tell(f);

                    // mark as playing
                    streams[i].source = this;
                    this->playingChannel = i;

                    aica_play_chn(
                        streams[nStream].mapped_ch[0],
                        STREAM_CHANNEL_SAMPLE_COUNT,
                        streams[nStream].aica_buffers[0],
                        3 /* adpcm long stream */,
                        streams[nStream].vol,
                        streams[nStream].pan[0],
                        1,
                        streams[nStream].rate
                    );
                
                    aica_play_chn(
                        streams[nStream].mapped_ch[1],
                        STREAM_CHANNEL_SAMPLE_COUNT,
                        streams[nStream].aica_buffers[streams[nStream].stereo ? 1 : 0],
                        3 /* adpcm long stream */,
                        streams[nStream].vol,
                        streams[nStream].pan[1],
                        1,
                        streams[nStream].rate
                    );

                    break;
                }
            }
        }
        irq_restore(mask);
    }
}
void audio_source_t::awake() {
    if (playOnAwake) {
        play();
    }

    infof("audio source %d was awaken %d -> %s\n", find_audio_source_num(this), this->playingChannel, clip->file);
}

void audio_source_t::disable() {
    auto mask = irq_disable();
    infof("audio source %d, %d was disabled\n", find_audio_source_num(this), this->playingChannel);
    if (this->playingChannel != -1) {
        if (this->clip->isSfx) {
            assert(sfx_channel[this->playingChannel].source == this);
            aica_stop_chn(sfx_channel[this->playingChannel].mapped_ch);
            sfx_channel[this->playingChannel].source = nullptr;
        } else {
            assert(streams[this->playingChannel].source == this);
            aica_stop_chn(streams[this->playingChannel].mapped_ch[0]);
            aica_stop_chn(streams[this->playingChannel].mapped_ch[1]);
            streams[this->playingChannel].source = nullptr;
            assert(streams[this->playingChannel].fd >= 0);
            fs_close(streams[this->playingChannel].fd);
            streams[this->playingChannel].fd = -1;
        }
        this->playingChannel = -1;
    }
    irq_restore(mask);
}

void audio_source_t::update(native::game_object_t* listener) {
    auto dist = length(sub(gameObject->ltw.pos, listener->ltw.pos));

    float effVolume;
    if (dist <= minDistance) {
        effVolume = 1;
    } else if (dist < maxDistance) {
        assert(maxDistance > minDistance);
        effVolume = 1 - (dist - minDistance)/(maxDistance - minDistance);
    } else {
        effVolume = 0;
    }
    effVolume = volume * (1-spatialBlend) + effVolume*spatialBlend;

    int aicaVol = int(effVolume * 255);
    if (aicaVol > 255) {
        aicaVol = 255;
    }

    // set volume if applicable
    auto mask = irq_disable();
    {
        if (this->playingChannel != -1) {
            // infof("source %d clip %s volume %d\n", find_audio_source_num(this), this->clip->file, aicaVol);
            if (this->clip->isSfx) {
                assert(sfx_channel[this->playingChannel].source == this);
                aica_volpan_chn(sfx_channel[this->playingChannel].mapped_ch, aicaVol, 128);
            } else {
                assert(streams[this->playingChannel].source == this);
                aica_volpan_chn(streams[this->playingChannel].mapped_ch[0], aicaVol, 0);
                aica_volpan_chn(streams[this->playingChannel].mapped_ch[1], aicaVol, 255);
            }
        }
    }
    irq_restore(mask);
}
#else
void InitializeAudioClips() {
    // no op
}
void audio_source_t::play() {
    assert(enabled && "audio_source_t must be enabled before play");
}
void audio_source_t::awake() {
    if (playOnAwake) {
        play();
    }
}
void audio_source_t::disable() {}
void audio_source_t::update(native::game_object_t* go) {}

#endif

void audio_source_t::setEnabled(bool nv) {
    if (enabled != nv) {
        enabled = nv;
        if (nv) {
            awake();
        } else {
            disable();
        }
    }
}

void InitializeAudioSources() {
    auto audio_source = audio_sources;

    while (*audio_source) {
        (*audio_source)->playingChannel = -1;
        audio_source++;
    }
}