#include "pch.h"
#include "HitSounds.h"
#include "hitsounds_data.h"
#include "app/Settings.h"

#include <Windows.h>
#include <mmsystem.h>
#include <mmreg.h>
#include <msacm.h>

#include <cstdint>
#include <cstring>
#include <vector>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "msacm32.lib")

namespace {

    struct WavChunks {
        const WAVEFORMATEX* fmt{ nullptr };
        DWORD fmt_bytes{ 0 };
        const BYTE* data{ nullptr };
        DWORD data_bytes{ 0 };
    };

    bool ParseWav(const unsigned char* bytes, std::size_t size, WavChunks& out)
    {
        if (!bytes || size < 12)
            return false;
        if (std::memcmp(bytes, "RIFF", 4) != 0 || std::memcmp(bytes + 8, "WAVE", 4) != 0)
            return false;

        std::size_t off = 12;
        while (off + 8 <= size)
        {
            const char* id = reinterpret_cast<const char*>(bytes + off);
            const DWORD chunk_size = *reinterpret_cast<const DWORD*>(bytes + off + 4);
            const BYTE* payload = bytes + off + 8;
            if (off + 8 + chunk_size > size)
                break;

            if (std::memcmp(id, "fmt ", 4) == 0)
            {
                out.fmt = reinterpret_cast<const WAVEFORMATEX*>(payload);
                out.fmt_bytes = chunk_size;
            }

            else if (std::memcmp(id, "data", 4) == 0)
            {
                out.data = payload;
                out.data_bytes = chunk_size;
            }

            off += 8 + chunk_size + (chunk_size & 1u);
        }

        return out.fmt != nullptr && out.data != nullptr && out.data_bytes > 0 && out.fmt_bytes >= 16;
    }

    bool DecodeToPcm16(const WavChunks& wav, std::vector<BYTE>& pcm, WAVEFORMATEX& pcm_fmt)
    {
        std::vector<BYTE> src_fmt_buf(wav.fmt_bytes);
        std::memcpy(src_fmt_buf.data(), wav.fmt, wav.fmt_bytes);
        auto* src_fmt = reinterpret_cast<WAVEFORMATEX*>(src_fmt_buf.data());

        pcm_fmt = {};
        pcm_fmt.wFormatTag = WAVE_FORMAT_PCM;
        pcm_fmt.nChannels = src_fmt->nChannels;
        pcm_fmt.nSamplesPerSec = src_fmt->nSamplesPerSec;
        pcm_fmt.wBitsPerSample = 16;
        pcm_fmt.nBlockAlign = static_cast<WORD>(pcm_fmt.nChannels * (pcm_fmt.wBitsPerSample / 8));
        pcm_fmt.nAvgBytesPerSec = pcm_fmt.nSamplesPerSec * pcm_fmt.nBlockAlign;
        pcm_fmt.cbSize = 0;

        if (src_fmt->wFormatTag == WAVE_FORMAT_PCM && src_fmt->wBitsPerSample == 16)
        {
            pcm.assign(wav.data, wav.data + wav.data_bytes);
            return !pcm.empty();
        }

        HACMSTREAM stream = nullptr;
        if (acmStreamOpen(&stream, nullptr, src_fmt, &pcm_fmt, nullptr, 0, 0, ACM_STREAMOPENF_NONREALTIME) != 0)
            return false;

        DWORD dst_bytes = 0;
        if (acmStreamSize(stream, wav.data_bytes, &dst_bytes, ACM_STREAMSIZEF_SOURCE) != 0 || dst_bytes == 0)
        {
            acmStreamClose(stream, 0);
            return false;
        }

        pcm.resize(dst_bytes);

        ACMSTREAMHEADER hdr{};
        hdr.cbStruct = sizeof(hdr);
        hdr.pbSrc = const_cast<BYTE*>(wav.data);
        hdr.cbSrcLength = wav.data_bytes;
        hdr.pbDst = pcm.data();
        hdr.cbDstLength = dst_bytes;

        bool ok = false;
        if (acmStreamPrepareHeader(stream, &hdr, 0) == 0)
        {
            if (acmStreamConvert(stream, &hdr, ACM_STREAMCONVERTF_BLOCKALIGN) == 0)
            {
                pcm.resize(hdr.cbDstLengthUsed);
                ok = !pcm.empty();
            }
            acmStreamUnprepareHeader(stream, &hdr, 0);
        }

        acmStreamClose(stream, 0);
        return ok;
    }

    void ApplyVolume(std::vector<BYTE>& pcm16, float volume)
    {
		if (volume < 0.f) volume = 0.f;
		if (volume > 1.f) volume = 1.f;
		if (volume >= 0.999f)
			return;

		auto* samples = reinterpret_cast<std::int16_t*>(pcm16.data());
		std::size_t count = pcm16.size() / sizeof(std::int16_t);
		for (std::size_t i = 0; i < count; ++i)
		{
			float s = (float)samples[i] * volume;
			if (s > 32767.0f)
				samples[i] = 32767;

			else if (s < -32768.0f)
				samples[i] = -32768;

			else
				samples[i] = (std::int16_t)s;
		}
    }

    void BuildWavMemory(const WAVEFORMATEX& fmt, const std::vector<BYTE>& pcm, std::vector<BYTE>& out)
    {
        const DWORD fmt_size = 16;
        const DWORD data_size = static_cast<DWORD>(pcm.size());
        const DWORD riff_size = 4 + (8 + fmt_size) + (8 + data_size);

        out.resize(8 + riff_size);
        BYTE* p = out.data();

        std::memcpy(p, "RIFF", 4); p += 4;
        *reinterpret_cast<DWORD*>(p) = riff_size; p += 4;
        std::memcpy(p, "WAVE", 4); p += 4;

        std::memcpy(p, "fmt ", 4); p += 4;
        *reinterpret_cast<DWORD*>(p) = fmt_size; p += 4;
        *reinterpret_cast<WORD*>(p) = fmt.wFormatTag; p += 2;
        *reinterpret_cast<WORD*>(p) = fmt.nChannels; p += 2;
        *reinterpret_cast<DWORD*>(p) = fmt.nSamplesPerSec; p += 4;
        *reinterpret_cast<DWORD*>(p) = fmt.nAvgBytesPerSec; p += 4;
        *reinterpret_cast<WORD*>(p) = fmt.nBlockAlign; p += 2;
        *reinterpret_cast<WORD*>(p) = fmt.wBitsPerSample; p += 2;

        std::memcpy(p, "data", 4); p += 4;
        *reinterpret_cast<DWORD*>(p) = data_size; p += 4;
        if (data_size)
            std::memcpy(p, pcm.data(), data_size);
    }

    std::vector<BYTE> g_play_bufs[2];
    int g_play_slot = 0;

}

namespace Cheat::Features::HitSounds {

    const char* const* Names()
    {
        const int n = HitSoundsData::Count();
        static std::vector<const char*> names;
        static bool ready = false;
        if (!ready) {
            names.resize((std::size_t)n);
            const auto* entries = HitSoundsData::Entries();
            for (int i = 0; i < n; ++i)
                names[(std::size_t)i] = entries[i].name;
            ready = true;
        }
        return names.data();
    }

    int Count()
    {
        return HitSoundsData::Count();
    }

    const char* FileName(int index)
    {
        if (index < 0 || index >= HitSoundsData::Count())
            return "";
        return HitSoundsData::At(index).name;
    }

    void Play(int index)
    {
		if (index < 0 || index >= HitSoundsData::Count())
			return;

		float vol_pct = g_Settings.hitsound.volume;
		if (vol_pct <= 0.0f)
			return;

		const auto& e = HitSoundsData::At(index);
		if (!e.data || e.size == 0)
			return;

		if (vol_pct > 100.f) vol_pct = 100.f;
		float volume = vol_pct / 100.0f;

		if (volume >= 0.999f)
		{
			PlaySoundA(reinterpret_cast<LPCSTR>(e.data), nullptr,
				SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
			return;
		}

		WavChunks chunks{};
		if (!ParseWav(e.data, e.size, chunks))
			return;

		std::vector<BYTE> pcm;
		WAVEFORMATEX pcm_fmt{};
		if (!DecodeToPcm16(chunks, pcm, pcm_fmt))
			return;

		ApplyVolume(pcm, volume);

		g_play_slot ^= 1;
		auto& buf = g_play_bufs[g_play_slot];
		BuildWavMemory(pcm_fmt, pcm, buf);

		PlaySoundA(reinterpret_cast<LPCSTR>(buf.data()), nullptr,
			SND_MEMORY | SND_ASYNC | SND_NODEFAULT);
    }

    void PlayCurrent()
    {
        Play(g_Settings.hitsound.index);
    }

}
