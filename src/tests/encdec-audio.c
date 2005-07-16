/*
   Copyright (C) 2003 Commonwealth Scientific and Industrial Research
   Organisation (CSIRO) Australia

   Redistribution and use in source and binary forms, with or without
   modification, are permitted provided that the following conditions
   are met:

   - Redistributions of source code must retain the above copyright
   notice, this list of conditions and the following disclaimer.

   - Redistributions in binary form must reproduce the above copyright
   notice, this list of conditions and the following disclaimer in the
   documentation and/or other materials provided with the distribution.

   - Neither the name of CSIRO Australia nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
   ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A
   PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE ORGANISATION OR
   CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
   EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
   PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
   PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
   LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
   NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
   SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <fishsound/fishsound.h>

#include "fs_tests.h"

#define DEBUG

#define DEFAULT_ITER 2
#define DEFAULT_ENCODE_QUALITY 0.1F

static void
usage (char * progname)
{
  printf ("Usage: %s [options]\n\n", progname);
  printf ("Options:\n");
  printf ("  --iter n                  Specify iterations per test (default %d)\n", DEFAULT_ITER);
  printf ("  --nasty                   Run with large test parameters\n");
  printf ("  --disable-vorbis          Disable testing of Vorbis codec\n");
  printf ("  --disable-speex           Disable testing of Speex codec\n");
  printf ("  --disable-interleave      Disable testing of interleave\n");
  printf ("  --disable-non-interleave  Disable testing of non-interleave\n");
  exit (1);
}

/** PCM type */
typedef enum {
  /** Undefined/Error */
  FISH_SOUND_PCM_UNDEF = 0x00,

  /** short */
  FISH_SOUND_PCM_SHORT = 0x01,

  /** int */
  FISH_SOUND_PCM_INT = 0x02,

  /** float */
  FISH_SOUND_PCM_FLOAT = 0x03,

  /** double */
  FISH_SOUND_PCM_DOUBLE = 0x04

} FishSoundPCM;

static char * pcm_name[] = {
  "UNDEF",
  "short",
  "int",
  "float",
  "double"
};

/* For one-time tests, configure these by commandline args */
static int * test_blocksizes, * test_samplerates, * test_channels;
static int iter = DEFAULT_ITER;
static int test_vorbis = HAVE_VORBIS, test_speex = HAVE_SPEEX;
static int test_interleave = 1, test_non_interleave = 1;

static int nasty_blocksizes[] = {128, 256, 512, 1024, 2048, 4096, 0};
static int nasty_samplerates[] = {8000, 16000, 32000, 48000, 0};
static int nasty_channels[] = {1, 2, 4, 5, 6, 8, 10, 16, 32, 0};

static int default_blocksizes[] = {128, 1024, 0};
static int default_samplerates[] = {8000, 48000, 0};
static int default_channels[] = {1, 2, 6, 16, 0};

typedef struct {
  FishSound * encoder;
  FishSound * decoder;
  int interleave;
  int channels;
  int retval;
  union {
    short ** s;
    int ** i;
    float ** f;
    double ** d;
  } pcm;
  long frames_in;
  long frames_out;
} FS_EncDec;

static const char *
stopctl_string (FishSoundStopCtl stopctl)
{
  switch (stopctl) {
  case FISH_SOUND_CONTINUE:
    return "C";
  case FISH_SOUND_STOP_OK:
    return "O";
  case FISH_SOUND_STOP_ERR:
    return "E";
  default:
    FAIL("stopctl_string(): Unknown FishSoundStopCtl method");
  }
}

static int
decoded_short (FishSound * fsound, short ** pcm, long frames, void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;

  ed->frames_out += frames;

  return ed->retval;
}

static int
decoded_short_ilv (FishSound * fsound, short * pcm[], long frames,
		   void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;

  ed->frames_out += frames;

  return ed->retval;
}

static int
decoded_int (FishSound * fsound, int ** pcm, long frames, void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;

  ed->frames_out += frames;

  return ed->retval;
}

static int
decoded_int_ilv (FishSound * fsound, int * pcm[], long frames,
		   void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;

  ed->frames_out += frames;

  return ed->retval;
}

static int
decoded_float (FishSound * fsound, float ** pcm, long frames, void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;

  ed->frames_out += frames;

  return ed->retval;
}

static int
decoded_float_ilv (FishSound * fsound, float * pcm[], long frames,
		   void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;

  ed->frames_out += frames;

  return ed->retval;
}

static int
decoded_double (FishSound * fsound, double ** pcm, long frames,
		void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;

  ed->frames_out += frames;

  return ed->retval;
}

static int
decoded_double_ilv (FishSound * fsound, double * pcm[], long frames,
		   void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;

  ed->frames_out += frames;

  return ed->retval;
}

static int
encoded (FishSound * fsound, unsigned char * buf, long bytes, void * user_data)
{
  FS_EncDec * ed = (FS_EncDec *) user_data;
  long ret = ed->retval, bytes_decoded = 0;

  switch (ed->retval) {
  case FISH_SOUND_CONTINUE:
    bytes_decoded = fish_sound_decode (ed->decoder, buf, bytes);
    if (bytes_decoded != bytes)
      FAIL ("FISH_SOUND_CONTINUE: Incorrect byte count decoded");
    break;
  case FISH_SOUND_STOP_OK:
    while (bytes_decoded < bytes &&
	   (ret == FISH_SOUND_ERR_STOP_OK || ret > 0)) {
      ret = fish_sound_decode (ed->decoder, buf+bytes_decoded,
			       bytes-bytes_decoded);
      if (ret > 0) bytes_decoded += ret;
    }
    if (ret != FISH_SOUND_ERR_STOP_OK && ret < 0)
      FAIL ("FISH_SOUND_STOP_OK: Bad return value from fish_sound_decode");
    if (bytes_decoded != bytes)
      FAIL ("FISH_SOUND_STOP_OK: Incorrect byte count decoded");
    break;
  case FISH_SOUND_STOP_ERR:
    while (bytes_decoded < bytes &&
	   (ret == FISH_SOUND_ERR_STOP_ERR || ret > 0)) {
      ret = fish_sound_decode (ed->decoder, buf+bytes_decoded,
			       bytes-bytes_decoded);
      if (ret > 0) bytes_decoded += ret;
    }
    if (ret != FISH_SOUND_ERR_STOP_ERR && ret < -1) {
      fprintf (stderr, "STOP_ERR: retval %d\n", ret);
      FAIL ("FISH_SOUND_STOP_ERR: Bad return value from fish_sound_decode");
    }
    if (bytes_decoded > bytes)
      FAIL ("FISH_SOUND_STOP_ERR: Bad byte count decoded");
    break;
  default:
    break;
  }

  return 0;
}

/* Fill a PCM buffer with a squarish wave */
static void
fs_fill_square_short (short * pcm, int length)
{
  short value = 15000;
  int i;

  for (i = 0; i < length; i++) {
    pcm[i] = value;
    if ((i % 100) == 0) {
      value = -value;
    }
  }
}

static void
fs_fill_square_int (int * pcm, int length)
{
  int value = 1000000000;
  int i;

  for (i = 0; i < length; i++) {
    pcm[i] = value;
    if ((i % 100) == 0) {
      value = -value;
    }
  }
}

static void
fs_fill_square_float (float * pcm, int length)
{
  float value = 0.5;
  int i;

  for (i = 0; i < length; i++) {
    pcm[i] = value;
    if ((i % 100) == 0) {
      value = -value;
    }
  }
}

static void
fs_fill_square_double (double * pcm, int length)
{
  double value = 0.5;
  int i;

  for (i = 0; i < length; i++) {
    pcm[i] = value;
    if ((i % 100) == 0) {
      value = -value;
    }
  }
}

static FS_EncDec *
fs_encdec_new (FishSoundPCM pcm_type, int samplerate, int channels,
	       int format, int interleave, int blocksize, int retval)
{
  FS_EncDec * ed;
  FishSoundInfo fsinfo;
  int i;

  ed = malloc (sizeof (FS_EncDec));

  fsinfo.samplerate = samplerate;
  fsinfo.channels = channels;
  fsinfo.format = format;

  ed->encoder = fish_sound_new (FISH_SOUND_ENCODE, &fsinfo);
  ed->decoder = fish_sound_new (FISH_SOUND_DECODE, &fsinfo);

  fish_sound_set_interleave (ed->encoder, interleave);
  fish_sound_set_interleave (ed->decoder, interleave);

  fish_sound_set_encoded_callback (ed->encoder, encoded, ed);

  if (fish_sound_set_encode_quality (ed->encoder, DEFAULT_ENCODE_QUALITY) != 0)
    FAIL ("Error setting encode quality");
  if (fish_sound_get_encode_quality (ed->encoder) != DEFAULT_ENCODE_QUALITY)
    FAIL ("Error retrieving previously set encode quality");

  ed->interleave = interleave;
  ed->channels = channels;
  ed->retval = retval;

  switch (pcm_type) {
  case FISH_SOUND_PCM_SHORT:
    if (interleave) {
      fish_sound_set_decoded_short_ilv (ed->decoder, decoded_short_ilv, ed);
      ed->pcm.s = (short **) malloc (sizeof (short) * channels * blocksize);
      fs_fill_square_short ((short *)ed->pcm.s, channels * blocksize);
    } else {
      fish_sound_set_decoded_short (ed->decoder, decoded_short, ed);
      ed->pcm.s = (short **) malloc (sizeof (short *) * channels);
      for (i = 0; i < channels; i++) {
	ed->pcm.s[i] = (short *) malloc (sizeof (short) * blocksize);
	fs_fill_square_short (ed->pcm.s[i], blocksize);
      }
    }

    break;
  case FISH_SOUND_PCM_INT:
    if (interleave) {
      fish_sound_set_decoded_int_ilv (ed->decoder, decoded_int_ilv, ed);
      ed->pcm.i = (int **) malloc (sizeof (int) * channels * blocksize);
      fs_fill_square_int ((int *)ed->pcm.i, channels * blocksize);
    } else {
      fish_sound_set_decoded_int (ed->decoder, decoded_int, ed);
      ed->pcm.i = (int **) malloc (sizeof (int *) * channels);
      for (i = 0; i < channels; i++) {
	ed->pcm.i[i] = (int *) malloc (sizeof (int) * blocksize);
	fs_fill_square_int (ed->pcm.i[i], blocksize);
      }
    }
    break;
  case FISH_SOUND_PCM_FLOAT:
    if (interleave) {
      fish_sound_set_decoded_float_ilv (ed->decoder, decoded_float_ilv, ed);
      ed->pcm.f = (float **) malloc (sizeof (float) * channels * blocksize);
      fs_fill_square_float ((float *)ed->pcm.f, channels * blocksize);
    } else {
      fish_sound_set_decoded_float (ed->decoder, decoded_float, ed);
      ed->pcm.f = (float **) malloc (sizeof (float *) * channels);
      for (i = 0; i < channels; i++) {
	ed->pcm.f[i] = (float *) malloc (sizeof (float) * blocksize);
	fs_fill_square_float (ed->pcm.f[i], blocksize);
      }
    }
    break;
  case FISH_SOUND_PCM_DOUBLE:
    if (interleave) {
      fish_sound_set_decoded_double_ilv (ed->decoder, decoded_double_ilv, ed);
      ed->pcm.d = (double **) malloc (sizeof (double) * channels * blocksize);
      fs_fill_square_double ((double *)ed->pcm.d, channels * blocksize);
    } else {
      fish_sound_set_decoded_double (ed->decoder, decoded_double, ed);
      ed->pcm.d = (double **) malloc (sizeof (double *) * channels);
      for (i = 0; i < channels; i++) {
	ed->pcm.d[i] = (double *) malloc (sizeof (double) * blocksize);
	fs_fill_square_double (ed->pcm.d[i], blocksize);
      }
    }
    break;
  default:
    break;
  }

  ed->frames_in = 0;
  ed->frames_out = 0;

  return ed;
}

static int
fs_encdec_delete (FS_EncDec * ed)
{
  int i;

  if (!ed) return -1;

  fish_sound_delete (ed->encoder);
  fish_sound_delete (ed->decoder);

  if (!ed->interleave) {
    for (i = 0; i < ed->channels; i++)
      free (ed->pcm.f[i]);
  }
  free (ed->pcm.f);
  
  free (ed);

  return 0;
}

static int
fs_encdec_test (FishSoundPCM pcm_type, int samplerate, int channels,
		int format, int interleave, int blocksize, int retval)
{
  FS_EncDec * ed;
  char msg[128];
  int i;

  snprintf (msg, 128,
	    "+ %2d channel %6d Hz %s %d frame %-6s (%s) [%s]",
	    channels, samplerate,
	    format == FISH_SOUND_VORBIS ? "Vorbis," : "Speex, ",
	    blocksize, pcm_name[pcm_type],
	    interleave ? "interleave" : "non-interleave",
	    stopctl_string(retval));
  INFO (msg);
  
  ed = fs_encdec_new (pcm_type, samplerate, channels, format,
		      interleave, blocksize, retval);

  for (i = 0; i < iter; i++) {
    ed->frames_in += blocksize;
    fish_sound_prepare_truncation (ed->encoder, ed->frames_in,
				   (i == (iter - 1)));
    switch (pcm_type) {
    case FISH_SOUND_PCM_SHORT:
      if (interleave) {
	fish_sound_encode_short_ilv (ed->encoder, ed->pcm.s, blocksize);
      } else {
	fish_sound_encode_short (ed->encoder, ed->pcm.s, blocksize);
      }
      break;
    case FISH_SOUND_PCM_INT:
      if (interleave) {
	fish_sound_encode_int_ilv (ed->encoder, ed->pcm.i, blocksize);
      } else {
	fish_sound_encode_int (ed->encoder, ed->pcm.i, blocksize);
      }
      break;
    case FISH_SOUND_PCM_FLOAT:
      if (interleave) {
	fish_sound_encode_float_ilv (ed->encoder, ed->pcm.f, blocksize);
      } else {
	fish_sound_encode_float (ed->encoder, ed->pcm.f, blocksize);
      }
      break;
    case FISH_SOUND_PCM_DOUBLE:
      if (interleave) {
	fish_sound_encode_double_ilv (ed->encoder, ed->pcm.d, blocksize);
      } else {
	fish_sound_encode_double (ed->encoder, ed->pcm.d, blocksize);
      }
      break;
    default:
      break;
    }
  }

  fish_sound_flush (ed->encoder);

  if (ed->frames_in != ed->frames_out) {
    snprintf (msg, 128,
	      "%ld frames encoded, %ld frames decoded",
	      ed->frames_in, ed->frames_out);
    if (retval == FISH_SOUND_CONTINUE && ed->frames_out < ed->frames_in) {
      FAIL (msg);
    } else {
      WARN (msg);
    }
  }

  fs_encdec_delete (ed);

  return 0;
}

static void
parse_args (int argc, char * argv[])
{
  int i;

  for (i = 1; i < argc; i++) {
    if (!strcmp (argv[i], "--nasty")) {
      test_blocksizes = nasty_blocksizes;
      test_samplerates = nasty_samplerates;
      test_channels = nasty_channels;
    } else if (!strcmp (argv[i], "--iter")) {
      i++; if (i >= argc) usage(argv[0]);
      iter = atoi (argv[i]);
    } else if (!strcmp (argv[i], "--disable-vorbis")) {
      test_vorbis = 0;
    } else if (!strcmp (argv[i], "--disable-speex")) {
      test_speex = 0;
    } else if (!strcmp (argv[i], "--disable-interleave")) {
      test_interleave = 0;
    } else if (!strcmp (argv[i], "--disable-non-interleave")) {
      test_non_interleave = 0;
    } else if (!strcmp (argv[i], "--help") || !strcmp (argv[i], "-h")) {
      usage(argv[0]);
    }
  }

  INFO ("Testing encode/decode pipeline for audio");

  /* Report abnormal options */

  if (test_blocksizes == nasty_blocksizes)
    INFO ("* Running NASTY large test parameters");

  if (!test_vorbis) INFO ("* DISABLED testing of Vorbis");
  if (!test_speex) INFO ("* DISABLED testing of Speex");
  if (!test_interleave) INFO ("* DISABLED testing of INTERLEAVE");
  if (!test_non_interleave) INFO ("* DISABLED testing of NON-INTERLEAVE");
}

int
main (int argc, char * argv[])
{
  FishSoundPCM pcm_type;
  int b, s, c;

  test_blocksizes = default_blocksizes;
  test_samplerates = default_samplerates;
  test_channels = default_channels;

  parse_args (argc, argv);
  
  for (b = 0; test_blocksizes[b]; b++) {
    for (s = 0; test_samplerates[s]; s++) {
      for (c = 0; test_channels[c]; c++) {
	for (pcm_type = 0x01; pcm_type <= 0x04; pcm_type++) {

	  if (test_non_interleave) {
	    /* Test VORBIS */
	    if (test_vorbis) {
	      fs_encdec_test (pcm_type, test_samplerates[s], test_channels[c],
			      FISH_SOUND_VORBIS, 0, test_blocksizes[b],
			      FISH_SOUND_CONTINUE);
	    }
	  
	    /* Test SPEEX */
	    if (test_speex) {
	      if (test_channels[c] <= 2) {
		fs_encdec_test (pcm_type, test_samplerates[s], test_channels[c],
				FISH_SOUND_SPEEX, 0, test_blocksizes[b],
				FISH_SOUND_CONTINUE);
	      
		fs_encdec_test (pcm_type, test_samplerates[s], test_channels[c],
				FISH_SOUND_SPEEX, 0, test_blocksizes[b],
				FISH_SOUND_STOP_OK);
		fs_encdec_test (pcm_type, test_samplerates[s], test_channels[c],
				FISH_SOUND_SPEEX, 0, test_blocksizes[b],
				FISH_SOUND_STOP_ERR);
	      }
	    }
	  }

	  if (test_interleave) {
	    /* Test VORBIS */
	    if (test_vorbis) {
	      fs_encdec_test (pcm_type, test_samplerates[s], test_channels[c],
			      FISH_SOUND_VORBIS, 1, test_blocksizes[b],
			      FISH_SOUND_CONTINUE);
	    }
	  
	    /* Test SPEEX */
	    if (test_speex) {
	      if (test_channels[c] <= 2) {
		fs_encdec_test (pcm_type, test_samplerates[s], test_channels[c],
				FISH_SOUND_SPEEX, 1, test_blocksizes[b],
				FISH_SOUND_CONTINUE);
	      
		fs_encdec_test (pcm_type, test_samplerates[s], test_channels[c],
				FISH_SOUND_SPEEX, 1, test_blocksizes[b],
				FISH_SOUND_STOP_OK);
		fs_encdec_test (pcm_type, test_samplerates[s], test_channels[c],
				FISH_SOUND_SPEEX, 1, test_blocksizes[b],
				FISH_SOUND_STOP_ERR);
	      }
	    }
	  }
	}
      }
    }
  }

  exit (0);
}
