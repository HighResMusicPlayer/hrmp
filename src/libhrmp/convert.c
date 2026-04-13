/*
 * Copyright (C) 2026 The HighResMusicPlayer community
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "convert.h"

#include <sndfile.h>

#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#define HRMP_DSF_MAX_CHANNELS        8
#define HRMP_DSF_STAGE1_TAPS         256
#define HRMP_DSF_STAGE1_BYTES        (HRMP_DSF_STAGE1_TAPS / 8)
#define HRMP_DSF_STAGE1_HISTORY      64
#define HRMP_DSF_STAGE1_HISTORY_MASK (HRMP_DSF_STAGE1_HISTORY - 1)
#define HRMP_PCM_STAGE_TAPS          63
#define HRMP_PCM_STAGE_EVEN_TAPS     ((HRMP_PCM_STAGE_TAPS + 1) / 2)
#define HRMP_PCM_STAGE_ODD_TAPS      (HRMP_PCM_STAGE_TAPS / 2)
#define HRMP_DSF_SILENCE_BYTE        0x69u

struct hrmp_dsf_info
{
   uint64_t data_offset;
   uint64_t data_size;
   uint64_t total_samples;
   uint32_t sample_rate;
   uint32_t channels;
   uint32_t block_size;
   uint32_t bits_per_sample;
};

struct hrmp_dsd_stage1_state
{
   uint8_t history[HRMP_DSF_STAGE1_HISTORY];
   size_t pos;
};

struct hrmp_pcm_stage_filter
{
   float even_taps[HRMP_PCM_STAGE_EVEN_TAPS];
   float odd_taps[HRMP_PCM_STAGE_ODD_TAPS];
};

struct hrmp_pcm_stage_state
{
   float phase0_history[HRMP_PCM_STAGE_ODD_TAPS * 2];
   float phase1_history[HRMP_PCM_STAGE_EVEN_TAPS * 2];
   size_t phase0_pos;
   size_t phase1_pos;
   unsigned phase;
};

struct hrmp_channel_state
{
   struct hrmp_dsd_stage1_state dsd_stage;
   struct hrmp_pcm_stage_state pcm_stage_1;
   struct hrmp_pcm_stage_state pcm_stage_2;
};

static uint64_t
read_le_u64(const uint8_t* p)
{
   return ((uint64_t)p[0]) |
          ((uint64_t)p[1] << 8) |
          ((uint64_t)p[2] << 16) |
          ((uint64_t)p[3] << 24) |
          ((uint64_t)p[4] << 32) |
          ((uint64_t)p[5] << 40) |
          ((uint64_t)p[6] << 48) |
          ((uint64_t)p[7] << 56);
}

static uint32_t
read_le_u32(const uint8_t* p)
{
   return ((uint32_t)p[0]) |
          ((uint32_t)p[1] << 8) |
          ((uint32_t)p[2] << 16) |
          ((uint32_t)p[3] << 24);
}

static uint8_t
bitrev8(uint8_t x)
{
   x = ((x & 0xF0u) >> 4) | ((x & 0x0Fu) << 4);
   x = ((x & 0xCCu) >> 2) | ((x & 0x33u) << 2);
   x = ((x & 0xAAu) >> 1) | ((x & 0x55u) << 1);
   return x;
}

static void
design_lowpass_fir(float* taps, size_t tap_count, double cutoff_cycles)
{
   const double center = (double)(tap_count - 1) / 2.0;
   double scale = 0.0;

   for (size_t i = 0; i < tap_count; ++i)
   {
      const double offset = (double)i - center;
      const double x = 2.0 * M_PI * cutoff_cycles * offset;
      const double sinc = fabs(offset) < 1e-12
                             ? 2.0 * cutoff_cycles
                             : sin(x) / (M_PI * offset);
      const double window = 0.42 - 0.5 * cos((2.0 * M_PI * (double)i) / (double)(tap_count - 1)) +
                            0.08 * cos((4.0 * M_PI * (double)i) / (double)(tap_count - 1));
      const double value = sinc * window;

      taps[i] = (float)value;
      scale += value;
   }

   if (scale == 0.0)
   {
      return;
   }

   for (size_t i = 0; i < tap_count; ++i)
   {
      taps[i] = (float)((double)taps[i] / scale);
   }
}

static void
init_dsd_stage1_tables(float table[HRMP_DSF_STAGE1_BYTES][256])
{
   float taps[HRMP_DSF_STAGE1_TAPS];

   design_lowpass_fir(taps, HRMP_DSF_STAGE1_TAPS, 0.0200);

   for (size_t byte_index = 0; byte_index < HRMP_DSF_STAGE1_BYTES; ++byte_index)
   {
      for (unsigned value = 0; value < 256; ++value)
      {
         const uint8_t reversed = bitrev8((uint8_t)value);
         double acc = 0.0;

         for (size_t bit = 0; bit < 8; ++bit)
         {
            const int sample = ((reversed >> bit) & 1u) != 0u ? 1 : -1;
            acc += (double)sample * (double)taps[byte_index * 8 + bit];
         }

         table[byte_index][value] = (float)acc;
      }
   }
}

static void
init_pcm_stage_filter(struct hrmp_pcm_stage_filter* filter)
{
   float taps[HRMP_PCM_STAGE_TAPS];

   design_lowpass_fir(taps, HRMP_PCM_STAGE_TAPS, 0.22);

   for (size_t i = 0; i < HRMP_PCM_STAGE_EVEN_TAPS; ++i)
   {
      filter->even_taps[i] = taps[i * 2u];
   }

   for (size_t i = 0; i < HRMP_PCM_STAGE_ODD_TAPS; ++i)
   {
      filter->odd_taps[i] = taps[i * 2u + 1u];
   }
}

static void
init_dsd_stage_state(struct hrmp_dsd_stage1_state* state)
{
   memset(state->history, bitrev8(HRMP_DSF_SILENCE_BYTE), sizeof(state->history));
   state->pos = 0;
}

static void
init_pcm_stage_state(struct hrmp_pcm_stage_state* state)
{
   memset(state->phase0_history, 0, sizeof(state->phase0_history));
   memset(state->phase1_history, 0, sizeof(state->phase1_history));
   state->phase0_pos = 0;
   state->phase1_pos = 0;
   state->phase = 0;
}

static float
process_dsd_stage(struct hrmp_dsd_stage1_state* state,
                  const float table[HRMP_DSF_STAGE1_BYTES][256],
                  uint8_t dsf_byte)
{
   float acc = 0.0f;
   size_t pos;

   pos = state->pos;
   state->history[pos] = dsf_byte;

   for (size_t i = 0; i < HRMP_DSF_STAGE1_BYTES; ++i)
   {
      acc += table[i][state->history[(pos - i) & HRMP_DSF_STAGE1_HISTORY_MASK]];
   }

   state->pos = (pos + 1) & HRMP_DSF_STAGE1_HISTORY_MASK;
   return acc;
}

static void
write_pcm_history(float* history, size_t history_length, size_t* pos, float input)
{
   const size_t write_pos = *pos;

   history[write_pos] = input;
   history[write_pos + history_length] = input;
   *pos = (write_pos + 1u < history_length) ? write_pos + 1u : 0u;
}

static const float*
newest_pcm_history(const float* history, size_t history_length, size_t pos)
{
   const size_t newest = (pos == 0u) ? history_length - 1u : pos - 1u;

   return history + newest + history_length;
}

static bool
process_pcm_stage(struct hrmp_pcm_stage_state* state,
                  const struct hrmp_pcm_stage_filter* filter,
                  float input,
                  float* output)
{
   float acc = 0.0f;
   const float* phase1_history;
   const float* phase0_history;

   if (state->phase == 0u)
   {
      write_pcm_history(state->phase0_history, HRMP_PCM_STAGE_ODD_TAPS, &state->phase0_pos, input);
      state->phase = 1u;
      return false;
   }

   write_pcm_history(state->phase1_history, HRMP_PCM_STAGE_EVEN_TAPS, &state->phase1_pos, input);
   state->phase = 0u;

   phase1_history = newest_pcm_history(state->phase1_history, HRMP_PCM_STAGE_EVEN_TAPS, state->phase1_pos);
   phase0_history = newest_pcm_history(state->phase0_history, HRMP_PCM_STAGE_ODD_TAPS, state->phase0_pos);

   for (size_t i = 0; i < HRMP_PCM_STAGE_EVEN_TAPS; ++i)
   {
      acc += filter->even_taps[i] * phase1_history[-(ptrdiff_t)i];
   }

   for (size_t i = 0; i < HRMP_PCM_STAGE_ODD_TAPS; ++i)
   {
      acc += filter->odd_taps[i] * phase0_history[-(ptrdiff_t)i];
   }

   *output = acc;
   return true;
}

static int32_t
float_to_pcm32(float sample)
{
   const float clamped = sample < -1.0f ? -1.0f : (sample > 1.0f ? 1.0f : sample);
   const double scaled = (double)clamped * 2147483647.0;

   if (scaled >= 2147483647.0)
   {
      return INT32_MAX;
   }

   if (scaled <= -2147483648.0)
   {
      return INT32_MIN;
   }

   return (int32_t)lrint(scaled);
}

static bool
read_exact(FILE* fp, void* dst, size_t size)
{
   return fread(dst, 1, size, fp) == size;
}

static bool
parse_dsf(FILE* fp, struct hrmp_dsf_info* info)
{
   uint8_t chunk[28];
   uint8_t fmt_header[12];
   uint8_t fmt_body[40];
   uint8_t data_header[12];
   uint64_t metadata_offset;
   uint64_t file_size = 0;
   uint64_t max_bytes;

   memset(info, 0, sizeof(*info));

   if (!read_exact(fp, chunk, sizeof(chunk)))
   {
      fprintf(stderr, "Failed to read DSF header.\n");
      return false;
   }

   if (memcmp(chunk, "DSD ", 4) != 0)
   {
      fprintf(stderr, "Input is not a DSF file.\n");
      return false;
   }

   if (read_le_u64(chunk + 4) < sizeof(chunk))
   {
      fprintf(stderr, "Invalid DSF header size.\n");
      return false;
   }

   metadata_offset = read_le_u64(chunk + 20);

   if (!read_exact(fp, fmt_header, sizeof(fmt_header)))
   {
      fprintf(stderr, "Failed to read DSF fmt header.\n");
      return false;
   }

   if (memcmp(fmt_header, "fmt ", 4) != 0)
   {
      fprintf(stderr, "Missing DSF fmt chunk.\n");
      return false;
   }

   if (read_le_u64(fmt_header + 4) < sizeof(fmt_header) + sizeof(fmt_body))
   {
      fprintf(stderr, "Invalid DSF fmt chunk size.\n");
      return false;
   }

   if (!read_exact(fp, fmt_body, sizeof(fmt_body)))
   {
      fprintf(stderr, "Failed to read DSF fmt body.\n");
      return false;
   }

   info->channels = read_le_u32(fmt_body + 12);
   info->sample_rate = read_le_u32(fmt_body + 16);
   info->bits_per_sample = read_le_u32(fmt_body + 20);
   info->total_samples = read_le_u64(fmt_body + 24);
   info->block_size = read_le_u32(fmt_body + 32);

   if (info->channels == 0 || info->channels > HRMP_DSF_MAX_CHANNELS)
   {
      fprintf(stderr, "Unsupported DSF channel count: %u.\n", info->channels);
      return false;
   }

   if (info->sample_rate == 0 || (info->sample_rate % 32u) != 0u)
   {
      fprintf(stderr, "Unsupported DSF sample rate: %u.\n", info->sample_rate);
      return false;
   }

   if (info->bits_per_sample != 1u && info->bits_per_sample != 8u)
   {
      fprintf(stderr, "Unsupported DSF bits-per-sample value: %u.\n", info->bits_per_sample);
      return false;
   }

   if (info->block_size == 0u)
   {
      fprintf(stderr, "Invalid DSF block size.\n");
      return false;
   }

   if (!read_exact(fp, data_header, sizeof(data_header)))
   {
      fprintf(stderr, "Failed to read DSF data header.\n");
      return false;
   }

   if (memcmp(data_header, "data", 4) != 0 && memcmp(data_header, "DATA", 4) != 0)
   {
      fprintf(stderr, "Missing DSF data chunk.\n");
      return false;
   }

   info->data_offset = (uint64_t)ftello(fp);
   info->data_size = read_le_u64(data_header + 4);

   if (info->data_size < sizeof(data_header))
   {
      fprintf(stderr, "Invalid DSF data chunk size.\n");
      return false;
   }

   info->data_size -= sizeof(data_header);
   max_bytes = info->data_size;

   if (fseeko(fp, 0, SEEK_END) != 0)
   {
      fprintf(stderr, "Failed to seek to end of DSF file.\n");
      return false;
   }

   file_size = (uint64_t)ftello(fp);
   if (fseeko(fp, (off_t)info->data_offset, SEEK_SET) != 0)
   {
      fprintf(stderr, "Failed to seek back to DSF audio data.\n");
      return false;
   }

   if (file_size > info->data_offset)
   {
      const uint64_t available = file_size - info->data_offset;
      if (available < max_bytes)
      {
         max_bytes = available;
      }
   }

   if (metadata_offset != 0u &&
       metadata_offset > info->data_offset &&
       metadata_offset <= file_size &&
       (metadata_offset - info->data_offset) < max_bytes)
   {
      max_bytes = metadata_offset - info->data_offset;
   }

   info->data_size = max_bytes;

   if (info->total_samples == 0u || (info->total_samples % 32u) != 0u)
   {
      fprintf(stderr, "Unsupported DSF sample count.\n");
      return false;
   }

   if (info->channels > 0u)
   {
      const uint64_t expected_bytes = (info->total_samples * info->channels + 7u) / 8u;
      if (info->data_size > expected_bytes)
      {
         info->data_size = expected_bytes;
      }
   }

   return true;
}

static char*
derive_flac_path(const char* input_path)
{
   const char* ext;
   size_t base_len;
   char* output_path;

   ext = strrchr(input_path, '.');
   base_len = ext != NULL ? (size_t)(ext - input_path) : strlen(input_path);

   output_path = malloc(base_len + 6);
   if (output_path == NULL)
   {
      fprintf(stderr, "Out of memory while creating output path.\n");
      return NULL;
   }

   memcpy(output_path, input_path, base_len);
   memcpy(output_path + base_len, ".flac", 6);
   return output_path;
}

int
hrmp_convert_dsf_default_output_path(char* input_path, char* output_path, size_t output_path_size)
{
   char* derived_path;
   size_t required_size;

   if (input_path == NULL || output_path == NULL || output_path_size == 0u)
   {
      return 1;
   }

   derived_path = derive_flac_path(input_path);
   if (derived_path == NULL)
   {
      return 1;
   }

   required_size = strlen(derived_path) + 1u;
   if (required_size > output_path_size)
   {
      free(derived_path);
      return 1;
   }

   memcpy(output_path, derived_path, required_size);
   free(derived_path);
   return 0;
}

int
hrmp_convert_dsf_to_flac(char* input_path, char* output_path)
{
   static bool tables_ready = false;
   static float dsd_stage1_table[HRMP_DSF_STAGE1_BYTES][256];
   static struct hrmp_pcm_stage_filter pcm_stage_filter;

   struct hrmp_channel_state channel_states[HRMP_DSF_MAX_CHANNELS];
   struct hrmp_dsf_info info;
   SNDFILE* out = NULL;
   FILE* in = NULL;
   SF_INFO sfinfo;
   uint8_t* input_block = NULL;
   int32_t* output_block = NULL;
   char* derived_output_path = NULL;
   const char* final_output_path = output_path;
   sf_count_t frames_remaining;
   int exit_code = 1;

   if (input_path == NULL)
   {
      fprintf(stderr, "No input path provided.\n");
      return 1;
   }

   if (!tables_ready)
   {
      init_dsd_stage1_tables(dsd_stage1_table);
      init_pcm_stage_filter(&pcm_stage_filter);
      tables_ready = true;
   }

   if (final_output_path == NULL || final_output_path[0] == '\0')
   {
      derived_output_path = derive_flac_path(input_path);
      if (derived_output_path == NULL)
      {
         return 1;
      }

      final_output_path = derived_output_path;
   }

   in = fopen(input_path, "rb");
   if (in == NULL)
   {
      fprintf(stderr, "Failed to open input file '%s': %s\n", input_path, strerror(errno));
      goto cleanup;
   }

   if (!parse_dsf(in, &info))
   {
      goto cleanup;
   }

   memset(&sfinfo, 0, sizeof(sfinfo));
   sfinfo.samplerate = (int)(info.sample_rate / 32u);
   sfinfo.channels = (int)info.channels;
   sfinfo.format = SF_FORMAT_FLAC | SF_FORMAT_PCM_24;

   if (sfinfo.samplerate <= 0)
   {
      fprintf(stderr, "Invalid output sample rate.\n");
      goto cleanup;
   }

   if (!sf_format_check(&sfinfo))
   {
      fprintf(stderr, "Unsupported FLAC output format for %d channels at %d Hz.\n",
              sfinfo.channels, sfinfo.samplerate);
      goto cleanup;
   }

   out = sf_open(final_output_path, SFM_WRITE, &sfinfo);
   if (out == NULL)
   {
      fprintf(stderr, "Failed to open output file '%s': %s\n",
              final_output_path, sf_strerror(NULL));
      goto cleanup;
   }

   for (uint32_t ch = 0; ch < info.channels; ++ch)
   {
      init_dsd_stage_state(&channel_states[ch].dsd_stage);
      init_pcm_stage_state(&channel_states[ch].pcm_stage_1);
      init_pcm_stage_state(&channel_states[ch].pcm_stage_2);
   }

   input_block = malloc((size_t)info.block_size * info.channels);
   output_block = malloc((((size_t)info.block_size + 3u) / 4u) * info.channels * sizeof(int32_t));

   if (input_block == NULL || output_block == NULL)
   {
      fprintf(stderr, "Out of memory while allocating conversion buffers.\n");
      goto cleanup;
   }

   if (fseeko(in, (off_t)info.data_offset, SEEK_SET) != 0)
   {
      fprintf(stderr, "Failed to seek to DSF audio data.\n");
      goto cleanup;
   }

   frames_remaining = (sf_count_t)(info.total_samples / 32u);

   while (frames_remaining > 0)
   {
      const uint64_t bytes_left = info.data_size > 0 ? info.data_size : 0;
      const size_t chunk_total = (size_t)((bytes_left < (uint64_t)((size_t)info.block_size * info.channels))
                                             ? bytes_left
                                             : (uint64_t)((size_t)info.block_size * info.channels));
      const size_t per_channel_bytes = info.channels == 0 ? 0 : chunk_total / info.channels;
      size_t frames_produced = 0;

      if (chunk_total == 0 || per_channel_bytes == 0)
      {
         break;
      }

      if (!read_exact(in, input_block, chunk_total))
      {
         fprintf(stderr, "Unexpected end of DSF audio data.\n");
         goto cleanup;
      }

      info.data_size -= chunk_total;

      for (size_t i = 0; i < per_channel_bytes; ++i)
      {
         bool have_frame = false;

         for (uint32_t ch = 0; ch < info.channels; ++ch)
         {
            const uint8_t dsf_byte = input_block[(size_t)ch * per_channel_bytes + i];
            float stage1_out;
            float stage2_out;
            float final_out;
            bool stage2_ready;
            bool final_ready;

            stage1_out = process_dsd_stage(&channel_states[ch].dsd_stage, dsd_stage1_table, dsf_byte);
            stage2_ready = process_pcm_stage(&channel_states[ch].pcm_stage_1,
                                             &pcm_stage_filter,
                                             stage1_out,
                                             &stage2_out);
            final_ready = stage2_ready &&
                          process_pcm_stage(&channel_states[ch].pcm_stage_2,
                                            &pcm_stage_filter,
                                            stage2_out,
                                            &final_out);

            if (final_ready)
            {
               output_block[frames_produced * info.channels + ch] = float_to_pcm32(final_out);
               have_frame = true;
            }
         }

         if (have_frame)
         {
            ++frames_produced;
         }
      }

      if ((sf_count_t)frames_produced > frames_remaining)
      {
         frames_produced = (size_t)frames_remaining;
      }

      if (frames_produced > 0)
      {
         const sf_count_t written = sf_writef_int(out, output_block, (sf_count_t)frames_produced);
         if (written != (sf_count_t)frames_produced)
         {
            fprintf(stderr, "Failed while writing FLAC output '%s': %s\n",
                    final_output_path, sf_strerror(out));
            goto cleanup;
         }

         frames_remaining -= written;
      }
   }

   if (frames_remaining != 0)
   {
      fprintf(stderr, "Conversion ended before all PCM frames were generated.\n");
      goto cleanup;
   }

   sf_write_sync(out);

   exit_code = 0;

cleanup:
   if (out != NULL)
   {
      sf_close(out);
   }

   if (in != NULL)
   {
      fclose(in);
   }

   free(output_block);
   free(input_block);
   free(derived_output_path);
   return exit_code;
}
