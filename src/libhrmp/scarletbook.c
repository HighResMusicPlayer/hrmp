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

#include <hrmp.h>
#include <logging.h>
#include <scarletbook.h>
#include <stdint.h>
#include <utils.h>

#include <byteswap.h>
#include <errno.h>
#include <fcntl.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#if !defined(NO_SSE2) && (defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__))
#include <emmintrin.h>
#endif

#define SACD_LSN_SIZE             2048
#define SACD_SAMPLING_FREQUENCY   2822400

#define START_OF_MASTER_TOC       510
#define MASTER_TOC_LEN            10
#define MAX_AREA_TOC_SIZE_LSN     96
#define MAX_LANGUAGE_COUNT        8
#define MAX_CHANNEL_COUNT         6
#define MAX_DST_SIZE              (1024 * 64)
#define FRAME_SIZE_64             4704
#define MAX_PACKET_SIZE           2045
#define SUPPORTED_VERSION_MAJOR   1
#define SUPPORTED_VERSION_MINOR   20

#define MAX_PROCESSING_BLOCK_SIZE 512

#define MAKE_MARKER(a, b, c, d)   ((a) | ((b) << 8) | ((c) << 16) | ((d) << 24))

#define SWAP16(x)                 x = bswap_16(x)
#define SWAP32(x)                 x = bswap_32(x)
#ifndef htole16
#define htole16(x) ((uint16_t)(x))
#endif
#ifndef htole32
#define htole32(x) ((uint32_t)(x))
#endif
#ifndef htole64
#define htole64(x) ((uint64_t)(x))
#endif

#define DSD_MARKER                  (MAKE_MARKER('D', 'S', 'D', ' '))
#define FMT_MARKER                  (MAKE_MARKER('f', 'm', 't', ' '))
#define DATA_MARKER                 (MAKE_MARKER('d', 'a', 't', 'a'))
#define DSF_VERSION                 1
#define FORMAT_ID_DSD               0
#define DSF_HEADER_FOOTER_SIZE      2048
#define SACD_BLOCK_SIZE_PER_CHANNEL 4096
#define SACD_BITS_PER_SAMPLE        1
#define MAX_TRACK_TITLE_LEN         120

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif
#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

#define RESOL               8

#define SIZE_CODEDPREDORDER 7
#define SIZE_PREDCOEF       9

#define AC_BITS             8
#define AC_PROBS            (1 << AC_BITS)
#define AC_HISBITS          6
#define AC_HISMAX           (1 << AC_HISBITS)
#define AC_QSTEP            (SIZE_PREDCOEF - AC_HISBITS)

#define NROFFRICEMETHODS    3
#define NROFPRICEMETHODS    3
#define MAXCPREDORDER       3
#define SIZE_RICEMETHOD     2
#define SIZE_RICEM          3

#define MAXNROF_FSEGS       4
#define MAXNROF_PSEGS       8
#define MIN_FSEG_LEN        1024
#define MIN_PSEG_LEN        32

#define MAX_CHANNELS        6
#define MAX_DSDBITS_INFRAME (588 * 64)
#define MAXNROF_SEGS        8

enum scarletbook_dst_error_codes {
   dst_err_no_error = 0,
   dst_err_negative_bit_allocation,
   dst_err_too_many_segments,
   dst_err_invalid_segment_resolution,
   dst_err_invalid_segment_length,
   dst_err_too_many_tables,
   dst_err_invalid_table_number,
   dst_err_invalid_channel_mapping,
   dst_err_segment_number_mismatch,
   dst_err_invalid_coefficient_coding,
   dst_err_invalid_coefficient_range,
   dst_err_invalid_ptable_coding,
   dst_err_invalid_ptable_range,
   dst_err_invalid_stuffing_pattern,
   dst_err_invalid_arithmetic_code,
   dst_err_arithmetic_decoder,
   dst_err_max_error,
};

enum t_table { filter,
               ptable };

typedef struct
{
   int resolution;
   int segment_len[MAX_CHANNELS][MAXNROF_SEGS];
   int nr_of_segments[MAX_CHANNELS];
   int table4_segment[MAX_CHANNELS][MAXNROF_SEGS];
} scarletbook_segment;

typedef struct
{
   int frame_nr;
   int nr_of_channels;
   int nr_of_filters;
   int nr_of_ptables;
   int fsample_44;
   int pred_order[2 * MAX_CHANNELS];
   int ptable_len[2 * MAX_CHANNELS];
   int16_t** i_coef_a;
   int dst_coded;

   long calc_nr_of_bytes;
   long calc_nr_of_bits;

   int half_prob[MAX_CHANNELS];

   int nr_of_half_bits[MAX_CHANNELS];

   scarletbook_segment f_seg;
   char filter4_bit[MAX_CHANNELS][MAX_DSDBITS_INFRAME];
   scarletbook_segment p_seg;
   char ptable4_bit[MAX_CHANNELS][MAX_DSDBITS_INFRAME];
   int p_same_seg_as_f;
   int p_same_map_as_f;
   int f_same_seg_all_ch;
   int f_same_map_all_ch;
   int p_same_seg_all_ch;
   int p_same_map_all_ch;
   int seg_and_map_bits;
   int max_nr_of_filters;
   int max_nr_of_ptables;
   long max_frame_len;
   long byte_stream_len;
   long bit_stream_len;
   long nr_of_bits_per_ch;
} scarletbook_frame_header;

typedef struct
{
   int* c_pred_order;
   int** c_pred_coef;
   int* coded;
   int* best_method;
   int** m;
   int** data;
   int* data_len;
   int stream_bits;
   int table_type;
} scarletbook_coded_table;

typedef struct
{
   uint8_t* p_dst_data;
   int32_t total_bytes;
   int32_t byte_counter;
   int bit_position;
   uint8_t data_byte;
} scarletbook_str_data;

typedef struct
{
   unsigned int init;
   unsigned int c;
   unsigned int a;
   int cbptr;
} scarletbook_ac_data;

typedef struct
{
   scarletbook_frame_header frame_hdr;
   scarletbook_coded_table str_filter;
   scarletbook_coded_table str_ptable;
   int** p_one;
   uint8_t* a_data;
   int a_data_len;
   scarletbook_str_data s;
   int sse_2;
   int16_t lt_icoef_i[2 * MAX_CHANNELS][16][256] __attribute__((aligned(16)));
   uint8_t lt_status[MAX_CHANNELS][16] __attribute__((aligned(16)));
} scarletbook_ebunch;

static int scarletbook_dst_init_decoder(scarletbook_ebunch* d, int nr_of_channels, int sample_rate);
static int scarletbook_dst_close_decoder(scarletbook_ebunch* d);
static int scarletbook_dst_fram_dst_decode(uint8_t* dst_data, uint8_t* muxed_dsd_data, int frame_size_in_bytes, int frame_cnt, scarletbook_ebunch* d);
static const char* scarletbook_dst_get_error_message(int error);

static int
scarletbook_ccp_calc_init(scarletbook_coded_table* ct)
{
   int retval = 0;
   int i;

   switch (ct->table_type)
   {
      case filter:
         ct->c_pred_order[0] = 1;
         ct->c_pred_coef[0][0] = -8;
         for (i = ct->c_pred_order[0]; i < MAXCPREDORDER; i++)
         {
            ct->c_pred_coef[0][i] = 0;
         }

         ct->c_pred_order[1] = 2;
         ct->c_pred_coef[1][0] = -16;
         ct->c_pred_coef[1][1] = 8;
         for (i = ct->c_pred_order[1]; i < MAXCPREDORDER; i++)
         {
            ct->c_pred_coef[1][i] = 0;
         }

         ct->c_pred_order[2] = 3;
         ct->c_pred_coef[2][0] = -9;
         ct->c_pred_coef[2][1] = -5;
         ct->c_pred_coef[2][2] = 6;
         for (i = ct->c_pred_order[2]; i < MAXCPREDORDER; i++)
         {
            ct->c_pred_coef[2][i] = 0;
         }
#if NROFFRICEMETHODS == 4
         ct->c_pred_order[3] = 1;
         ct->c_pred_coef[3][0] = 8;
         for (i = ct->c_pred_order[3]; i < MAXCPREDORDER; i++)
         {
            ct->c_pred_coef[3][i] = 0;
         }
#endif
         break;
      case ptable:
         ct->c_pred_order[0] = 1;
         ct->c_pred_coef[0][0] = -8;
         for (i = ct->c_pred_order[0]; i < MAXCPREDORDER; i++)
         {
            ct->c_pred_coef[0][i] = 0;
         }

         ct->c_pred_order[1] = 2;
         ct->c_pred_coef[1][0] = -16;
         ct->c_pred_coef[1][1] = 8;
         for (i = ct->c_pred_order[1]; i < MAXCPREDORDER; i++)
         {
            ct->c_pred_coef[1][i] = 0;
         }

         ct->c_pred_order[2] = 3;
         ct->c_pred_coef[2][0] = -24;
         ct->c_pred_coef[2][1] = 24;
         ct->c_pred_coef[2][2] = -8;
         for (i = ct->c_pred_order[2]; i < MAXCPREDORDER; i++)
         {
            ct->c_pred_coef[2][i] = 0;
         }
         break;
      default:
         retval = 1;
   }

   return (retval);
}

static int scarletbook_get_bits(scarletbook_str_data* s, long* outword, int out_bitptr);

static int
scarletbook_reset_reading_index(scarletbook_str_data* sd)
{
   int hr = 0;

   sd->bit_position = 0;
   sd->byte_counter = 0;
   sd->data_byte = 0;

   return (hr);
}

static int
scarletbook_create_buffer(scarletbook_str_data* sd, int32_t size)
{
   int hr = 0;

   sd->total_bytes = size;

   if (sd->p_dst_data != NULL)
   {
      free(sd->p_dst_data);
      sd->p_dst_data = NULL;
   }

   sd->p_dst_data = (uint8_t*)malloc(size);

   if (sd->p_dst_data == NULL)
   {
      sd->total_bytes = 0;
      hr = -1;
   }

   scarletbook_reset_reading_index(sd);

   return (hr);
}

static int
scarletbook_delete_buffer(scarletbook_str_data* sd)
{
   int hr = 0;

   sd->total_bytes = 0;

   if (sd->p_dst_data != NULL)
   {
      free(sd->p_dst_data);
      sd->p_dst_data = NULL;
   }

   scarletbook_reset_reading_index(sd);

   return (hr);
}

static int
scarletbook_fill_buffer(scarletbook_str_data* sd, uint8_t* p_buf, int32_t size)
{
   int hr = 0;
   int32_t cnt;

   scarletbook_create_buffer(sd, size);

   for (cnt = 0; cnt < size; cnt++)
   {
      sd->p_dst_data[cnt] = p_buf[cnt];
   }

   scarletbook_reset_reading_index(sd);

   return (hr);
}

static int
scarletbook_fio_bit_get_chr_unsigned(scarletbook_str_data* sd, int len, unsigned char* x)
{
   int return_value;
   long tmp = 0L;

   return_value = -1;
   if (len > 0)
   {
      return_value = scarletbook_get_bits(sd, &tmp, len);
      *x = (unsigned char)tmp;
   }
   else if (len == 0)
   {
      *x = 0;
      return_value = 0;
   }
   else
   {
   }
   return return_value;
}

static int
scarletbook_fio_bit_get_int_unsigned(scarletbook_str_data* sd, int len, int* x)
{
   int return_value;
   long tmp = 0L;

   return_value = -1;
   if (len > 0)
   {
      return_value = scarletbook_get_bits(sd, &tmp, len);
      *x = (int)tmp;
   }
   else if (len == 0)
   {
      *x = 0;
      return_value = 0;
   }
   else
   {
   }
   return return_value;
}

static int
scarletbook_fio_bit_get_short_signed(scarletbook_str_data* sd, int len, short* x)
{
   int return_value;
   long tmp = 0L;

   return_value = -1;
   if (len > 0)
   {
      return_value = scarletbook_get_bits(sd, &tmp, len);
      *x = (short)tmp;

      if (*x >= (1 << (len - 1)))
      {
         *x -= (1 << len);
      }
   }
   else if (len == 0)
   {
      *x = 0;
      return_value = 0;
   }
   else
   {
   }
   return return_value;
}

static int masks[] = {0, 1, 3, 7, 0xf, 0x1f, 0x3f, 0x7f, 0xff};

static int
scarletbook_get_bits(scarletbook_str_data* sd, long* outword, int out_bitptr)
{
   if (out_bitptr == 1)
   {
      if (sd->bit_position == 0)
      {
         sd->data_byte = sd->p_dst_data[sd->byte_counter++];
         if (sd->byte_counter > sd->total_bytes)
         {
            return (-1);
         }
         sd->bit_position = 8;
      }

      sd->bit_position--;
      *outword = (sd->data_byte >> sd->bit_position) & 1;

      return 0;
   }

   *outword = 0;
   while (out_bitptr > 0)
   {
      int thisbits, mask, shift;

      if (!sd->bit_position)
      {
         sd->data_byte = sd->p_dst_data[sd->byte_counter++];
         if (sd->byte_counter > sd->total_bytes)
         {
            return (-1);
         }
         sd->bit_position = 8;
      }

      thisbits = MIN(sd->bit_position, out_bitptr);
      shift = (sd->bit_position - thisbits);
      mask = masks[thisbits] << shift;

      shift = (out_bitptr - thisbits) - shift;
      if (shift <= 0)
         *outword |= ((sd->data_byte & mask) >> -shift);
      else
         *outword |= ((sd->data_byte & mask) << shift);

      out_bitptr -= thisbits;
      sd->bit_position -= thisbits;
   }

   return 0;
}

static int
scarletbook_get_in_bitcount(scarletbook_str_data* sd)
{
   return sd->byte_counter * 8 - sd->bit_position;
}

static void*
scarletbook_memory_allocate(int nr_of_elements, int size_of_element)
{
   void* array;
#if defined(__arm__) || defined(__aarch64__)
   if ((array = malloc(nr_of_elements * size_of_element)) == NULL)
   {
   }
#else
   if ((array = _mm_malloc(nr_of_elements * size_of_element, 16)) == NULL)
   {
   }
#endif
   return array;
}

static void
scarletbook_memory_free(void* array)
{
#if defined(__arm__) || defined(__aarch64__)
   free(array);
#else
   _mm_free(array);
#endif
}

static void*
scarletbook_allocate_array(int dim, int element_size, ...)
{
   void*** a;
   void* AA;
   va_list ap;
   int i;
   int j;
   int n;
   int* size;

   size = scarletbook_memory_allocate(dim, sizeof(*size));
   va_start(ap, element_size);
   for (i = 0; i < dim; i++)
   {
      size[i] = va_arg(ap, int);
   }
   va_end(ap);
   a = scarletbook_memory_allocate(dim, sizeof(**a));
   for (n = 1, i = 0; i < dim - 1; i++)
   {
      n *= size[i];
      a[i] = scarletbook_memory_allocate(n, sizeof(void*));
   }
   n *= size[i];
   a[i] = scarletbook_memory_allocate(n, element_size);
   for (n = 1, i = 0; i < dim - 1; i++)
   {
      n *= size[i];
      for (j = 0; j < n; j++)
      {
         a[i][j] = &((char*)a[i + 1])[j * size[i + 1] * element_size];
      }
   }
   AA = a[0];
   scarletbook_memory_free(a);
   scarletbook_memory_free(size);
   return AA;
}

static void
scarletbook_free_dec_memory(scarletbook_ebunch* d)
{
   scarletbook_delete_buffer(&d->s);
   scarletbook_memory_free(d->frame_hdr.i_coef_a[0]);
   scarletbook_memory_free(d->frame_hdr.i_coef_a);

   scarletbook_memory_free(d->str_filter.m[0]);
   scarletbook_memory_free(d->str_filter.m);
   scarletbook_memory_free(d->str_filter.data[0]);
   scarletbook_memory_free(d->str_filter.data);
   scarletbook_memory_free(d->str_filter.c_pred_order);
   scarletbook_memory_free(d->str_filter.c_pred_coef[0]);
   scarletbook_memory_free(d->str_filter.c_pred_coef);
   scarletbook_memory_free(d->str_filter.coded);
   scarletbook_memory_free(d->str_filter.best_method);
   scarletbook_memory_free(d->str_filter.data_len);
   scarletbook_memory_free(d->str_ptable.m[0]);
   scarletbook_memory_free(d->str_ptable.m);
   scarletbook_memory_free(d->str_ptable.data[0]);
   scarletbook_memory_free(d->str_ptable.data);
   scarletbook_memory_free(d->str_ptable.c_pred_order);
   scarletbook_memory_free(d->str_ptable.c_pred_coef[0]);
   scarletbook_memory_free(d->str_ptable.c_pred_coef);
   scarletbook_memory_free(d->str_ptable.coded);
   scarletbook_memory_free(d->str_ptable.best_method);
   scarletbook_memory_free(d->str_ptable.data_len);
   scarletbook_memory_free(d->p_one[0]);
   scarletbook_memory_free(d->p_one);
   scarletbook_memory_free(d->a_data);
}

static void
scarletbook_allocate_dec_memory(scarletbook_ebunch* d)
{
   d->frame_hdr.i_coef_a = scarletbook_allocate_array(2, sizeof(**d->frame_hdr.i_coef_a), d->frame_hdr.max_nr_of_filters, (1 << SIZE_CODEDPREDORDER));

   d->str_filter.coded = scarletbook_memory_allocate(d->frame_hdr.max_nr_of_filters, sizeof(*d->str_filter.coded));
   d->str_filter.best_method = scarletbook_memory_allocate(d->frame_hdr.max_nr_of_filters, sizeof(*d->str_filter.best_method));
   d->str_filter.m = scarletbook_allocate_array(2, sizeof(**d->str_filter.m), d->frame_hdr.max_nr_of_filters, NROFFRICEMETHODS);
   d->str_filter.data = scarletbook_allocate_array(2, sizeof(**d->str_filter.data), d->frame_hdr.max_nr_of_filters, (1 << SIZE_CODEDPREDORDER) * SIZE_PREDCOEF);
   d->str_filter.data_len = scarletbook_memory_allocate(d->frame_hdr.max_nr_of_filters, sizeof(*d->str_filter.data_len));
   d->str_filter.c_pred_order = scarletbook_memory_allocate(NROFFRICEMETHODS, sizeof(*d->str_filter.c_pred_order));
   d->str_filter.c_pred_coef = scarletbook_allocate_array(2, sizeof(**d->str_filter.c_pred_coef), NROFFRICEMETHODS, MAXCPREDORDER);
   d->str_ptable.coded = scarletbook_memory_allocate(d->frame_hdr.max_nr_of_ptables, sizeof(*d->str_ptable.coded));
   d->str_ptable.best_method = scarletbook_memory_allocate(d->frame_hdr.max_nr_of_ptables, sizeof(*d->str_ptable.best_method));
   d->str_ptable.m = scarletbook_allocate_array(2, sizeof(**d->str_ptable.m), d->frame_hdr.max_nr_of_ptables, NROFPRICEMETHODS);
   d->str_ptable.data = scarletbook_allocate_array(2, sizeof(**d->str_ptable.data), d->frame_hdr.max_nr_of_ptables, AC_BITS * AC_HISMAX);
   d->str_ptable.data_len = scarletbook_memory_allocate(d->frame_hdr.max_nr_of_ptables, sizeof(*d->str_ptable.data_len));
   d->str_ptable.c_pred_order = scarletbook_memory_allocate(NROFPRICEMETHODS, sizeof(*d->str_ptable.c_pred_order));
   d->str_ptable.c_pred_coef = scarletbook_allocate_array(2, sizeof(**d->str_ptable.c_pred_coef), NROFPRICEMETHODS, MAXCPREDORDER);
   d->p_one = scarletbook_allocate_array(2, sizeof(**d->p_one), d->frame_hdr.max_nr_of_ptables, AC_HISMAX);
   d->a_data = scarletbook_memory_allocate(d->frame_hdr.bit_stream_len, sizeof(*d->a_data));
}

static int
scarletbook_dst_init_decoder(scarletbook_ebunch* d, int nr_of_channels, int sample_rate)
{
   int retval = 0;

   memset(d, 0, sizeof(scarletbook_ebunch));

   d->frame_hdr.nr_of_channels = nr_of_channels;
   d->frame_hdr.max_frame_len = (588 * sample_rate / 8);
   d->frame_hdr.byte_stream_len = d->frame_hdr.max_frame_len * d->frame_hdr.nr_of_channels;
   d->frame_hdr.bit_stream_len = d->frame_hdr.byte_stream_len * RESOL;
   d->frame_hdr.nr_of_bits_per_ch = d->frame_hdr.max_frame_len * RESOL;
   d->frame_hdr.max_nr_of_filters = 2 * d->frame_hdr.nr_of_channels;
   d->frame_hdr.max_nr_of_ptables = 2 * d->frame_hdr.nr_of_channels;

   d->frame_hdr.frame_nr = 0;
   d->str_filter.table_type = filter;
   d->str_ptable.table_type = ptable;

   if (retval == 0)
   {
      scarletbook_allocate_dec_memory(d);
   }

   if (retval == 0)
   {
      retval = scarletbook_ccp_calc_init(&d->str_filter);
   }
   if (retval == 0)
   {
      retval = scarletbook_ccp_calc_init(&d->str_ptable);
   }

   d->sse_2 = 0;
#if !defined(NO_SSE2) && (defined(_M_IX86) || defined(_M_X64) || defined(__i386__) || defined(__x86_64__))
   {
      int cpu_info[4];
#if defined(__i386__) || defined(__x86_64__)
#define cpuid(type, a, b, c, d) \
   __asm__("cpuid" : "=a"(a), "=b"(b), "=c"(c), "=d"(d) : "a"(type));

      cpuid(1, cpu_info[0], cpu_info[1], cpu_info[2], cpu_info[3]);
#else
      __cpuid(cpu_info, 1);
#endif

      d->sse_2 = (cpu_info[3] & (1L << 26)) ? 1 : 0;
   }
#endif

   return (retval);
}

static int
scarletbook_dst_close_decoder(scarletbook_ebunch* d)
{
   scarletbook_free_dec_memory(d);

   return 0;
}

static void scarletbook_read_dsd_frame(scarletbook_str_data* sd,
                                       long max_frame_len,
                                       int nr_of_channels,
                                       unsigned char* dsd_frame);

static int scarletbook_rice_decode(scarletbook_str_data* sd, int m);
static int scarletbook_log_2_round_up(long x);

static int scarletbook_read_table_segment_data(scarletbook_str_data* sd,
                                               int nr_of_channels,
                                               int frame_len,
                                               int max_nr_of_segs,
                                               int min_seg_len,
                                               scarletbook_segment* s,
                                               int* same_seg_all_ch);
static int scarletbook_copy_segment_data(scarletbook_frame_header* fh);
static int scarletbook_read_segment_data(scarletbook_str_data* sd, scarletbook_frame_header* fh);
static int scarletbook_read_table_mapping_data(scarletbook_str_data* sd, int nr_of_channels,
                                               int max_nr_of_tables,
                                               scarletbook_segment* s,
                                               int* nr_of_tables,
                                               int* same_map_all_ch);
static int scarletbook_copy_mapping_data(scarletbook_frame_header* fh);
static int scarletbook_read_mapping_data(scarletbook_str_data* sd, scarletbook_frame_header* fh);
static int scarletbook_read_filter_coef_sets(scarletbook_str_data* sd, int nr_of_channels, scarletbook_frame_header* fh, scarletbook_coded_table* cf);
static int scarletbook_read_probability_tables(scarletbook_str_data* sd, scarletbook_frame_header* fh, scarletbook_coded_table* cp, int** p_one);
static void scarletbook_read_arithmetic_coded_data(scarletbook_str_data* sd, int a_data_len, unsigned char* a_data);

static void
scarletbook_read_dsd_frame(scarletbook_str_data* s,
                           long max_frame_len,
                           int nr_of_channels,
                           unsigned char* dsd_frame)
{
   int byte_nr;
   int max = (max_frame_len * nr_of_channels);

   for (byte_nr = 0; byte_nr < max; byte_nr++)
      scarletbook_fio_bit_get_chr_unsigned(s, 8, &dsd_frame[byte_nr]);
}

static int
scarletbook_rice_decode(scarletbook_str_data* s, int m)
{
   int lsbs;
   int nr;
   int rl_bit;
   int run_length;
   int sign;

   run_length = 0;
   do
   {
      scarletbook_fio_bit_get_int_unsigned(s, 1, &rl_bit);
      run_length += (1 - rl_bit);
   }
   while (rl_bit == 0);

   scarletbook_fio_bit_get_int_unsigned(s, m, &lsbs);

   nr = (run_length << m) + lsbs;

   if (nr != 0)
   {
      scarletbook_fio_bit_get_int_unsigned(s, 1, &sign);
      if (sign == 1)
      {
         nr = -nr;
      }
   }

   return nr;
}

static int
scarletbook_log_2_round_up(long x)
{
   int y = 0;

   while (x >= (1 << y))
      y++;

   return y;
}

static int
scarletbook_read_table_segment_data(scarletbook_str_data* sd,
                                    int nr_of_channels,
                                    int frame_len,
                                    int max_nr_of_segs,
                                    int min_seg_len,
                                    scarletbook_segment* s,
                                    int* same_seg_all_ch)
{
   int ch_nr = 0;
   int defined_bits = 0;
   int resol_read = 0;
   int seg_nr = 0;
   int max_seg_size;
   int nr_of_bits;
   int end_of_channel;

   max_seg_size = frame_len - min_seg_len / 8;

   if (scarletbook_fio_bit_get_int_unsigned(sd, 1, same_seg_all_ch))
      return dst_err_negative_bit_allocation;
   if (*same_seg_all_ch == 1)
   {
      if (scarletbook_fio_bit_get_int_unsigned(sd, 1, &end_of_channel))
         return dst_err_negative_bit_allocation;

      while (end_of_channel == 0)
      {
         if (seg_nr >= max_nr_of_segs)
            return dst_err_too_many_segments;

         if (resol_read == 0)
         {
            nr_of_bits = scarletbook_log_2_round_up(frame_len - min_seg_len / 8);
            if (scarletbook_fio_bit_get_int_unsigned(sd, nr_of_bits, &s->resolution))
               return dst_err_negative_bit_allocation;

            if ((s->resolution == 0) || (s->resolution > frame_len - min_seg_len / 8))
               return dst_err_invalid_segment_resolution;

            resol_read = 1;
         }

         nr_of_bits = scarletbook_log_2_round_up(max_seg_size / s->resolution);
         if (scarletbook_fio_bit_get_int_unsigned(sd, nr_of_bits, &s->segment_len[0][seg_nr]))
            return dst_err_negative_bit_allocation;

         if ((s->resolution * 8 * s->segment_len[0][seg_nr] < min_seg_len) ||
             (s->resolution * 8 * s->segment_len[0][seg_nr] > frame_len * 8 - defined_bits - min_seg_len))
         {
            return dst_err_invalid_segment_length;
         }

         defined_bits += s->resolution * 8 * s->segment_len[0][seg_nr];
         max_seg_size -= s->resolution * s->segment_len[0][seg_nr];
         seg_nr++;

         if (scarletbook_fio_bit_get_int_unsigned(sd, 1, &end_of_channel))
            return dst_err_negative_bit_allocation;
      }
      s->nr_of_segments[0] = seg_nr + 1;
      s->segment_len[0][seg_nr] = 0;

      for (ch_nr = 1; ch_nr < nr_of_channels; ch_nr++)
      {
         s->nr_of_segments[ch_nr] = s->nr_of_segments[0];
         for (seg_nr = 0; seg_nr < s->nr_of_segments[0]; seg_nr++)
            s->segment_len[ch_nr][seg_nr] = s->segment_len[0][seg_nr];
      }
   }
   else
   {
      while (ch_nr < nr_of_channels)
      {
         if (seg_nr >= max_nr_of_segs)
            return dst_err_too_many_segments;

         if (scarletbook_fio_bit_get_int_unsigned(sd, 1, &end_of_channel))
            return dst_err_negative_bit_allocation;

         if (end_of_channel == 0)
         {
            if (resol_read == 0)
            {
               nr_of_bits = scarletbook_log_2_round_up(frame_len - min_seg_len / 8);
               if (scarletbook_fio_bit_get_int_unsigned(sd, nr_of_bits, &s->resolution))
                  return dst_err_negative_bit_allocation;

               if ((s->resolution == 0) || (s->resolution > frame_len - min_seg_len / 8))
                  return dst_err_invalid_segment_resolution;

               resol_read = 1;
            }

            nr_of_bits = scarletbook_log_2_round_up(max_seg_size / s->resolution);
            if (scarletbook_fio_bit_get_int_unsigned(sd, nr_of_bits, &s->segment_len[ch_nr][seg_nr]))
               return dst_err_negative_bit_allocation;

            if ((s->resolution * 8 * s->segment_len[ch_nr][seg_nr] < min_seg_len) ||
                (s->resolution * 8 * s->segment_len[ch_nr][seg_nr] > frame_len * 8 - defined_bits - min_seg_len))
            {
               return dst_err_invalid_segment_length;
            }

            defined_bits += s->resolution * 8 * s->segment_len[ch_nr][seg_nr];
            max_seg_size -= s->resolution * s->segment_len[ch_nr][seg_nr];
            seg_nr++;
         }
         else
         {
            s->nr_of_segments[ch_nr] = seg_nr + 1;
            s->segment_len[ch_nr][seg_nr] = 0;
            seg_nr = 0;
            defined_bits = 0;
            max_seg_size = frame_len - min_seg_len / 8;
            ch_nr++;
         }
      }
   }

   if (resol_read == 0)
      s->resolution = 1;

   return dst_err_no_error;
}

static int
scarletbook_copy_segment_data(scarletbook_frame_header* fh)
{
   int ch_nr;
   int seg_nr;

   int *dst = fh->p_seg.nr_of_segments, *src = fh->f_seg.nr_of_segments;

   fh->p_seg.resolution = fh->f_seg.resolution;
   fh->p_same_seg_all_ch = 1;
   for (ch_nr = 0; ch_nr < fh->nr_of_channels; ch_nr++)
   {
      dst[ch_nr] = src[ch_nr];
      if (dst[ch_nr] > MAXNROF_PSEGS)
         return dst_err_too_many_segments;

      if (dst[ch_nr] != dst[0])
         fh->p_same_seg_all_ch = 0;

      for (seg_nr = 0; seg_nr < dst[ch_nr]; seg_nr++)
      {
         int *lendst = fh->p_seg.segment_len[ch_nr], *lensrc = fh->f_seg.segment_len[ch_nr];

         lendst[seg_nr] = lensrc[seg_nr];
         if ((lendst[seg_nr] != 0) && (fh->p_seg.resolution * 8 * lendst[seg_nr] < MIN_PSEG_LEN))
            return dst_err_invalid_segment_length;

         if (lendst[seg_nr] != fh->p_seg.segment_len[0][seg_nr])
            fh->p_same_seg_all_ch = 0;
      }
   }

   return dst_err_no_error;
}

static int
scarletbook_read_segment_data(scarletbook_str_data* sd, scarletbook_frame_header* fh)
{
   int error;

   if (scarletbook_fio_bit_get_int_unsigned(sd, 1, &fh->p_same_seg_as_f))
      return dst_err_negative_bit_allocation;

   if ((error = scarletbook_read_table_segment_data(sd,
                                                    fh->nr_of_channels,
                                                    fh->max_frame_len,
                                                    MAXNROF_FSEGS,
                                                    MIN_FSEG_LEN,
                                                    &fh->f_seg,
                                                    &fh->f_same_seg_all_ch)) != 0)
   {
      return error;
   }

   if (fh->p_same_seg_as_f == 1)
      return scarletbook_copy_segment_data(fh);
   else
   {
      return scarletbook_read_table_segment_data(sd, fh->nr_of_channels,
                                                 fh->max_frame_len,
                                                 MAXNROF_PSEGS,
                                                 MIN_PSEG_LEN,
                                                 &fh->p_seg,
                                                 &fh->p_same_seg_all_ch);
   }

   return dst_err_no_error;
}

static int
scarletbook_read_table_mapping_data(scarletbook_str_data* sd,
                                    int nr_of_channels,
                                    int max_nr_of_tables,
                                    scarletbook_segment* s,
                                    int* nr_of_tables,
                                    int* same_map_all_ch)
{
   int ch_nr;
   int count_tables = 1;
   int nr_of_bits = 1;
   int seg_nr;

   s->table4_segment[0][0] = 0;

   if (scarletbook_fio_bit_get_int_unsigned(sd, 1, same_map_all_ch))
      return dst_err_negative_bit_allocation;

   if (*same_map_all_ch == 1)
   {
      for (seg_nr = 1; seg_nr < s->nr_of_segments[0]; seg_nr++)
      {
         nr_of_bits = scarletbook_log_2_round_up(count_tables);
         if (scarletbook_fio_bit_get_int_unsigned(sd, nr_of_bits, &s->table4_segment[0][seg_nr]))
            return dst_err_negative_bit_allocation;

         if (s->table4_segment[0][seg_nr] == count_tables)
            count_tables++;
         else if (s->table4_segment[0][seg_nr] > count_tables)
            return dst_err_invalid_table_number;
      }
      for (ch_nr = 1; ch_nr < nr_of_channels; ch_nr++)
      {
         if (s->nr_of_segments[ch_nr] != s->nr_of_segments[0])
            return dst_err_invalid_channel_mapping;

         for (seg_nr = 0; seg_nr < s->nr_of_segments[0]; seg_nr++)
            s->table4_segment[ch_nr][seg_nr] = s->table4_segment[0][seg_nr];
      }
   }
   else
   {
      for (ch_nr = 0; ch_nr < nr_of_channels; ch_nr++)
      {
         for (seg_nr = 0; seg_nr < s->nr_of_segments[ch_nr]; seg_nr++)
         {
            if ((ch_nr != 0) || (seg_nr != 0))
            {
               nr_of_bits = scarletbook_log_2_round_up(count_tables);
               if (scarletbook_fio_bit_get_int_unsigned(sd, nr_of_bits, &s->table4_segment[ch_nr][seg_nr]))
                  return dst_err_negative_bit_allocation;

               if (s->table4_segment[ch_nr][seg_nr] == count_tables)
                  count_tables++;
               else if (s->table4_segment[ch_nr][seg_nr] > count_tables)
                  return dst_err_invalid_table_number;
            }
         }
      }
   }

   if (count_tables > max_nr_of_tables)
      return dst_err_too_many_tables;
   *nr_of_tables = count_tables;

   return dst_err_no_error;
}

static int
scarletbook_copy_mapping_data(scarletbook_frame_header* fh)
{
   int ch_nr;
   int seg_nr;

   fh->p_same_map_all_ch = 1;
   for (ch_nr = 0; ch_nr < fh->nr_of_channels; ch_nr++)
   {
      if (fh->p_seg.nr_of_segments[ch_nr] == fh->f_seg.nr_of_segments[ch_nr])
      {
         for (seg_nr = 0; seg_nr < fh->f_seg.nr_of_segments[ch_nr]; seg_nr++)
         {
            fh->p_seg.table4_segment[ch_nr][seg_nr] = fh->f_seg.table4_segment[ch_nr][seg_nr];
            if (fh->p_seg.table4_segment[ch_nr][seg_nr] != fh->p_seg.table4_segment[0][seg_nr])
               fh->p_same_map_all_ch = 0;
         }
      }
      else
         return dst_err_segment_number_mismatch;
   }

   fh->nr_of_ptables = fh->nr_of_filters;
   if (fh->nr_of_ptables > fh->max_nr_of_ptables)
      return dst_err_too_many_tables;

   return dst_err_no_error;
}

static int
scarletbook_read_mapping_data(scarletbook_str_data* sd, scarletbook_frame_header* fh)
{
   int j, error;

   if (scarletbook_fio_bit_get_int_unsigned(sd, 1, &fh->p_same_map_as_f))
      return dst_err_negative_bit_allocation;

   if ((error = scarletbook_read_table_mapping_data(sd, fh->nr_of_channels, fh->max_nr_of_filters, &fh->f_seg, &fh->nr_of_filters, &fh->f_same_map_all_ch)) != 0)
      return error;

   if (fh->p_same_map_as_f == 1)
   {
      if ((error = scarletbook_copy_mapping_data(fh)) != 0)
         return error;
   }
   else
   {
      if ((error = scarletbook_read_table_mapping_data(sd, fh->nr_of_channels, fh->max_nr_of_ptables, &fh->p_seg, &fh->nr_of_ptables, &fh->p_same_map_all_ch)) != 0)
         return error;
   }

   for (j = 0; j < fh->nr_of_channels; j++)
   {
      if (scarletbook_fio_bit_get_int_unsigned(sd, 1, &fh->half_prob[j]))
         return dst_err_negative_bit_allocation;
   }

   return dst_err_no_error;
}

static int
scarletbook_read_filter_coef_sets(scarletbook_str_data* sd,
                                  int nr_of_channels,
                                  scarletbook_frame_header* fh,
                                  scarletbook_coded_table* cf)
{
   int c;
   int ch_nr;
   int coef_nr;
   int filter_nr;
   int tap_nr;
   int x;

   for (filter_nr = 0; filter_nr < fh->nr_of_filters; filter_nr++)
   {
      if (scarletbook_fio_bit_get_int_unsigned(sd, SIZE_CODEDPREDORDER, &fh->pred_order[filter_nr]))
         return dst_err_negative_bit_allocation;

      fh->pred_order[filter_nr]++;
      if (scarletbook_fio_bit_get_int_unsigned(sd, 1, &cf->coded[filter_nr]))
         return dst_err_negative_bit_allocation;

      if (cf->coded[filter_nr] == 0)
      {
         cf->best_method[filter_nr] = -1;
         for (coef_nr = 0; coef_nr < fh->pred_order[filter_nr]; coef_nr++)
         {
            if (scarletbook_fio_bit_get_short_signed(sd, SIZE_PREDCOEF, &fh->i_coef_a[filter_nr][coef_nr]))
               return dst_err_negative_bit_allocation;
         }
      }
      else
      {
         int bestmethod;

         if (scarletbook_fio_bit_get_int_unsigned(sd, SIZE_RICEMETHOD, &cf->best_method[filter_nr]))
            return dst_err_negative_bit_allocation;

         bestmethod = cf->best_method[filter_nr];
         if (cf->c_pred_order[bestmethod] >= fh->pred_order[filter_nr])
            return dst_err_invalid_coefficient_coding;

         for (coef_nr = 0; coef_nr < cf->c_pred_order[bestmethod]; coef_nr++)
         {
            if (scarletbook_fio_bit_get_short_signed(sd, SIZE_PREDCOEF, &fh->i_coef_a[filter_nr][coef_nr]))
               return dst_err_negative_bit_allocation;
         }

         if (scarletbook_fio_bit_get_int_unsigned(sd, SIZE_RICEM, &cf->m[filter_nr][bestmethod]))
            return dst_err_negative_bit_allocation;

         for (coef_nr = cf->c_pred_order[bestmethod]; coef_nr < fh->pred_order[filter_nr]; coef_nr++)
         {
            for (tap_nr = 0, x = 0; tap_nr < cf->c_pred_order[bestmethod]; tap_nr++)
               x += cf->c_pred_coef[bestmethod][tap_nr] * fh->i_coef_a[filter_nr][coef_nr - tap_nr - 1];

            if (x >= 0)
               c = scarletbook_rice_decode(sd, cf->m[filter_nr][bestmethod]) - (x + 4) / 8;
            else
               c = scarletbook_rice_decode(sd, cf->m[filter_nr][bestmethod]) + (-x + 3) / 8;

            if ((c < -(1 << (SIZE_PREDCOEF - 1))) || (c >= (1 << (SIZE_PREDCOEF - 1))))
               return dst_err_invalid_coefficient_range;
            else
               fh->i_coef_a[filter_nr][coef_nr] = (int16_t)c;
         }
      }

      memset(&fh->i_coef_a[filter_nr][coef_nr], 0, ((1 << SIZE_CODEDPREDORDER) - coef_nr) * sizeof(**fh->i_coef_a));
   }

   for (ch_nr = 0; ch_nr < nr_of_channels; ch_nr++)
      fh->nr_of_half_bits[ch_nr] = fh->pred_order[fh->f_seg.table4_segment[ch_nr][0]];

   return dst_err_no_error;
}

static int
scarletbook_read_probability_tables(scarletbook_str_data* sd,
                                    scarletbook_frame_header* fh,
                                    scarletbook_coded_table* cp,
                                    int** p_one)
{
   int c;
   int entry_nr;
   int ptable_nr;
   int tap_nr;
   int x;

   for (ptable_nr = 0; ptable_nr < fh->nr_of_ptables; ptable_nr++)
   {
      if (scarletbook_fio_bit_get_int_unsigned(sd, AC_HISBITS, &fh->ptable_len[ptable_nr]))
         return dst_err_negative_bit_allocation;

      fh->ptable_len[ptable_nr]++;
      if (fh->ptable_len[ptable_nr] > 1)
      {
         if (scarletbook_fio_bit_get_int_unsigned(sd, 1, &cp->coded[ptable_nr]))
            return dst_err_negative_bit_allocation;

         if (cp->coded[ptable_nr] == 0)
         {
            cp->best_method[ptable_nr] = -1;
            for (entry_nr = 0; entry_nr < fh->ptable_len[ptable_nr]; entry_nr++)
            {
               if (scarletbook_fio_bit_get_int_unsigned(sd, AC_BITS - 1, &p_one[ptable_nr][entry_nr]))
                  return dst_err_negative_bit_allocation;
               p_one[ptable_nr][entry_nr]++;
            }
         }
         else
         {
            int bestmethod;

            if (scarletbook_fio_bit_get_int_unsigned(sd, SIZE_RICEMETHOD, &cp->best_method[ptable_nr]))
               return dst_err_negative_bit_allocation;

            bestmethod = cp->best_method[ptable_nr];
            if (cp->c_pred_order[bestmethod] >= fh->ptable_len[ptable_nr])
               return dst_err_invalid_ptable_coding;

            for (entry_nr = 0; entry_nr < cp->c_pred_order[bestmethod]; entry_nr++)
            {
               if (scarletbook_fio_bit_get_int_unsigned(sd, AC_BITS - 1, &p_one[ptable_nr][entry_nr]))
                  return dst_err_negative_bit_allocation;

               p_one[ptable_nr][entry_nr]++;
            }

            if (scarletbook_fio_bit_get_int_unsigned(sd, SIZE_RICEM, &cp->m[ptable_nr][bestmethod]))
               return dst_err_negative_bit_allocation;

            for (entry_nr = cp->c_pred_order[bestmethod]; entry_nr < fh->ptable_len[ptable_nr]; entry_nr++)
            {
               if (entry_nr < 0 || entry_nr > AC_HISMAX)
                  return dst_err_invalid_ptable_range;

               for (tap_nr = 0, x = 0; tap_nr < cp->c_pred_order[bestmethod]; tap_nr++)
                  x += cp->c_pred_coef[bestmethod][tap_nr] * p_one[ptable_nr][entry_nr - tap_nr - 1];

               if (x >= 0)
                  c = scarletbook_rice_decode(sd, cp->m[ptable_nr][bestmethod]) - (x + 4) / 8;
               else
                  c = scarletbook_rice_decode(sd, cp->m[ptable_nr][bestmethod]) + (-x + 3) / 8;

               if ((c < 1) || (c > (1 << (AC_BITS - 1))))
                  return dst_err_invalid_ptable_range;
               else
                  p_one[ptable_nr][entry_nr] = c;
            }
         }
      }
      else
      {
         p_one[ptable_nr][0] = 128;
         cp->best_method[ptable_nr] = -1;
      }
   }

   return dst_err_no_error;
}

static const int spread[16] = {
   0x00000000, 0x01000000, 0x00010000, 0x01010000,
   0x00000100, 0x01000100, 0x00010100, 0x01010100,
   0x00000001, 0x01000001, 0x00010001, 0x01010001,
   0x00000101, 0x01000101, 0x00010101, 0x01010101};

static void
scarletbook_read_arithmetic_coded_data(scarletbook_str_data* sd,
                                       int a_data_len,
                                       unsigned char* a_data)
{
   int j;
   int val;

   for (j = 0; j < a_data_len - 31; j += 32)
   {
      scarletbook_fio_bit_get_int_unsigned(sd, 32, &val);

      *(int*)&a_data[j] = spread[(val >> 28) & 0xf];
      *(int*)&a_data[j + 4] = spread[(val >> 24) & 0xf];
      *(int*)&a_data[j + 8] = spread[(val >> 20) & 0xf];
      *(int*)&a_data[j + 12] = spread[(val >> 16) & 0xf];
      *(int*)&a_data[j + 16] = spread[(val >> 12) & 0xf];
      *(int*)&a_data[j + 20] = spread[(val >> 8) & 0xf];
      *(int*)&a_data[j + 24] = spread[(val >> 4) & 0xf];
      *(int*)&a_data[j + 28] = spread[(val) & 0xf];
   }
   for (; j < a_data_len; j++)
      scarletbook_fio_bit_get_chr_unsigned(sd, 1, &a_data[j]);
}

static int
scarletbook_unpack_dst_frame(scarletbook_ebunch* d,
                             uint8_t* dst_data_frame,
                             uint8_t* dsd_data_frame)
{
   int dummy;

   scarletbook_fill_buffer(&d->s, dst_data_frame, d->frame_hdr.calc_nr_of_bytes);

   if (scarletbook_fio_bit_get_int_unsigned(&d->s, 1, &d->frame_hdr.dst_coded))
      return dst_err_negative_bit_allocation;

   if (d->frame_hdr.dst_coded == 0)
   {
      if (scarletbook_fio_bit_get_int_unsigned(&d->s, 1, &dummy))
         return dst_err_negative_bit_allocation;

      if (scarletbook_fio_bit_get_int_unsigned(&d->s, 6, &dummy))
         return dst_err_negative_bit_allocation;

      if (dummy != 0)
         return dst_err_invalid_stuffing_pattern;

      scarletbook_read_dsd_frame(&d->s, d->frame_hdr.max_frame_len, d->frame_hdr.nr_of_channels, dsd_data_frame);
   }
   else
   {
      int error;

      if ((error = scarletbook_read_segment_data(&d->s, &d->frame_hdr)) != 0)
         return error;

      if ((error = scarletbook_read_mapping_data(&d->s, &d->frame_hdr)) != 0)
         return error;

      if ((error = scarletbook_read_filter_coef_sets(&d->s, d->frame_hdr.nr_of_channels, &d->frame_hdr, &d->str_filter)) != 0)
         return error;

      if ((error = scarletbook_read_probability_tables(&d->s, &d->frame_hdr, &d->str_ptable, d->p_one)) != 0)
         return error;

      d->a_data_len = d->frame_hdr.calc_nr_of_bits - scarletbook_get_in_bitcount(&d->s);
      scarletbook_read_arithmetic_coded_data(&d->s, d->a_data_len, d->a_data);

      if ((d->a_data_len > 0) && (d->a_data[0] != 0))
         return dst_err_invalid_arithmetic_code;
   }

   return dst_err_no_error;
}

#define PBITS AC_BITS
#define NBITS 4

#define ABITS (PBITS + NBITS)
#define ONE   (1 << ABITS)
#define HALF  (1 << (ABITS - 1))

static __inline void
scarletbook_lt_ac_decode_bit_init(scarletbook_ac_data* ac, uint8_t* cb, int fs)
{
   ac->init = 0;
   ac->a = ONE - 1;
   ac->c = 0;
   for (ac->cbptr = 1; ac->cbptr <= ABITS; ac->cbptr++)
   {
      ac->c <<= 1;
      if (ac->cbptr < fs)
      {
         ac->c |= cb[ac->cbptr];
      }
   }
}

static __inline void
scarletbook_lt_ac_decode_bit_decode(scarletbook_ac_data* ac, uint8_t* b, int p, uint8_t* cb, int fs)
{
   unsigned int ap;
   unsigned int h;

   ap = ((ac->a >> PBITS) | ((ac->a >> (PBITS - 1)) & 1)) * p;

   h = ac->a - ap;
   if (ac->c >= h)
   {
      *b = 0;
      ac->c -= h;
      ac->a = ap;
   }
   else
   {
      *b = 1;
      ac->a = h;
   }
   while (ac->a < HALF)
   {
      ac->a <<= 1;
      ac->c <<= 1;
      if (ac->cbptr < fs)
      {
         ac->c |= cb[ac->cbptr];
      }
      ac->cbptr++;
   }
}

static __inline void
scarletbook_lt_ac_decode_bit_flush(scarletbook_ac_data* ac, uint8_t* b, int p, uint8_t* cb, int fs)
{
   ac->init = 1;
   if (ac->cbptr < fs - 7)
   {
      *b = 0;
   }
   else
   {
      *b = 1;
      while ((ac->cbptr < fs) && (*b == 1))
      {
         if (cb[ac->cbptr] != 0)
         {
            *b = 1;
         }
         ac->cbptr++;
      }
   }
}

static __inline int
scarletbook_lt_ac_get_ptable_index(int16_t predic_val, int ptable_len)
{
   int j;

   j = (predic_val > 0 ? predic_val : -predic_val) >> AC_QSTEP;
   if (j >= ptable_len)
   {
      j = ptable_len - 1;
   }

   return j;
}

static void
scarletbook_fill_table_4_bit(int nr_of_channels, int nr_of_bits_per_ch, scarletbook_segment* s, char table4_bit[MAX_CHANNELS][MAX_DSDBITS_INFRAME])
{
   int bit_nr;
   int ch_nr;
   int seg_nr;
   int start;
   int end;
   char val;

   for (ch_nr = 0; ch_nr < nr_of_channels; ch_nr++)
   {
      char* table4_bit_ch = table4_bit[ch_nr];
      for (seg_nr = 0, start = 0; seg_nr < s->nr_of_segments[ch_nr] - 1; seg_nr++)
      {
         val = (char)s->table4_segment[ch_nr][seg_nr];
         end = start + s->resolution * 8 * s->segment_len[ch_nr][seg_nr];
         for (bit_nr = start; bit_nr < end; bit_nr++)
         {
            table4_bit[ch_nr][bit_nr] = val;
         }
         start += s->resolution * 8 * s->segment_len[ch_nr][seg_nr];
      }

      val = (char)s->table4_segment[ch_nr][seg_nr];
      memset(&table4_bit_ch[start], val, nr_of_bits_per_ch - start);
   }
}

static const int16_t reverse[128] = {
   1, 65, 33, 97, 17, 81, 49, 113, 9, 73, 41, 105, 25, 89, 57, 121,
   5, 69, 37, 101, 21, 85, 53, 117, 13, 77, 45, 109, 29, 93, 61, 125,
   3, 67, 35, 99, 19, 83, 51, 115, 11, 75, 43, 107, 27, 91, 59, 123,
   7, 71, 39, 103, 23, 87, 55, 119, 15, 79, 47, 111, 31, 95, 63, 127,
   2, 66, 34, 98, 18, 82, 50, 114, 10, 74, 42, 106, 26, 90, 58, 122,
   6, 70, 38, 102, 22, 86, 54, 118, 14, 78, 46, 110, 30, 94, 62, 126,
   4, 68, 36, 100, 20, 84, 52, 116, 12, 76, 44, 108, 28, 92, 60, 124,
   8, 72, 40, 104, 24, 88, 56, 120, 16, 80, 48, 112, 32, 96, 64, 128};

static int16_t
scarletbook_reverse_7_lsbs(int16_t c)
{
   return reverse[(c + (1 << SIZE_PREDCOEF)) & 127];
}

static void
scarletbook_lt_init_coef_tables_i(scarletbook_ebunch* d, int16_t i_coef_i[2 * MAX_CHANNELS][16][256])
{
   int filter_nr, filter_length, table_nr, k, i, j;

   for (filter_nr = 0; filter_nr < d->frame_hdr.nr_of_filters; filter_nr++)
   {
      filter_length = d->frame_hdr.pred_order[filter_nr];
      for (table_nr = 0; table_nr < 16; table_nr++)
      {
         k = filter_length - table_nr * 8;
         if (k > 8)
         {
            k = 8;
         }
         else if (k < 0)
         {
            k = 0;
         }
         for (i = 0; i < 256; i++)
         {
            int cvalue = 0;
            for (j = 0; j < k; j++)
            {
               cvalue += (((i >> j) & 1) * 2 - 1) * d->frame_hdr.i_coef_a[filter_nr][table_nr * 8 + j];
            }
            i_coef_i[filter_nr][table_nr][i] = (int16_t)cvalue;
         }
      }
   }
}

static void
scarletbook_lt_init_status(scarletbook_ebunch* d, uint8_t status[MAX_CHANNELS][16])
{
   int ch_nr, table_nr;

   for (ch_nr = 0; ch_nr < d->frame_hdr.nr_of_channels; ch_nr++)
   {
      for (table_nr = 0; table_nr < 16; table_nr++)
      {
         status[ch_nr][table_nr] = 0xaa;
      }
   }
}

#define LT_RUN_FILTER_I(filter_table, channel_status) \
   predict = filter_table[0][channel_status[0]];      \
   predict += filter_table[1][channel_status[1]];     \
   predict += filter_table[2][channel_status[2]];     \
   predict += filter_table[3][channel_status[3]];     \
   predict += filter_table[4][channel_status[4]];     \
   predict += filter_table[5][channel_status[5]];     \
   predict += filter_table[6][channel_status[6]];     \
   predict += filter_table[7][channel_status[7]];     \
   predict += filter_table[8][channel_status[8]];     \
   predict += filter_table[9][channel_status[9]];     \
   predict += filter_table[10][channel_status[10]];   \
   predict += filter_table[11][channel_status[11]];   \
   predict += filter_table[12][channel_status[12]];   \
   predict += filter_table[13][channel_status[13]];   \
   predict += filter_table[14][channel_status[14]];   \
   predict += filter_table[15][channel_status[15]];

static int
scarletbook_dst_fram_dst_decode(uint8_t* dst_data, uint8_t* muxed_dsd_data, int frame_size_in_bytes, int frame_cnt, scarletbook_ebunch* d)
{
   int error;
   int bit_nr;
   int ch_nr;
   uint8_t ac_error;
   scarletbook_frame_header* frame_hdr = &d->frame_hdr;
   const int nr_of_bits_per_ch = frame_hdr->nr_of_bits_per_ch;
   const int nr_of_channels = frame_hdr->nr_of_channels;
   uint8_t* muxed_dsd = muxed_dsd_data;
   const char* filter4_bit[MAX_CHANNELS];
   const char* ptable4_bit[MAX_CHANNELS];

   frame_hdr->frame_nr = frame_cnt;
   frame_hdr->calc_nr_of_bytes = frame_size_in_bytes;
   frame_hdr->calc_nr_of_bits = frame_hdr->calc_nr_of_bytes * 8;

   error = scarletbook_unpack_dst_frame(d, dst_data, muxed_dsd_data);

   if (error == dst_err_no_error && frame_hdr->dst_coded == 1)
   {
      scarletbook_ac_data ac;
      int16_t (*lt_icoef_i)[16][256] = d->lt_icoef_i;
      uint8_t (*lt_status)[16] = d->lt_status;

      scarletbook_fill_table_4_bit(nr_of_channels, nr_of_bits_per_ch, &frame_hdr->f_seg, frame_hdr->filter4_bit);
      scarletbook_fill_table_4_bit(nr_of_channels, nr_of_bits_per_ch, &frame_hdr->p_seg, frame_hdr->ptable4_bit);

      scarletbook_lt_init_coef_tables_i(d, lt_icoef_i);
      scarletbook_lt_init_status(d, lt_status);

      for (ch_nr = 0; ch_nr < nr_of_channels; ch_nr++)
      {
         filter4_bit[ch_nr] = frame_hdr->filter4_bit[ch_nr];
         ptable4_bit[ch_nr] = frame_hdr->ptable4_bit[ch_nr];
      }

      scarletbook_lt_ac_decode_bit_init(&ac, d->a_data, d->a_data_len);
      scarletbook_lt_ac_decode_bit_decode(&ac, &ac_error, scarletbook_reverse_7_lsbs(frame_hdr->i_coef_a[0][0]), d->a_data, d->a_data_len);

      memset(muxed_dsd, 0, nr_of_bits_per_ch * nr_of_channels / 8);
      for (bit_nr = 0; bit_nr < nr_of_bits_per_ch; bit_nr++)
      {
         uint8_t* muxed_dsd_row = muxed_dsd + ((size_t)(bit_nr >> 3) * (size_t)nr_of_channels);
         const uint8_t bit_mask = (uint8_t)(1u << (7 - (bit_nr & 7)));

         for (ch_nr = 0; ch_nr < nr_of_channels; ch_nr++)
         {
            int16_t predict;
            uint8_t residual;
            int16_t bit_val;
            const int filter = filter4_bit[ch_nr][bit_nr];

            LT_RUN_FILTER_I(lt_icoef_i[filter], lt_status[ch_nr]);

            if (frame_hdr->half_prob[ch_nr] && (bit_nr < frame_hdr->nr_of_half_bits[ch_nr]))
            {
               scarletbook_lt_ac_decode_bit_decode(&ac, &residual, AC_PROBS / 2, d->a_data, d->a_data_len);
            }
            else
            {
               const int table4bit = ptable4_bit[ch_nr][bit_nr];
               const int ptable_index = scarletbook_lt_ac_get_ptable_index(predict, frame_hdr->ptable_len[table4bit]);

               scarletbook_lt_ac_decode_bit_decode(&ac, &residual, d->p_one[table4bit][ptable_index], d->a_data, d->a_data_len);
            }

            bit_val = ((((uint16_t)predict) >> 15) ^ residual) & 1;

            muxed_dsd_row[ch_nr] |= (uint8_t)(bit_val != 0 ? bit_mask : 0);

            {
               uint32_t* const st = (uint32_t*)lt_status[ch_nr];
               st[3] = (st[3] << 1) | ((st[2] >> 31) & 1);
               st[2] = (st[2] << 1) | ((st[1] >> 31) & 1);
               st[1] = (st[1] << 1) | ((st[0] >> 31) & 1);
               st[0] = (st[0] << 1) | bit_val;
            }
         }
      }

      scarletbook_lt_ac_decode_bit_flush(&ac, &ac_error, 0, d->a_data, d->a_data_len);

      if (ac_error != 1)
         error = dst_err_arithmetic_decoder;
   }

   if (error != dst_err_no_error)
   {
      memset(muxed_dsd_data, 0x55, (nr_of_bits_per_ch * nr_of_channels) / 8);
   }

   return error;
}

static const char* scarletbook_dst_error_messages[] =
   {
      "",
      "A negative number of bits allocated",
      "Too many segments for this channel",
      "Invalid segment resolution",
      "Invalid segment length",
      "Too many tables for this frame",
      "Invalid table number for segment",
      "Mapping can't be the same for all channels",
      "Not same number of segments for filters and Ptables",
      "Invalid coefficient coding method",
      "Filter coefficient out of range",
      "Invalid Ptable coding method",
      "Ptable entry out of range",
      "Illegal stuffing pattern",
      "Illegal arithmetic code",
      "Arithmetic decoding error",
};

static const char*
scarletbook_dst_get_error_message(int error)
{
   if (error >= 0 && error < dst_err_max_error)
      return scarletbook_dst_error_messages[error];

   return "Unknown";
}

enum frame_format {
   FRAME_FORMAT_DST = 0,
   FRAME_FORMAT_DSD_3_IN_14 = 2,
   FRAME_FORMAT_DSD_3_IN_16 = 3
};

enum track_type {
   TRACK_TYPE_TITLE = 0x01,
   TRACK_TYPE_PERFORMER = 0x02,
   TRACK_TYPE_SONGWRITER = 0x03,
   TRACK_TYPE_COMPOSER = 0x04,
   TRACK_TYPE_ARRANGER = 0x05,
   TRACK_TYPE_MESSAGE = 0x06,
   TRACK_TYPE_EXTRA_MESSAGE = 0x07,
   TRACK_TYPE_TITLE_PHONETIC = 0x81,
   TRACK_TYPE_PERFORMER_PHONETIC = 0x82,
   TRACK_TYPE_SONGWRITER_PHONETIC = 0x83,
   TRACK_TYPE_COMPOSER_PHONETIC = 0x84,
   TRACK_TYPE_ARRANGER_PHONETIC = 0x85,
   TRACK_TYPE_MESSAGE_PHONETIC = 0x86,
   TRACK_TYPE_EXTRA_MESSAGE_PHONETIC = 0x87
};

enum audio_packet_data_type {
   DATA_TYPE_AUDIO = 2,
   DATA_TYPE_SUPPLEMENTARY = 3,
   DATA_TYPE_PADDING = 7
};

enum dsf_channel_type {
   CHANNEL_TYPE_MONO = 1,
   CHANNEL_TYPE_STEREO = 2,
   CHANNEL_TYPE_3_CHANNELS = 3,
   CHANNEL_TYPE_QUAD = 4,
   CHANNEL_TYPE_4_CHANNELS = 5,
   CHANNEL_TYPE_5_CHANNELS = 6,
   CHANNEL_TYPE_5_1_CHANNELS = 7
};

struct scarletbook_genre_table
{
   uint8_t category;
   uint16_t reserved;
   uint8_t genre;
} __attribute__((packed));

struct scarletbook_locale_table
{
   char language_code[2];
   uint8_t character_set;
   uint8_t reserved;
} __attribute__((packed));

struct scarletbook_master_man
{
   char id[8];
   uint8_t information[2040];
} __attribute__((packed));

struct scarletbook_master_text
{
   char* album_title;
   char* album_title_phonetic;
   char* album_artist;
   char* album_artist_phonetic;
   char* album_publisher;
   char* album_publisher_phonetic;
   char* album_copyright;
   char* album_copyright_phonetic;
   char* disc_title;
   char* disc_title_phonetic;
   char* disc_artist;
   char* disc_artist_phonetic;
   char* disc_publisher;
   char* disc_publisher_phonetic;
   char* disc_copyright;
   char* disc_copyright_phonetic;
};

struct scarletbook_master_sacd_text
{
   char id[8];
   uint8_t reserved[8];
   uint16_t album_title_position;
   uint16_t album_artist_position;
   uint16_t album_publisher_position;
   uint16_t album_copyright_position;
   uint16_t album_title_phonetic_position;
   uint16_t album_artist_phonetic_position;
   uint16_t album_publisher_phonetic_position;
   uint16_t album_copyright_phonetic_position;
   uint16_t disc_title_position;
   uint16_t disc_artist_position;
   uint16_t disc_publisher_position;
   uint16_t disc_copyright_position;
   uint16_t disc_title_phonetic_position;
   uint16_t disc_artist_phonetic_position;
   uint16_t disc_publisher_phonetic_position;
   uint16_t disc_copyright_phonetic_position;
   uint8_t data[2000];
} __attribute__((packed));

struct scarletbook_master_toc
{
   char id[8];
   struct
   {
      uint8_t major;
      uint8_t minor;
   } version;
   uint8_t reserved01[6];
   uint16_t album_set_size;
   uint16_t album_sequence_number;
   uint8_t reserved02[4];
   char album_catalog_number[16];
   struct scarletbook_genre_table album_genre[4];
   uint8_t reserved03[8];
   uint32_t area_1_toc_1_start;
   uint32_t area_1_toc_2_start;
   uint32_t area_2_toc_1_start;
   uint32_t area_2_toc_2_start;
#if defined(__BIG_ENDIAN__)
   uint8_t disc_type_hybrid : 1;
   uint8_t disc_type_reserved : 7;
#else
   uint8_t disc_type_reserved : 7;
   uint8_t disc_type_hybrid : 1;
#endif
   uint8_t reserved04[3];
   uint16_t area_1_toc_size;
   uint16_t area_2_toc_size;
   char disc_catalog_number[16];
   struct scarletbook_genre_table disc_genre[4];
   uint16_t disc_date_year;
   uint8_t disc_date_month;
   uint8_t disc_date_day;
   uint8_t reserved05[4];
   uint8_t text_area_count;
   uint8_t reserved06[7];
   struct scarletbook_locale_table locales[8];
} __attribute__((packed));

struct scarletbook_area_toc
{
   char id[8];
   struct
   {
      uint8_t major;
      uint8_t minor;
   } version;
   uint16_t size;
   uint8_t reserved01[4];
   uint32_t max_byte_rate;
   uint8_t sample_frequency;
   uint8_t frame_format : 4;
   uint8_t reserved02 : 4;
   uint8_t reserved03[10];
   uint8_t channel_count;
   uint8_t extra_settings : 3;
   uint8_t loudspeaker_config : 5;
   uint8_t max_available_channels;
   uint8_t area_mute_flags;
   uint8_t reserved04[12];
   uint8_t track_attribute : 4;
   uint8_t reserved05 : 4;
   uint8_t reserved06[15];
   struct
   {
      uint8_t minutes;
      uint8_t seconds;
      uint8_t frames;
   } total_playtime;
   uint8_t reserved07;
   uint8_t track_offset;
   uint8_t track_count;
   uint8_t reserved08[2];
   uint32_t track_start;
   uint32_t track_end;
   uint8_t text_area_count;
   uint8_t reserved09[7];
   struct scarletbook_locale_table languages[10];
   uint16_t track_text_offset;
   uint16_t index_list_offset;
   uint16_t access_list_offset;
   uint8_t reserved10[10];
   uint16_t area_description_offset;
   uint16_t copyright_offset;
   uint16_t area_description_phonetic_offset;
   uint16_t copyright_phonetic_offset;
   uint8_t data[1896];
} __attribute__((packed));

struct scarletbook_area_track_text
{
   char* track_type_title;
   char* track_type_performer;
   char* track_type_songwriter;
   char* track_type_composer;
   char* track_type_arranger;
   char* track_type_message;
   char* track_type_extra_message;
   char* track_type_title_phonetic;
   char* track_type_performer_phonetic;
   char* track_type_songwriter_phonetic;
   char* track_type_composer_phonetic;
   char* track_type_arranger_phonetic;
   char* track_type_message_phonetic;
   char* track_type_extra_message_phonetic;
};

struct scarletbook_audio_frame_info
{
   struct
   {
      uint8_t minutes;
      uint8_t seconds;
      uint8_t frames;
   } __attribute__((packed)) timecode;

   uint8_t channel_bit_3 : 1;
   uint8_t channel_bit_2 : 1;
   uint8_t sector_count : 5;
   uint8_t channel_bit_1 : 1;
} __attribute__((packed));
#define AUDIO_FRAME_INFO_SIZE 4U

struct scarletbook_audio_frame_header
{
   uint8_t dst_encoded : 1;
   uint8_t reserved : 1;
   uint8_t frame_info_count : 3;
   uint8_t packet_info_count : 3;
} __attribute__((packed));
#define AUDIO_SECTOR_HEADER_SIZE 1U

struct scarletbook_audio_packet_info
{
   uint8_t frame_start : 1;
   uint8_t reserved : 1;
   uint8_t data_type : 3;
   uint16_t packet_length : 11;
} __attribute__((packed));
#define AUDIO_PACKET_INFO_SIZE 2U

struct scarletbook_audio_sector
{
   struct scarletbook_audio_frame_header header;
   struct scarletbook_audio_packet_info packet[7];
   struct scarletbook_audio_frame_info frame[7];
} __attribute__((packed));

struct scarletbook_area_text
{
   char id[8];
   uint16_t track_text_position[];
} __attribute__((packed));

struct scarletbook_isrc
{
   char country_code[2];
   char owner_code[3];
   char recording_year[2];
   char designation_code[5];
} __attribute__((packed));

struct scarletbook_area_isrc_genre
{
   char id[8];
   struct scarletbook_isrc isrc[255];
   uint32_t reserved;
   struct scarletbook_genre_table track_genre[255];
} __attribute__((packed));

struct scarletbook_area_tracklist_offset
{
   char id[8];
   uint32_t track_start_lsn[255];
   uint32_t track_length_lsn[255];
} __attribute__((packed));

struct scarletbook_area_tracklist_time
{
   uint8_t minutes;
   uint8_t seconds;
   uint8_t frames;
   uint8_t reserved : 3;
   uint8_t track_flags_tmf1 : 1;
   uint8_t track_flags_tmf2 : 1;
   uint8_t track_flags_tmf3 : 1;
   uint8_t track_flags_tmf4 : 1;
   uint8_t track_flags_ilp : 1;
} __attribute__((packed));

struct scarletbook_area_tracklist
{
   char id[8];
   struct scarletbook_area_tracklist_time start[255];
   struct scarletbook_area_tracklist_time duration[255];
} __attribute__((packed));

struct scarletbook_audio_frame
{
   uint8_t* data;
   int size;
   int started;

   int sector_count;
   int channel_count;

   int dst_encoded;

   struct
   {
      uint8_t minutes;
      uint8_t seconds;
      uint8_t frames;
   } timecode;
};

struct scarletbook_area
{
   uint8_t* area_data;
   struct scarletbook_area_toc* area_toc;
   struct scarletbook_area_tracklist_offset* area_tracklist_offset;
   struct scarletbook_area_tracklist* area_tracklist_time;
   struct scarletbook_area_text* area_text;
   struct scarletbook_area_track_text area_track_text[255];
   struct scarletbook_area_isrc_genre* area_isrc_genre;

   char* description;
   char* copyright;
   char* description_phonetic;
   char* copyright_phonetic;
};

struct scarletbook_iso
{
   int fd;
   uint8_t* buffer;
   int stereo_index;
   int multi_index;

   uint8_t* master_data;
   struct scarletbook_master_toc master_toc;
   struct scarletbook_master_man master_man;
   struct scarletbook_master_text master_text;

   int area_count;
   struct scarletbook_area area[4];

   struct scarletbook_audio_frame frame;
   struct scarletbook_audio_sector audio_sector;

   int frame_info_idx;

   uint32_t count_frames;
};

struct scarletbook_handle
{
   struct scarletbook_sacd_reader* sacd;

   uint8_t* master_data;
   struct scarletbook_master_toc* master_toc;
   struct scarletbook_master_man* master_man;
   struct scarletbook_master_text master_text;

   int twoch_area_idx;
   int mulch_area_idx;
   int area_count;
   struct scarletbook_area area[4];

   struct scarletbook_audio_frame frame;
   struct scarletbook_audio_sector audio_sector;
   int frame_info_idx;

   uint32_t count_frames;

   int id3_tag_mode;
};

struct scarletbook_sacd_input
{
   int fd;
   uint8_t* input_buffer;
};

struct scarletbook_sacd_reader
{
   int is_image_file;

   struct scarletbook_sacd_input* dev;
};

struct scarletbook_dsd_chunk_header
{
   uint32_t chunk_id;
   uint64_t chunk_data_size;
   uint64_t total_file_size;
   uint64_t metadata_offset;
} __attribute__((packed));

struct scarletbook_fmt_chunk
{
   uint32_t chunk_id;
   uint64_t chunk_data_size;
   uint32_t version;
   uint32_t format_id;
   uint32_t channel_type;
   uint32_t channel_count;
   uint32_t sample_frequency;
   uint32_t bits_per_sample;
   uint64_t sample_count;
   uint32_t block_size_per_channel;
   uint32_t reserved;
} __attribute__((packed));

struct scarletbook_data_chunk
{
   uint32_t chunk_id;
   uint64_t chunk_data_size;
} __attribute__((packed));

struct scarletbook_dsf_handle
{
   uint8_t* header;
   size_t header_size;
   uint8_t* footer;
   size_t footer_size;
   uint64_t audio_data_size;
   int channel_count;
   uint64_t sample_count;
   uint8_t buffer[MAX_CHANNEL_COUNT][SACD_BLOCK_SIZE_PER_CHANNEL];
   uint8_t* buffer_ptr[MAX_CHANNEL_COUNT];
};

typedef void (*dst_frame_decoded_callback_t)(uint8_t* frame_data, size_t frame_size, void* userdata);
typedef void (*dst_frame_error_callback_t)(int frame_count, int frame_error_code, const char* frame_error_message, void* userdata);

struct scarletbook_dst_decoder
{
   scarletbook_ebunch decoder;
   int channel_count;
   int frame_count;
   size_t decoded_frame_size;
   uint8_t* dsd_data;
   dst_frame_decoded_callback_t scarletbook_frame_decoded_callback;
   dst_frame_error_callback_t scarletbook_frame_error_callback;
   void* userdata;
};

struct scarletbook_output_format
{
   FILE* fd;
   char* filename;
   struct scarletbook_handle* sb_handle;
   int area;
   int track;
   void* priv;
   struct scarletbook_dst_decoder* dst_decoder;
   int dst_encoded_import;
   int decoder_failed;
   int write_failed;
};

typedef void (*frame_read_callback_t)(struct scarletbook_handle* handle, uint8_t* frame_data, size_t frame_size, void* userdata);

static const char* character_set[] =
   {
      "US-ASCII",
      "ISO646-JP",
      "ISO-8859-1",
      "SHIFT_JISX0213",
      "KSC5601.1987-0",
      "GB2312.1980-0",
      "BIG5",
      "ISO-8859-1",
};

static const uint8_t bit_reverse_table[] =
   {
      0x00, 0x80, 0x40, 0xc0, 0x20, 0xa0, 0x60, 0xe0, 0x10, 0x90, 0x50, 0xd0, 0x30, 0xb0, 0x70, 0xf0,
      0x08, 0x88, 0x48, 0xc8, 0x28, 0xa8, 0x68, 0xe8, 0x18, 0x98, 0x58, 0xd8, 0x38, 0xb8, 0x78, 0xf8,
      0x04, 0x84, 0x44, 0xc4, 0x24, 0xa4, 0x64, 0xe4, 0x14, 0x94, 0x54, 0xd4, 0x34, 0xb4, 0x74, 0xf4,
      0x0c, 0x8c, 0x4c, 0xcc, 0x2c, 0xac, 0x6c, 0xec, 0x1c, 0x9c, 0x5c, 0xdc, 0x3c, 0xbc, 0x7c, 0xfc,
      0x02, 0x82, 0x42, 0xc2, 0x22, 0xa2, 0x62, 0xe2, 0x12, 0x92, 0x52, 0xd2, 0x32, 0xb2, 0x72, 0xf2,
      0x0a, 0x8a, 0x4a, 0xca, 0x2a, 0xaa, 0x6a, 0xea, 0x1a, 0x9a, 0x5a, 0xda, 0x3a, 0xba, 0x7a, 0xfa,
      0x06, 0x86, 0x46, 0xc6, 0x26, 0xa6, 0x66, 0xe6, 0x16, 0x96, 0x56, 0xd6, 0x36, 0xb6, 0x76, 0xf6,
      0x0e, 0x8e, 0x4e, 0xce, 0x2e, 0xae, 0x6e, 0xee, 0x1e, 0x9e, 0x5e, 0xde, 0x3e, 0xbe, 0x7e, 0xfe,
      0x01, 0x81, 0x41, 0xc1, 0x21, 0xa1, 0x61, 0xe1, 0x11, 0x91, 0x51, 0xd1, 0x31, 0xb1, 0x71, 0xf1,
      0x09, 0x89, 0x49, 0xc9, 0x29, 0xa9, 0x69, 0xe9, 0x19, 0x99, 0x59, 0xd9, 0x39, 0xb9, 0x79, 0xf9,
      0x05, 0x85, 0x45, 0xc5, 0x25, 0xa5, 0x65, 0xe5, 0x15, 0x95, 0x55, 0xd5, 0x35, 0xb5, 0x75, 0xf5,
      0x0d, 0x8d, 0x4d, 0xcd, 0x2d, 0xad, 0x6d, 0xed, 0x1d, 0x9d, 0x5d, 0xdd, 0x3d, 0xbd, 0x7d, 0xfd,
      0x03, 0x83, 0x43, 0xc3, 0x23, 0xa3, 0x63, 0xe3, 0x13, 0x93, 0x53, 0xd3, 0x33, 0xb3, 0x73, 0xf3,
      0x0b, 0x8b, 0x4b, 0xcb, 0x2b, 0xab, 0x6b, 0xeb, 0x1b, 0x9b, 0x5b, 0xdb, 0x3b, 0xbb, 0x7b, 0xfb,
      0x07, 0x87, 0x47, 0xc7, 0x27, 0xa7, 0x67, 0xe7, 0x17, 0x97, 0x57, 0xd7, 0x37, 0xb7, 0x77, 0xf7,
      0x0f, 0x8f, 0x4f, 0xcf, 0x2f, 0xaf, 0x6f, 0xef, 0x1f, 0x9f, 0x5f, 0xdf, 0x3f, 0xbf, 0x7f, 0xff};

static struct scarletbook_iso* scarletbook_open(char* filename);
static int scarletbook_close(struct scarletbook_iso* iso);
static bool scarletbook_has_channel(bool stereo, struct scarletbook_iso* iso);
static int scarletbook_extract(bool stereo, bool multi_channel, struct scarletbook_iso* iso);
static int scarletbook_extract_area(bool stereo, bool multi_channel, struct scarletbook_iso* iso);
static int scarletbook_remove_output_directories(void);
static int scarletbook_load_master_toc(struct scarletbook_iso* iso);
static void scarletbook_free_master_text(struct scarletbook_master_text* master_text);
static void scarletbook_free_area(struct scarletbook_area* area);
static int scarletbook_load_area(struct scarletbook_iso* iso, struct scarletbook_handle* handle, int area_idx, bool stereo);
static void scarletbook_frame_init(struct scarletbook_handle* handle);
static int scarletbook_read_area_toc(struct scarletbook_handle* handle, int area_idx);
static int scarletbook_process_frames(struct scarletbook_handle* handle, uint8_t* read_buffer, int blocks_read_in, int last_block,
                                      frame_read_callback_t scarletbook_frame_read_callback, void* userdata);
static char* scarletbook_get_music_filename(struct scarletbook_handle* handle, int area, int track, const char* override_title);
static int scarletbook_dsf_create(struct scarletbook_output_format* ft);
static int scarletbook_dsf_create_header(struct scarletbook_output_format* ft);
static int scarletbook_dsf_write_frame(struct scarletbook_output_format* ft, const uint8_t* buf, size_t len);
static int scarletbook_dsf_close(struct scarletbook_output_format* ft);
static struct scarletbook_dst_decoder* scarletbook_dst_decoder_create(int channel_count,
                                                                      dst_frame_decoded_callback_t scarletbook_frame_decoded_callback,
                                                                      dst_frame_error_callback_t scarletbook_frame_error_callback,
                                                                      void* userdata);
static void scarletbook_dst_decoder_destroy(struct scarletbook_dst_decoder* dst_decoder);
static void scarletbook_dst_decoder_decode(struct scarletbook_dst_decoder* dst_decoder, uint8_t* frame_data, size_t frame_size);
static void scarletbook_frame_read_callback(struct scarletbook_handle* handle, uint8_t* frame_data, size_t frame_size, void* userdata);
static void scarletbook_frame_decoded_callback(uint8_t* frame_data, size_t frame_size, void* userdata);
static void scarletbook_frame_error_callback(int frame_count, int frame_error_code, const char* frame_error_message, void* userdata);
static int scarletbook_get_channel_count(struct scarletbook_audio_frame_info* frame_info);
static void scarletbook_exec_read_callback(struct scarletbook_handle* handle, frame_read_callback_t scarletbook_frame_read_callback, void* userdata);
static void scarletbook_safe_copy(char* dst, size_t cap, const char* src);
static void scarletbook_sanitize_filename(char* value);
static void scarletbook_print_completed_output_filename(const char* current_directory, const char* filename);

static char* scarletbook_charset_convert(const char* input, size_t input_len, const char* from_charset, const char* to_charset);

static int scarletbook_read_master_toc(struct scarletbook_handle* handle);
static uint32_t scarletbook_sacd_input_read(struct scarletbook_sacd_input* dev, uint32_t pos, uint32_t blocks, void* buffer);
static uint32_t scarletbook_sacd_read_block_raw(struct scarletbook_sacd_reader* sacd, uint32_t lb_number, uint32_t block_count, uint8_t* data);

static struct scarletbook_dst_decoder*
scarletbook_dst_decoder_create(int channel_count, dst_frame_decoded_callback_t scarletbook_frame_decoded_callback,
                               dst_frame_error_callback_t scarletbook_frame_error_callback, void* userdata)
{
   struct scarletbook_dst_decoder* dst_decoder;

   dst_decoder = (struct scarletbook_dst_decoder*)calloc(1, sizeof(struct scarletbook_dst_decoder));
   if (dst_decoder == NULL)
   {
      return NULL;
   }

   if (scarletbook_dst_init_decoder(&dst_decoder->decoder, channel_count, 64) != 0)
   {
      free(dst_decoder);
      return NULL;
   }

   dst_decoder->channel_count = channel_count;
   dst_decoder->decoded_frame_size = (size_t)(dst_decoder->decoder.frame_hdr.nr_of_bits_per_ch / 8 * channel_count);
   dst_decoder->dsd_data = (uint8_t*)malloc(dst_decoder->decoded_frame_size);
   if (dst_decoder->dsd_data == NULL)
   {
      scarletbook_dst_close_decoder(&dst_decoder->decoder);
      free(dst_decoder);
      return NULL;
   }

   dst_decoder->scarletbook_frame_decoded_callback = scarletbook_frame_decoded_callback;
   dst_decoder->scarletbook_frame_error_callback = scarletbook_frame_error_callback;
   dst_decoder->userdata = userdata;

   return dst_decoder;
}

static void
scarletbook_dst_decoder_destroy(struct scarletbook_dst_decoder* dst_decoder)
{
   if (dst_decoder == NULL)
   {
      return;
   }

   scarletbook_dst_close_decoder(&dst_decoder->decoder);
   free(dst_decoder->dsd_data);
   free(dst_decoder);
}

static void
scarletbook_dst_decoder_decode(struct scarletbook_dst_decoder* dst_decoder, uint8_t* frame_data, size_t frame_size)
{
   int error;

   if (dst_decoder == NULL || frame_data == NULL)
   {
      return;
   }

   error = scarletbook_dst_fram_dst_decode(frame_data, dst_decoder->dsd_data, (int)frame_size,
                                           dst_decoder->frame_count, &dst_decoder->decoder);
   if (error != dst_err_no_error)
   {
      if (dst_decoder->scarletbook_frame_error_callback != NULL)
      {
         dst_decoder->scarletbook_frame_error_callback(dst_decoder->frame_count, error,
                                                       scarletbook_dst_get_error_message(error), dst_decoder->userdata);
      }
   }
   else if (dst_decoder->scarletbook_frame_decoded_callback != NULL)
   {
      dst_decoder->scarletbook_frame_decoded_callback(dst_decoder->dsd_data, dst_decoder->decoded_frame_size,
                                                      dst_decoder->userdata);
   }

   dst_decoder->frame_count++;
}

int
hrmp_extract_scarletbook(char* f)
{
   struct scarletbook_iso* iso = NULL;
   bool stereo = false;
   bool multi_channel = false;

   iso = scarletbook_open(f);
   if (iso == NULL)
   {
      goto error;
   }

   stereo = scarletbook_has_channel(true, iso);
   multi_channel = scarletbook_has_channel(false, iso);

   if (scarletbook_extract(stereo, multi_channel, iso))
   {
      goto error;
   }

   scarletbook_close(iso);

   return 0;

error:

   scarletbook_close(iso);

   return 1;
}

static struct scarletbook_iso*
scarletbook_open(char* filename)
{
   struct scarletbook_iso* iso = NULL;

   iso = (struct scarletbook_iso*)malloc(sizeof(struct scarletbook_iso));

   if (iso != NULL)
   {
      memset(iso, 0, sizeof(struct scarletbook_iso));

      iso->fd = -1;

      iso->fd = open(filename, O_RDONLY);
      if (iso->fd == -1)
      {
         goto error;
      }

      iso->stereo_index = -1;
      iso->multi_index = -1;

      if (scarletbook_load_master_toc(iso))
      {
         goto error;
      }
   }

   return iso;

error:

   scarletbook_close(iso);

   return NULL;
}

static int
scarletbook_close(struct scarletbook_iso* iso)
{
   if (iso != NULL)
   {
      free(iso->master_data);
      scarletbook_free_master_text(&iso->master_text);

      if (iso->fd != -1)
      {
         close(iso->fd);
      }

      free(iso);
   }

   return 0;
}

static bool
scarletbook_has_channel(bool stereo, struct scarletbook_iso* iso)
{
   if (iso == NULL)
   {
      return false;
   }

   if (stereo)
   {
      return iso->stereo_index >= 0;
   }

   return iso->multi_index >= 0;
}

static int
scarletbook_extract(bool stereo, bool multi_channel, struct scarletbook_iso* iso)
{
   bool extracted = false;

   if (scarletbook_remove_output_directories() != 0)
   {
      return 1;
   }

   if (stereo)
   {
      if (scarletbook_extract_area(true, multi_channel, iso))
      {
         return 1;
      }
      extracted = true;
   }

   if (multi_channel)
   {
      if (scarletbook_extract_area(false, multi_channel, iso))
      {
         return 1;
      }
      extracted = true;
   }

   return extracted ? 0 : 1;
}

static int
scarletbook_remove_output_directories(void)
{
   char* current_directory = NULL;
   char* directory = NULL;
   int status = 1;

   current_directory = hrmp_get_current_directory();
   if (current_directory == NULL)
   {
      goto error;
   }

   directory = hrmp_append(directory, current_directory);
   if (directory == NULL)
   {
      goto error;
   }
   if (!hrmp_ends_with(directory, "/"))
   {
      directory = hrmp_append_char(directory, '/');
      if (directory == NULL)
      {
         goto error;
      }
   }

   directory = hrmp_append(directory, "2.0");
   if (directory == NULL)
   {
      goto error;
   }
   if (hrmp_is_directory(directory) && hrmp_delete_directory(directory) != 0)
   {
      goto error;
   }

   free(directory);
   directory = hrmp_append(NULL, current_directory);
   if (directory == NULL)
   {
      goto error;
   }
   if (!hrmp_ends_with(directory, "/"))
   {
      directory = hrmp_append_char(directory, '/');
      if (directory == NULL)
      {
         goto error;
      }
   }

   directory = hrmp_append(directory, "5.1");
   if (directory == NULL)
   {
      goto error;
   }
   if (hrmp_is_directory(directory) && hrmp_delete_directory(directory) != 0)
   {
      goto error;
   }

   status = 0;

error:
   free(directory);
   free(current_directory);
   return status;
}

static int
scarletbook_extract_area(bool stereo, bool multi_channel, struct scarletbook_iso* iso)
{
   struct scarletbook_sacd_input input = {0};
   struct scarletbook_sacd_reader reader = {0};
   struct scarletbook_handle handle = {0};
   struct scarletbook_output_format output = {0};
   uint8_t* read_buffer = NULL;
   char* current_directory = NULL;
   char* target_directory = NULL;
   int area_idx = 0;
   int status = 1;
   FILE* output_file = NULL;

   if (iso == NULL)
   {
      goto error;
   }

   input.fd = iso->fd;
   reader.is_image_file = 1;
   reader.dev = &input;

   handle.sacd = &reader;
   handle.master_data = iso->master_data;
   handle.master_toc = &iso->master_toc;
   handle.master_man = &iso->master_man;
   handle.master_text = iso->master_text;
   handle.twoch_area_idx = -1;
   handle.mulch_area_idx = -1;
   handle.frame.data = (uint8_t*)malloc(MAX_DST_SIZE);
   if (handle.frame.data == NULL)
   {
      goto error;
   }

   if (scarletbook_load_area(iso, &handle, area_idx, stereo))
   {
      goto error;
   }

   if (handle.area[area_idx].area_toc == NULL || handle.area[area_idx].area_tracklist_offset == NULL)
   {
      goto error;
   }

   current_directory = hrmp_get_current_directory();
   target_directory = hrmp_append(target_directory, current_directory);

   if (!hrmp_ends_with(target_directory, "/"))
   {
      target_directory = hrmp_append_char(target_directory, '/');
   }

   if (stereo && multi_channel)
   {
      target_directory = hrmp_append(target_directory, "2.0/");
   }
   else if (!stereo)
   {
      target_directory = hrmp_append(target_directory, "5.1/");
   }

   if (!hrmp_exists(target_directory))
   {
      if (hrmp_mkdir(target_directory))
      {
         goto error;
      }
   }

   read_buffer = (uint8_t*)malloc(MAX_PROCESSING_BLOCK_SIZE * SACD_LSN_SIZE);
   if (read_buffer == NULL)
   {
      goto error;
   }

   for (int track_idx = 0; track_idx < handle.area[area_idx].area_toc->track_count; track_idx++)
   {
      uint32_t current_lsn;
      uint32_t remaining_lsn;
      char* music_filename;
      char* file_path;

      current_lsn = handle.area[area_idx].area_tracklist_offset->track_start_lsn[track_idx];
      remaining_lsn = handle.area[area_idx].area_tracklist_offset->track_length_lsn[track_idx];
      if (remaining_lsn == 0)
      {
         continue;
      }

      music_filename = scarletbook_get_music_filename(&handle, area_idx, track_idx, NULL);
      if (music_filename == NULL)
      {
         goto error;
      }

      file_path = hrmp_append(NULL, target_directory);
      file_path = hrmp_append(file_path, music_filename);
      file_path = hrmp_append(file_path, ".dsf");

      output_file = fopen(file_path, "wb");
      if (output_file == NULL)
      {
         free(file_path);
         free(music_filename);
         goto error;
      }

      memset(&output, 0, sizeof(output));
      output.fd = output_file;
      output.filename = file_path;
      output.sb_handle = &handle;
      output.area = area_idx;
      output.track = track_idx;
      output.dst_encoded_import = handle.area[area_idx].area_toc->frame_format == FRAME_FORMAT_DST;
      output.priv = calloc(1, sizeof(struct scarletbook_dsf_handle));
      if (output.priv == NULL)
      {
         free(file_path);
         free(music_filename);
         goto error;
      }

      if (scarletbook_dsf_create(&output))
      {
         free(music_filename);
         goto error;
      }

      if (output.dst_encoded_import)
      {
         output.dst_decoder = scarletbook_dst_decoder_create(handle.area[area_idx].area_toc->channel_count,
                                                             scarletbook_frame_decoded_callback, scarletbook_frame_error_callback, &output);
         if (output.dst_decoder == NULL)
         {
            free(music_filename);
            goto error;
         }
      }

      scarletbook_frame_init(&handle);

      while (remaining_lsn > 0)
      {
         uint32_t block_count = remaining_lsn > MAX_PROCESSING_BLOCK_SIZE ? MAX_PROCESSING_BLOCK_SIZE : remaining_lsn;
         uint32_t blocks_read = scarletbook_sacd_read_block_raw(&reader, current_lsn, block_count, read_buffer);
         int last_block;

         if (blocks_read == 0)
         {
            free(music_filename);
            goto error;
         }

         if (blocks_read > remaining_lsn)
         {
            blocks_read = remaining_lsn;
         }

         last_block = blocks_read >= remaining_lsn;
         if (scarletbook_process_frames(&handle, read_buffer, (int)blocks_read, last_block, scarletbook_frame_read_callback, &output) < 0 ||
             output.write_failed || output.decoder_failed)
         {
            free(music_filename);
            goto error;
         }

         current_lsn += blocks_read;
         remaining_lsn -= blocks_read;
      }

      if (output.dst_decoder != NULL)
      {
         scarletbook_dst_decoder_destroy(output.dst_decoder);
         output.dst_decoder = NULL;
      }

      if (output.write_failed || output.decoder_failed)
      {
         free(music_filename);
         goto error;
      }

      if (scarletbook_dsf_close(&output))
      {
         free(music_filename);
         goto error;
      }

      scarletbook_print_completed_output_filename(current_directory, output.filename);

      free(output.priv);
      output.priv = NULL;
      fclose(output_file);
      output_file = NULL;
      free(file_path);
      free(music_filename);
   }

   status = 0;

   free(current_directory);
   free(target_directory);
   free(read_buffer);
   free(handle.frame.data);
   scarletbook_free_area(&handle.area[area_idx]);

   return status;

error:

   if (output.priv != NULL)
   {
      if (output.dst_decoder != NULL)
      {
         scarletbook_dst_decoder_destroy(output.dst_decoder);
         output.dst_decoder = NULL;
      }
      scarletbook_dsf_close(&output);
      free(output.priv);
   }
   if (output_file != NULL)
   {
      fclose(output_file);
   }
   free(read_buffer);
   scarletbook_free_area(&handle.area[area_idx]);
   free(handle.frame.data);
   free(current_directory);
   free(target_directory);

   return 1;
}

static int
scarletbook_load_master_toc(struct scarletbook_iso* iso)
{
   struct scarletbook_sacd_input input = {0};
   struct scarletbook_sacd_reader reader = {0};
   struct scarletbook_handle handle = {0};

   if (iso == NULL || iso->fd < 0)
   {
      return 1;
   }

   input.fd = iso->fd;
   reader.is_image_file = 1;
   reader.dev = &input;

   handle.sacd = &reader;
   handle.twoch_area_idx = -1;
   handle.mulch_area_idx = -1;

   if (!scarletbook_read_master_toc(&handle))
   {
      scarletbook_free_master_text(&handle.master_text);
      free(handle.master_data);
      return 1;
   }

   iso->master_data = handle.master_data;
   iso->master_toc = *handle.master_toc;
   iso->master_man = *handle.master_man;
   iso->master_text = handle.master_text;

   if (iso->master_toc.area_1_toc_1_start > 0 && iso->master_toc.area_1_toc_size > 0)
   {
      iso->stereo_index = 0;
   }

   if (iso->master_toc.area_2_toc_1_start > 0 && iso->master_toc.area_2_toc_size > 0)
   {
      iso->multi_index = 1;
   }

   return 0;
}

static void
scarletbook_free_master_text(struct scarletbook_master_text* master_text)
{
   if (master_text == NULL)
   {
      return;
   }

   free(master_text->album_title);
   free(master_text->album_title_phonetic);
   free(master_text->album_artist);
   free(master_text->album_artist_phonetic);
   free(master_text->album_publisher);
   free(master_text->album_publisher_phonetic);
   free(master_text->album_copyright);
   free(master_text->album_copyright_phonetic);
   free(master_text->disc_title);
   free(master_text->disc_title_phonetic);
   free(master_text->disc_artist);
   free(master_text->disc_artist_phonetic);
   free(master_text->disc_publisher);
   free(master_text->disc_publisher_phonetic);
   free(master_text->disc_copyright);
   free(master_text->disc_copyright_phonetic);
}

static void
scarletbook_free_area(struct scarletbook_area* area)
{
   int i;
   uint8_t* area_data;
   struct scarletbook_area_toc* area_toc;

   if (area == NULL)
   {
      return;
   }

   area_data = area->area_data;
   area_toc = area->area_toc;

   if (area_toc != NULL)
   {
      for (i = 0; i < area_toc->track_count; i++)
      {
         free(area->area_track_text[i].track_type_title);
         free(area->area_track_text[i].track_type_performer);
         free(area->area_track_text[i].track_type_songwriter);
         free(area->area_track_text[i].track_type_composer);
         free(area->area_track_text[i].track_type_arranger);
         free(area->area_track_text[i].track_type_message);
         free(area->area_track_text[i].track_type_extra_message);
         free(area->area_track_text[i].track_type_title_phonetic);
         free(area->area_track_text[i].track_type_performer_phonetic);
         free(area->area_track_text[i].track_type_songwriter_phonetic);
         free(area->area_track_text[i].track_type_composer_phonetic);
         free(area->area_track_text[i].track_type_arranger_phonetic);
         free(area->area_track_text[i].track_type_message_phonetic);
         free(area->area_track_text[i].track_type_extra_message_phonetic);
      }
   }

   free(area->description);
   free(area->copyright);
   free(area->description_phonetic);
   free(area->copyright_phonetic);
   free(area_data);
   memset(area, 0, sizeof(*area));
}

static int
scarletbook_load_area(struct scarletbook_iso* iso, struct scarletbook_handle* handle, int area_idx, bool stereo)
{
   uint32_t area_start;
   uint16_t area_size;

   if (iso == NULL || handle == NULL)
   {
      return 1;
   }

   if (stereo)
   {
      area_start = iso->master_toc.area_1_toc_1_start;
      area_size = iso->master_toc.area_1_toc_size;
   }
   else
   {
      area_start = iso->master_toc.area_2_toc_1_start;
      area_size = iso->master_toc.area_2_toc_size;
   }

   if (area_start == 0 || area_size == 0 || area_size > MAX_AREA_TOC_SIZE_LSN)
   {
      return 1;
   }

   handle->area[area_idx].area_data = (uint8_t*)calloc((size_t)area_size, SACD_LSN_SIZE);
   if (handle->area[area_idx].area_data == NULL)
   {
      return 1;
   }

   if (!scarletbook_sacd_read_block_raw(handle->sacd, area_start, area_size, handle->area[area_idx].area_data))
   {
      free(handle->area[area_idx].area_data);
      handle->area[area_idx].area_data = NULL;
      return 1;
   }

   if (!scarletbook_read_area_toc(handle, area_idx))
   {
      scarletbook_free_area(&handle->area[area_idx]);
      return 1;
   }

   handle->area_count = area_idx + 1;
   return 0;
}

static void
scarletbook_frame_init(struct scarletbook_handle* handle)
{
   handle->frame_info_idx = 0;
   handle->frame.size = 0;
   handle->frame.started = 0;
   handle->frame.sector_count = 0;
   handle->frame.channel_count = 0;
   handle->frame.dst_encoded = 0;
   handle->frame.timecode.minutes = (uint8_t)0;
   handle->frame.timecode.seconds = (uint8_t)0;
   handle->frame.timecode.frames = (uint8_t)0;
   memset(&handle->audio_sector, 0, sizeof(struct scarletbook_audio_sector));
}

static int
scarletbook_get_channel_count(struct scarletbook_audio_frame_info* frame_info)
{
   if (frame_info->channel_bit_2 == 1 && frame_info->channel_bit_3 == 0)
   {
      return 6;
   }
   else if (frame_info->channel_bit_2 == 0 && frame_info->channel_bit_3 == 1)
   {
      return 5;
   }

   return 2;
}

static void
scarletbook_exec_read_callback(struct scarletbook_handle* handle, frame_read_callback_t scarletbook_frame_read_callback, void* userdata)
{
   handle->frame.started = 0;
   scarletbook_frame_read_callback(handle, handle->frame.data, (size_t)handle->frame.size, userdata);
}

static void
scarletbook_safe_copy(char* dst, size_t cap, const char* src)
{
   size_t n;

   if (dst == NULL || cap == 0)
   {
      return;
   }

   if (src == NULL)
   {
      dst[0] = '\0';
      return;
   }

   n = strlen(src);
   if (n >= cap)
   {
      n = cap - 1;
   }

   memcpy(dst, src, n);
   dst[n] = '\0';
}

static void
scarletbook_sanitize_filename(char* value)
{
   static const char forbidden[] = "/\\:*?\"<>|";

   if (value == NULL)
   {
      return;
   }

   for (; *value != '\0'; value++)
   {
      if ((unsigned char)*value < 0x20 || strchr(forbidden, *value) != NULL)
      {
         *value = '_';
      }
   }
}

static void
scarletbook_print_completed_output_filename(const char* current_directory, const char* filename)
{
   const char* output_name;
   size_t current_directory_len;

   if (filename == NULL || filename[0] == '\0')
   {
      return;
   }

   output_name = filename;
   if (current_directory != NULL && current_directory[0] != '\0')
   {
      current_directory_len = strlen(current_directory);
      if (strncmp(filename, current_directory, current_directory_len) == 0)
      {
         output_name = filename + current_directory_len;
         if (output_name[0] == '/')
         {
            output_name++;
         }
      }
   }

   printf("%s\n", output_name);
   fflush(stdout);
}

static char*
scarletbook_get_music_filename(struct scarletbook_handle* handle, int area, int track, const char* override_title)
{
   char track_title[MAX_TRACK_TITLE_LEN + 1];
   const char* title = NULL;
   char* result;

   if (override_title != NULL && override_title[0] != '\0')
   {
      title = override_title;
   }
   else if (handle->area[area].area_track_text[track].track_type_title != NULL)
   {
      title = handle->area[area].area_track_text[track].track_type_title;
   }
   else if (handle->master_text.disc_title != NULL)
   {
      title = handle->master_text.disc_title;
   }
   else if (handle->master_text.album_title != NULL)
   {
      title = handle->master_text.album_title;
   }
   else
   {
      title = "Track";
   }

   scarletbook_safe_copy(track_title, sizeof(track_title), title);
   scarletbook_sanitize_filename(track_title);
   if (track_title[0] == '\0')
   {
      scarletbook_safe_copy(track_title, sizeof(track_title), "Track");
   }

   result = NULL;
   if (track + 1 < 10)
   {
      result = hrmp_append_char(result, '0');
   }
   result = hrmp_append_int(result, track + 1);
   result = hrmp_append(result, " - ");
   result = hrmp_append(result, track_title);

   return result;
}

static int
scarletbook_read_area_toc(struct scarletbook_handle* handle, int area_idx)
{
   int i;
   int j;
   uint8_t* p;
   int sacd_text_idx = 0;
   struct scarletbook_area* area = &handle->area[area_idx];
   struct scarletbook_area_toc* area_toc;
   char* current_charset;

   p = area->area_data;
   area_toc = area->area_toc = (struct scarletbook_area_toc*)area->area_data;

   if (strncmp("TWOCHTOC", area_toc->id, 8) != 0 && strncmp("MULCHTOC", area_toc->id, 8) != 0)
   {
      return 0;
   }

   SWAP16(area_toc->size);
   SWAP32(area_toc->track_start);
   SWAP32(area_toc->track_end);
   SWAP16(area_toc->area_description_offset);
   SWAP16(area_toc->copyright_offset);
   SWAP16(area_toc->area_description_phonetic_offset);
   SWAP16(area_toc->copyright_phonetic_offset);
   SWAP32(area_toc->max_byte_rate);
   SWAP16(area_toc->track_text_offset);
   SWAP16(area_toc->index_list_offset);
   SWAP16(area_toc->access_list_offset);

   if (area_toc->version.major > SUPPORTED_VERSION_MAJOR || area_toc->version.minor > SUPPORTED_VERSION_MINOR)
   {
      return 0;
   }

   current_charset = (char*)character_set[area_toc->languages[sacd_text_idx].character_set & 0x07];

   if (area_toc->area_description_offset)
   {
      area->description = scarletbook_charset_convert((char*)area_toc + area_toc->area_description_offset,
                                                      strlen((char*)area_toc + area_toc->area_description_offset),
                                                      current_charset, "UTF-8");
   }

   if (area_toc->copyright_offset)
   {
      area->copyright = scarletbook_charset_convert((char*)area_toc + area_toc->copyright_offset,
                                                    strlen((char*)area_toc + area_toc->copyright_offset),
                                                    current_charset, "UTF-8");
   }

   if (area_toc->area_description_phonetic_offset)
   {
      area->description_phonetic = scarletbook_charset_convert((char*)area_toc + area_toc->area_description_phonetic_offset,
                                                               strlen((char*)area_toc + area_toc->area_description_phonetic_offset),
                                                               current_charset, "UTF-8");
   }

   if (area_toc->copyright_phonetic_offset)
   {
      area->copyright_phonetic = scarletbook_charset_convert((char*)area_toc + area_toc->copyright_phonetic_offset,
                                                             strlen((char*)area_toc + area_toc->copyright_phonetic_offset),
                                                             current_charset, "UTF-8");
   }

   if (area_toc->channel_count == 2 && area_toc->loudspeaker_config == 0)
   {
      handle->twoch_area_idx = area_idx;
   }
   else
   {
      handle->mulch_area_idx = area_idx;
   }

   p += SACD_LSN_SIZE;

   while (p < (area->area_data + area_toc->size * SACD_LSN_SIZE))
   {
      if (strncmp((char*)p, "SACDTTxt", 8) == 0)
      {
         if (sacd_text_idx == 0)
         {
            area->area_text = (struct scarletbook_area_text*)p;
            for (i = 0; i < area_toc->track_count; i++)
            {
               char* track_ptr;
               uint8_t track_amount;

               SWAP16(area->area_text->track_text_position[i]);
               if (area->area_text->track_text_position[i] == 0)
               {
                  continue;
               }

               track_ptr = (char*)(p + area->area_text->track_text_position[i]);
               track_amount = (uint8_t)*track_ptr;
               track_ptr += 4;

               for (j = 0; j < track_amount; j++)
               {
                  uint8_t track_type = (uint8_t)*track_ptr++;
                  char* converted;
                  int track_text_len;

                  track_ptr++;
                  if (*track_ptr == 0)
                  {
                     goto next_track_text;
                  }

                  track_text_len = (int)strlen(track_ptr);
                  for (int te = 0; te < track_text_len; te++)
                  {
                     if ((uint8_t)track_ptr[te] < (uint8_t)0x20)
                     {
                        track_ptr[te] = (char)0x20;
                     }
                  }

                  converted = scarletbook_charset_convert(track_ptr, (size_t)track_text_len, current_charset, "UTF-8");
                  if (converted == NULL || converted[0] == '\0')
                  {
                     free(converted);
                     goto next_track_text;
                  }

                  switch (track_type)
                  {
                     case TRACK_TYPE_TITLE:
                        area->area_track_text[i].track_type_title = converted;
                        break;
                     case TRACK_TYPE_PERFORMER:
                        area->area_track_text[i].track_type_performer = converted;
                        break;
                     case TRACK_TYPE_SONGWRITER:
                        area->area_track_text[i].track_type_songwriter = converted;
                        break;
                     case TRACK_TYPE_COMPOSER:
                        area->area_track_text[i].track_type_composer = converted;
                        break;
                     case TRACK_TYPE_ARRANGER:
                        area->area_track_text[i].track_type_arranger = converted;
                        break;
                     case TRACK_TYPE_MESSAGE:
                        area->area_track_text[i].track_type_message = converted;
                        break;
                     case TRACK_TYPE_EXTRA_MESSAGE:
                        area->area_track_text[i].track_type_extra_message = converted;
                        break;
                     case TRACK_TYPE_TITLE_PHONETIC:
                        area->area_track_text[i].track_type_title_phonetic = converted;
                        break;
                     case TRACK_TYPE_PERFORMER_PHONETIC:
                        area->area_track_text[i].track_type_performer_phonetic = converted;
                        break;
                     case TRACK_TYPE_SONGWRITER_PHONETIC:
                        area->area_track_text[i].track_type_songwriter_phonetic = converted;
                        break;
                     case TRACK_TYPE_COMPOSER_PHONETIC:
                        area->area_track_text[i].track_type_composer_phonetic = converted;
                        break;
                     case TRACK_TYPE_ARRANGER_PHONETIC:
                        area->area_track_text[i].track_type_arranger_phonetic = converted;
                        break;
                     case TRACK_TYPE_MESSAGE_PHONETIC:
                        area->area_track_text[i].track_type_message_phonetic = converted;
                        break;
                     case TRACK_TYPE_EXTRA_MESSAGE_PHONETIC:
                        area->area_track_text[i].track_type_extra_message_phonetic = converted;
                        break;
                     default:
                        free(converted);
                        break;
                  }

next_track_text:
                  if (j < track_amount - 1)
                  {
                     while (*track_ptr != 0)
                     {
                        track_ptr++;
                     }

                     while (*track_ptr == 0)
                     {
                        track_ptr++;
                     }
                  }
               }
            }
         }

         sacd_text_idx++;
         p += SACD_LSN_SIZE;
      }
      else if (strncmp((char*)p, "SACD_IGL", 8) == 0)
      {
         area->area_isrc_genre = (struct scarletbook_area_isrc_genre*)p;
         p += SACD_LSN_SIZE * 2;
      }
      else if (strncmp((char*)p, "SACD_ACC", 8) == 0)
      {
         p += SACD_LSN_SIZE * 32;
      }
      else if (strncmp((char*)p, "SACDTRL1", 8) == 0)
      {
         area->area_tracklist_offset = (struct scarletbook_area_tracklist_offset*)p;
         for (i = 0; i < area_toc->track_count; i++)
         {
            SWAP32(area->area_tracklist_offset->track_start_lsn[i]);
            SWAP32(area->area_tracklist_offset->track_length_lsn[i]);
         }
         p += SACD_LSN_SIZE;
      }
      else if (strncmp((char*)p, "SACDTRL2", 8) == 0)
      {
         area->area_tracklist_time = (struct scarletbook_area_tracklist*)p;
         p += SACD_LSN_SIZE;
      }
      else
      {
         break;
      }
   }

   return area->area_tracklist_offset != NULL;
}

static void
scarletbook_frame_read_callback(struct scarletbook_handle* handle, uint8_t* frame_data, size_t frame_size, void* userdata)
{
   struct scarletbook_output_format* ft;

   ft = (struct scarletbook_output_format*)userdata;
   if (ft == NULL || ft->write_failed || ft->decoder_failed)
   {
      return;
   }

   if (handle != NULL && handle->frame.dst_encoded && ft->dst_decoder != NULL)
   {
      scarletbook_dst_decoder_decode(ft->dst_decoder, frame_data, frame_size);
      return;
   }

   if (scarletbook_dsf_write_frame(ft, frame_data, frame_size) < 0)
   {
      ft->write_failed = 1;
   }
}

static void
scarletbook_frame_decoded_callback(uint8_t* frame_data, size_t frame_size, void* userdata)
{
   struct scarletbook_output_format* ft;

   ft = (struct scarletbook_output_format*)userdata;
   if (ft == NULL || ft->write_failed || ft->decoder_failed)
   {
      return;
   }

   if (scarletbook_dsf_write_frame(ft, frame_data, frame_size) < 0)
   {
      ft->write_failed = 1;
   }
}

static void
scarletbook_frame_error_callback(int frame_count, int frame_error_code, const char* frame_error_message, void* userdata)
{
   struct scarletbook_output_format* ft;

   (void)frame_count;
   (void)frame_error_code;
   (void)frame_error_message;

   ft = (struct scarletbook_output_format*)userdata;
   if (ft != NULL)
   {
      ft->decoder_failed = 1;
   }
}

static int
scarletbook_process_frames(struct scarletbook_handle* handle, uint8_t* read_buffer, int blocks_read_in, int last_block,
                           frame_read_callback_t frame_read_callback_fn, void* userdata)
{
   int frame_info_idx;
   int nr_frames_processed = 0;
   int sector_bad_reads = 0;
   uint8_t packet_info_idx;
   uint8_t* read_buffer_ptr_blocks = read_buffer;
   uint8_t* read_buffer_ptr = read_buffer;

   for (int j = 0; j < blocks_read_in; j++)
   {
      memcpy(&handle->audio_sector.header, read_buffer_ptr, AUDIO_SECTOR_HEADER_SIZE);
      read_buffer_ptr += AUDIO_SECTOR_HEADER_SIZE;

      if (handle->audio_sector.header.packet_info_count > (uint8_t)7)
      {
         sector_bad_reads = 1;
         handle->frame.started = 0;
         read_buffer_ptr_blocks += SACD_LSN_SIZE;
         read_buffer_ptr = read_buffer_ptr_blocks;
         continue;
      }

      for (uint8_t i = 0; i < handle->audio_sector.header.packet_info_count; i++)
      {
         handle->audio_sector.packet[i].frame_start = (read_buffer_ptr[0] >> 7) & 1;
         handle->audio_sector.packet[i].data_type = (read_buffer_ptr[0] >> 3) & 7;
         handle->audio_sector.packet[i].packet_length = (uint16_t)(((read_buffer_ptr[0] & 7) << 8) | read_buffer_ptr[1]);
         read_buffer_ptr += AUDIO_PACKET_INFO_SIZE;
      }

      if (handle->audio_sector.header.dst_encoded)
      {
         if (handle->audio_sector.header.frame_info_count > 0)
         {
            memcpy(&handle->audio_sector.frame, read_buffer_ptr, AUDIO_FRAME_INFO_SIZE * handle->audio_sector.header.frame_info_count);
            read_buffer_ptr += AUDIO_FRAME_INFO_SIZE * handle->audio_sector.header.frame_info_count;
         }
      }
      else
      {
         for (uint8_t i = 0; i < handle->audio_sector.header.frame_info_count; i++)
         {
            memcpy(&handle->audio_sector.frame[i], read_buffer_ptr, AUDIO_FRAME_INFO_SIZE - 1);
            read_buffer_ptr += AUDIO_FRAME_INFO_SIZE - 1;
         }
      }

      handle->frame_info_idx = 0;
      frame_info_idx = 0;

      for (packet_info_idx = 0; packet_info_idx < handle->audio_sector.header.packet_info_count; packet_info_idx++)
      {
         struct scarletbook_audio_packet_info* packet = &handle->audio_sector.packet[packet_info_idx];

         if (packet->packet_length > MAX_PACKET_SIZE)
         {
            sector_bad_reads = 1;
            continue;
         }

         switch (packet->data_type)
         {
            case DATA_TYPE_AUDIO:
               if (packet->frame_start)
               {
                  if (handle->frame.started && handle->frame.size > 0)
                  {
                     if ((handle->frame.dst_encoded && handle->frame.sector_count == 0) ||
                         (!handle->frame.dst_encoded && handle->frame.size == handle->frame.channel_count * FRAME_SIZE_64))
                     {
                        scarletbook_exec_read_callback(handle, frame_read_callback_fn, userdata);
                        nr_frames_processed++;
                     }
                  }

                  handle->frame.size = 0;
                  handle->frame.dst_encoded = handle->audio_sector.header.dst_encoded;
                  handle->frame.sector_count = handle->audio_sector.frame[frame_info_idx].sector_count;
                  handle->frame.channel_count = scarletbook_get_channel_count(&handle->audio_sector.frame[frame_info_idx]);
                  handle->frame.started = 1;
                  handle->frame.timecode.minutes = handle->audio_sector.frame[frame_info_idx].timecode.minutes;
                  handle->frame.timecode.seconds = handle->audio_sector.frame[frame_info_idx].timecode.seconds;
                  handle->frame.timecode.frames = handle->audio_sector.frame[frame_info_idx].timecode.frames;
                  handle->frame_info_idx = frame_info_idx;
                  frame_info_idx++;
               }

               if (handle->frame.started)
               {
                  if (handle->frame.size + packet->packet_length <= MAX_DST_SIZE)
                  {
                     memcpy(handle->frame.data + handle->frame.size, read_buffer_ptr, packet->packet_length);
                     handle->frame.size += packet->packet_length;
                     if (handle->frame.dst_encoded)
                     {
                        handle->frame.sector_count--;
                     }
                  }
                  else
                  {
                     sector_bad_reads = 1;
                     handle->frame.started = 0;
                  }
               }
               break;
            case DATA_TYPE_SUPPLEMENTARY:
            case DATA_TYPE_PADDING:
            default:
               break;
         }

         read_buffer_ptr += packet->packet_length;
      }

      read_buffer_ptr_blocks += SACD_LSN_SIZE;
      read_buffer_ptr = read_buffer_ptr_blocks;
   }

   if (last_block && handle->frame.started && handle->frame.size > 0)
   {
      if ((handle->frame.dst_encoded && handle->frame.sector_count == 0) ||
          (!handle->frame.dst_encoded && handle->frame.size == handle->frame.channel_count * FRAME_SIZE_64))
      {
         scarletbook_exec_read_callback(handle, frame_read_callback_fn, userdata);
         nr_frames_processed++;
      }
   }

   if (sector_bad_reads > 0)
   {
      return -1;
   }

   return nr_frames_processed;
}

static char*
scarletbook_charset_convert(const char* input, size_t input_len, const char* from_charset, const char* to_charset)
{
   char* input_buffer;
   char* output = NULL;
   char* output_buffer;
   char* resized_output = NULL;
   iconv_t converter;
   size_t input_left;
   size_t output_capacity;
   size_t output_left;

   if (input == NULL || from_charset == NULL || to_charset == NULL)
   {
      return NULL;
   }

   converter = iconv_open(to_charset, from_charset);
   if (converter == (iconv_t)-1)
   {
      return NULL;
   }

   output_capacity = input_len == 0 ? 1 : (input_len * 4) + 1;
   output = (char*)malloc(output_capacity);
   if (output == NULL)
   {
      iconv_close(converter);
      return NULL;
   }

   input_buffer = (char*)input;
   input_left = input_len;
   output_buffer = output;
   output_left = output_capacity - 1;

   while (input_left > 0)
   {
      if (iconv(converter, &input_buffer, &input_left, &output_buffer, &output_left) != (size_t)-1)
      {
         continue;
      }

      if (errno == E2BIG)
      {
         size_t bytes_used = (size_t)(output_buffer - output);
         size_t new_capacity = output_capacity * 2;

         resized_output = (char*)realloc(output, new_capacity);
         if (resized_output == NULL)
         {
            free(output);
            iconv_close(converter);
            return NULL;
         }

         output = resized_output;
         output_buffer = output + bytes_used;
         output_left += new_capacity - output_capacity;
         output_capacity = new_capacity;
         continue;
      }

      free(output);
      iconv_close(converter);
      return NULL;
   }

   *output_buffer = '\0';
   iconv_close(converter);

   return output;
}

static int
scarletbook_dsf_create_header(struct scarletbook_output_format* ft)
{
   struct scarletbook_dsf_handle* handle;
   struct scarletbook_dsd_chunk_header* dsd_chunk;
   struct scarletbook_area_toc* area_toc;
   uint8_t* write_ptr;
   size_t bytes_written;

   handle = (struct scarletbook_dsf_handle*)ft->priv;
   area_toc = ft->sb_handle->area[ft->area].area_toc;
   if (handle == NULL || area_toc == NULL)
   {
      return -1;
   }

   if (handle->header == NULL)
   {
      handle->header = (uint8_t*)calloc(DSF_HEADER_FOOTER_SIZE, 1);
   }
   if (handle->footer == NULL)
   {
      handle->footer = (uint8_t*)calloc(DSF_HEADER_FOOTER_SIZE, 1);
   }
   if (handle->header == NULL || handle->footer == NULL)
   {
      return -1;
   }

   handle->header_size = 0;
   handle->footer_size = 0;

   write_ptr = handle->header;
   dsd_chunk = (struct scarletbook_dsd_chunk_header*)write_ptr;
   dsd_chunk->chunk_id = DSD_MARKER;
   dsd_chunk->chunk_data_size = htole64(sizeof(struct scarletbook_dsd_chunk_header));
   handle->header_size += sizeof(struct scarletbook_dsd_chunk_header);
   write_ptr += sizeof(struct scarletbook_dsd_chunk_header);

   {
      struct scarletbook_fmt_chunk* scarletbook_fmt_chunk = (struct scarletbook_fmt_chunk*)write_ptr;

      scarletbook_fmt_chunk->chunk_id = FMT_MARKER;
      scarletbook_fmt_chunk->chunk_data_size = htole64(sizeof(struct scarletbook_fmt_chunk));
      scarletbook_fmt_chunk->version = htole32(DSF_VERSION);
      scarletbook_fmt_chunk->format_id = htole32(FORMAT_ID_DSD);
      if (area_toc->channel_count == 2 && area_toc->extra_settings == 0)
      {
         scarletbook_fmt_chunk->channel_type = htole32(CHANNEL_TYPE_STEREO);
      }
      else if (area_toc->channel_count == 5 && area_toc->extra_settings == 3)
      {
         scarletbook_fmt_chunk->channel_type = htole32(CHANNEL_TYPE_5_CHANNELS);
      }
      else if (area_toc->channel_count == 6 && area_toc->extra_settings == 4)
      {
         scarletbook_fmt_chunk->channel_type = htole32(CHANNEL_TYPE_5_1_CHANNELS);
      }
      else
      {
         scarletbook_fmt_chunk->channel_type = htole32(CHANNEL_TYPE_STEREO);
      }
      scarletbook_fmt_chunk->channel_count = htole32(area_toc->channel_count);
      scarletbook_fmt_chunk->sample_frequency = htole32(SACD_SAMPLING_FREQUENCY);
      scarletbook_fmt_chunk->bits_per_sample = htole32(SACD_BITS_PER_SAMPLE);
      scarletbook_fmt_chunk->sample_count = htole64((handle->sample_count / (uint64_t)area_toc->channel_count) * 8U);
      scarletbook_fmt_chunk->block_size_per_channel = htole32(SACD_BLOCK_SIZE_PER_CHANNEL);
      scarletbook_fmt_chunk->reserved = 0;

      handle->channel_count = area_toc->channel_count;
      handle->header_size += sizeof(struct scarletbook_fmt_chunk);
      write_ptr += sizeof(struct scarletbook_fmt_chunk);
   }

   {
      struct scarletbook_data_chunk* scarletbook_data_chunk = (struct scarletbook_data_chunk*)write_ptr;
      scarletbook_data_chunk->chunk_id = DATA_MARKER;
      scarletbook_data_chunk->chunk_data_size = htole64(sizeof(struct scarletbook_data_chunk) + handle->audio_data_size);
      handle->header_size += sizeof(struct scarletbook_data_chunk);
   }

   dsd_chunk->total_file_size = htole64(handle->header_size + handle->audio_data_size + handle->footer_size);
   dsd_chunk->metadata_offset = htole64(0);

   bytes_written = fwrite(handle->header, 1, handle->header_size, ft->fd);
   return bytes_written == handle->header_size ? 0 : -1;
}

static int
scarletbook_dsf_create(struct scarletbook_output_format* ft)
{
   struct scarletbook_dsf_handle* handle;
   int result;

   handle = (struct scarletbook_dsf_handle*)ft->priv;
   if (handle == NULL)
   {
      return -1;
   }

   result = scarletbook_dsf_create_header(ft);
   for (int i = 0; i < MAX_CHANNEL_COUNT; i++)
   {
      handle->buffer_ptr[i] = &handle->buffer[i][0];
   }

   return result;
}

static int
scarletbook_dsf_write_frame(struct scarletbook_output_format* ft, const uint8_t* buf, size_t len)
{
   struct scarletbook_dsf_handle* handle;
   const uint8_t* buf_ptr;
   uint64_t prev_audio_data_size;
   size_t fill;
   size_t bytes_per_channel;
   size_t remaining;
   int channel_count;

   handle = (struct scarletbook_dsf_handle*)ft->priv;
   if (handle == NULL)
   {
      return -1;
   }

   buf_ptr = buf;
   prev_audio_data_size = handle->audio_data_size;
   channel_count = handle->channel_count;
   if (channel_count <= 0 || (len % (size_t)channel_count) != 0)
   {
      return -1;
   }

   fill = (size_t)(handle->buffer_ptr[0] - handle->buffer[0]);
   bytes_per_channel = len / (size_t)channel_count;
   remaining = bytes_per_channel;

   while (remaining > 0)
   {
      size_t chunk = SACD_BLOCK_SIZE_PER_CHANNEL - fill;
      if (chunk > remaining)
      {
         chunk = remaining;
      }

      if (channel_count == 6)
      {
         uint8_t* dst0 = handle->buffer[0] + fill;
         uint8_t* dst1 = handle->buffer[1] + fill;
         uint8_t* dst2 = handle->buffer[2] + fill;
         uint8_t* dst3 = handle->buffer[3] + fill;
         uint8_t* dst4 = handle->buffer[4] + fill;
         uint8_t* dst5 = handle->buffer[5] + fill;

         for (size_t i = 0; i < chunk; i++)
         {
            dst0[i] = bit_reverse_table[buf_ptr[0]];
            dst1[i] = bit_reverse_table[buf_ptr[1]];
            dst2[i] = bit_reverse_table[buf_ptr[2]];
            dst3[i] = bit_reverse_table[buf_ptr[3]];
            dst4[i] = bit_reverse_table[buf_ptr[4]];
            dst5[i] = bit_reverse_table[buf_ptr[5]];
            buf_ptr += 6;
         }
      }
      else if (channel_count == 2)
      {
         uint8_t* dst0 = handle->buffer[0] + fill;
         uint8_t* dst1 = handle->buffer[1] + fill;

         for (size_t i = 0; i < chunk; i++)
         {
            dst0[i] = bit_reverse_table[buf_ptr[0]];
            dst1[i] = bit_reverse_table[buf_ptr[1]];
            buf_ptr += 2;
         }
      }
      else
      {
         for (size_t sample_idx = 0; sample_idx < chunk; sample_idx++)
         {
            for (int channel_idx = 0; channel_idx < channel_count; channel_idx++)
            {
               handle->buffer[channel_idx][fill + sample_idx] = bit_reverse_table[*buf_ptr++];
            }
         }
      }

      fill += chunk;
      remaining -= chunk;

      if (fill == SACD_BLOCK_SIZE_PER_CHANNEL)
      {
         for (int channel_idx = 0; channel_idx < channel_count; channel_idx++)
         {
            size_t bytes_written = fwrite(handle->buffer[channel_idx], 1, SACD_BLOCK_SIZE_PER_CHANNEL, ft->fd);
            if (bytes_written != SACD_BLOCK_SIZE_PER_CHANNEL)
            {
               return -1;
            }

            handle->buffer_ptr[channel_idx] = handle->buffer[channel_idx];
         }

         handle->sample_count += (uint64_t)SACD_BLOCK_SIZE_PER_CHANNEL * (uint64_t)channel_count;
         handle->audio_data_size += (uint64_t)SACD_BLOCK_SIZE_PER_CHANNEL * (uint64_t)channel_count;
         fill = 0;
      }
   }

   for (int channel_idx = 0; channel_idx < channel_count; channel_idx++)
   {
      handle->buffer_ptr[channel_idx] = handle->buffer[channel_idx] + fill;
   }

   return (int)(handle->audio_data_size - prev_audio_data_size);
}

static int
scarletbook_dsf_close(struct scarletbook_output_format* ft)
{
   struct scarletbook_dsf_handle* handle;
   int result = 0;

   handle = (struct scarletbook_dsf_handle*)ft->priv;
   if (handle == NULL)
   {
      return -1;
   }

   for (int i = 0; i < handle->channel_count; i++)
   {
      if (handle->buffer_ptr[i] > handle->buffer[i])
      {
         size_t bytes_used = (size_t)(handle->buffer_ptr[i] - handle->buffer[i]);

         memset(handle->buffer[i] + bytes_used, 0, SACD_BLOCK_SIZE_PER_CHANNEL - bytes_used);
         size_t bytes_written = fwrite(handle->buffer[i], 1, SACD_BLOCK_SIZE_PER_CHANNEL, ft->fd);
         if (bytes_written != SACD_BLOCK_SIZE_PER_CHANNEL)
         {
            result = -1;
            break;
         }

         handle->sample_count += (uint64_t)bytes_used;
         handle->audio_data_size += SACD_BLOCK_SIZE_PER_CHANNEL;
         handle->buffer_ptr[i] = handle->buffer[i];
      }
   }

   if (result == 0)
   {
      if (fseek(ft->fd, 0, SEEK_SET) != 0)
      {
         result = -1;
      }
      else if (scarletbook_dsf_create_header(ft) != 0)
      {
         result = -1;
      }
   }

   free(handle->header);
   handle->header = NULL;
   free(handle->footer);
   handle->footer = NULL;

   return result;
}

static int
scarletbook_read_master_toc(struct scarletbook_handle* handle)
{
   int i;
   uint8_t* p;
   struct scarletbook_master_toc* master_toc = NULL;

   handle->master_data = malloc(MASTER_TOC_LEN * SACD_LSN_SIZE);
   if (!handle->master_data)
   {
      return 0;
   }

   if (!scarletbook_sacd_read_block_raw(handle->sacd, START_OF_MASTER_TOC, MASTER_TOC_LEN, handle->master_data))
   {
      return 0;
   }

   master_toc = handle->master_toc = (struct scarletbook_master_toc*)handle->master_data;

   if (strncmp("SACDMTOC", master_toc->id, 8) != 0)
   {
      goto error;
   }

   SWAP16(master_toc->album_set_size);
   SWAP16(master_toc->album_sequence_number);
   SWAP32(master_toc->area_1_toc_1_start);
   SWAP32(master_toc->area_1_toc_2_start);
   SWAP16(master_toc->area_1_toc_size);
   SWAP32(master_toc->area_2_toc_1_start);
   SWAP32(master_toc->area_2_toc_2_start);
   SWAP16(master_toc->area_2_toc_size);
   SWAP16(master_toc->disc_date_year);

   if (master_toc->version.major > SUPPORTED_VERSION_MAJOR || master_toc->version.minor > SUPPORTED_VERSION_MINOR)
   {
      goto error;
   }

   p = handle->master_data + SACD_LSN_SIZE;

   for (i = 0; i < MAX_LANGUAGE_COUNT; i++)
   {
      struct scarletbook_master_sacd_text* master_text = (struct scarletbook_master_sacd_text*)p;

      if (strncmp("SACDText", master_text->id, 8) != 0)
      {
         return 0;
      }

      SWAP16(master_text->album_title_position);
      SWAP16(master_text->album_title_phonetic_position);
      SWAP16(master_text->album_artist_position);
      SWAP16(master_text->album_artist_phonetic_position);
      SWAP16(master_text->album_publisher_position);
      SWAP16(master_text->album_publisher_phonetic_position);
      SWAP16(master_text->album_copyright_position);
      SWAP16(master_text->album_copyright_phonetic_position);
      SWAP16(master_text->disc_title_position);
      SWAP16(master_text->disc_title_phonetic_position);
      SWAP16(master_text->disc_artist_position);
      SWAP16(master_text->disc_artist_phonetic_position);
      SWAP16(master_text->disc_publisher_position);
      SWAP16(master_text->disc_publisher_phonetic_position);
      SWAP16(master_text->disc_copyright_position);
      SWAP16(master_text->disc_copyright_phonetic_position);

      if (i == 0)
      {
         char* current_charset = (char*)character_set[handle->master_toc->locales[i].character_set & 0x07];

         if (master_text->album_title_position)
         {
            handle->master_text.album_title =
               scarletbook_charset_convert((char*)master_text + master_text->album_title_position,
                                           strlen((char*)master_text + master_text->album_title_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->album_title_phonetic_position)
         {
            handle->master_text.album_title_phonetic =
               scarletbook_charset_convert((char*)master_text + master_text->album_title_phonetic_position,
                                           strlen((char*)master_text + master_text->album_title_phonetic_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->album_artist_position)
         {
            handle->master_text.album_artist =
               scarletbook_charset_convert((char*)master_text + master_text->album_artist_position,
                                           strlen((char*)master_text + master_text->album_artist_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->album_artist_phonetic_position)
         {
            handle->master_text.album_artist_phonetic =
               scarletbook_charset_convert((char*)master_text + master_text->album_artist_phonetic_position,
                                           strlen((char*)master_text + master_text->album_artist_phonetic_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->album_publisher_position)
         {
            handle->master_text.album_publisher =
               scarletbook_charset_convert((char*)master_text + master_text->album_publisher_position,
                                           strlen((char*)master_text + master_text->album_publisher_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->album_publisher_phonetic_position)
         {
            handle->master_text.album_publisher_phonetic =
               scarletbook_charset_convert((char*)master_text + master_text->album_publisher_phonetic_position,
                                           strlen((char*)master_text + master_text->album_publisher_phonetic_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->album_copyright_position)
         {
            handle->master_text.album_copyright =
               scarletbook_charset_convert((char*)master_text + master_text->album_copyright_position,
                                           strlen((char*)master_text + master_text->album_copyright_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->album_copyright_phonetic_position)
         {
            handle->master_text.album_copyright_phonetic =
               scarletbook_charset_convert((char*)master_text + master_text->album_copyright_phonetic_position,
                                           strlen((char*)master_text + master_text->album_copyright_phonetic_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->disc_title_position)
         {
            handle->master_text.disc_title =
               scarletbook_charset_convert((char*)master_text + master_text->disc_title_position,
                                           strlen((char*)master_text + master_text->disc_title_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->disc_title_phonetic_position)
         {
            handle->master_text.disc_title_phonetic =
               scarletbook_charset_convert((char*)master_text + master_text->disc_title_phonetic_position,
                                           strlen((char*)master_text + master_text->disc_title_phonetic_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->disc_artist_position)
         {
            handle->master_text.disc_artist =
               scarletbook_charset_convert((char*)master_text + master_text->disc_artist_position,
                                           strlen((char*)master_text + master_text->disc_artist_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->disc_artist_phonetic_position)
         {
            handle->master_text.disc_artist_phonetic =
               scarletbook_charset_convert((char*)master_text + master_text->disc_artist_phonetic_position,
                                           strlen((char*)master_text + master_text->disc_artist_phonetic_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->disc_publisher_position)
         {
            handle->master_text.disc_publisher =
               scarletbook_charset_convert((char*)master_text + master_text->disc_publisher_position,
                                           strlen((char*)master_text + master_text->disc_publisher_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->disc_publisher_phonetic_position)
         {
            handle->master_text.disc_publisher_phonetic =
               scarletbook_charset_convert((char*)master_text + master_text->disc_publisher_phonetic_position,
                                           strlen((char*)master_text + master_text->disc_publisher_phonetic_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->disc_copyright_position)
         {
            handle->master_text.disc_copyright =
               scarletbook_charset_convert((char*)master_text + master_text->disc_copyright_position,
                                           strlen((char*)master_text + master_text->disc_copyright_position),
                                           current_charset, "UTF-8");
         }

         if (master_text->disc_copyright_phonetic_position)
         {
            handle->master_text.disc_copyright_phonetic =
               scarletbook_charset_convert((char*)master_text + master_text->disc_copyright_phonetic_position,
                                           strlen((char*)master_text + master_text->disc_copyright_phonetic_position),
                                           current_charset, "UTF-8");
         }
      }

      p += SACD_LSN_SIZE;
   }

   handle->master_man = (struct scarletbook_master_man*)p;
   if (strncmp("SACD_Man", handle->master_man->id, 8) != 0)
   {
      return 0;
   }

error:

   return 1;
}

static uint32_t
scarletbook_sacd_input_read(struct scarletbook_sacd_input* dev, uint32_t pos, uint32_t blocks, void* buffer)
{
   off_t ret_lseek;
   size_t len;
   ssize_t ret;

   ret_lseek = lseek(dev->fd, (off_t)pos * (off_t)SACD_LSN_SIZE, SEEK_SET);
   if (ret_lseek < 0)
   {
      return 0;
   }

   len = (size_t)blocks * SACD_LSN_SIZE;

   ret = read(dev->fd, buffer, len);

   if (ret <= 0)
   {
      return 0;
   }

   if ((size_t)ret < len)
   {
      return ((uint32_t)ret) / SACD_LSN_SIZE;
   }

   return blocks;
}

static uint32_t
scarletbook_sacd_read_block_raw(struct scarletbook_sacd_reader* sacd, uint32_t lb_number, uint32_t block_count, uint8_t* data)
{
   if (!sacd->dev)
   {
      return 0;
   }

   return scarletbook_sacd_input_read(sacd->dev, lb_number, block_count, (void*)data);
}
