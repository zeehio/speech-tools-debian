 /*************************************************************************/
 /*                                                                       */
 /*                Centre for Speech Technology Research                  */
 /*                     University of Edinburgh, UK                       */
 /*                      Copyright (c) 1995,1996                          */
 /*                        All Rights Reserved.                           */
 /*                                                                       */
 /*  Permission is hereby granted, free of charge, to use and distribute  */
 /*  this software and its documentation without restriction, including   */
 /*  without limitation the rights to use, copy, modify, merge, publish,  */
 /*  distribute, sublicense, and/or sell copies of this work, and to      */
 /*  permit persons to whom this work is furnished to do so, subject to   */
 /*  the following conditions:                                            */
 /*   1. The code must retain the above copyright notice, this list of    */
 /*      conditions and the following disclaimer.                         */
 /*   2. Any modifications must be clearly marked as such.                */
 /*   3. Original authors' names are not deleted.                         */
 /*   4. The authors' names are not used to endorse or promote products   */
 /*      derived from this software without specific prior written        */
 /*      permission.                                                      */
 /*                                                                       */
 /*  THE UNIVERSITY OF EDINBURGH AND THE CONTRIBUTORS TO THIS WORK        */
 /*  DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING      */
 /*  ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO EVENT   */
 /*  SHALL THE UNIVERSITY OF EDINBURGH NOR THE CONTRIBUTORS BE LIABLE     */
 /*  FOR ANY SPECIAL, INDIRECT OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES    */
 /*  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN   */
 /*  AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION,          */
 /*  ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF       */
 /*  THIS SOFTWARE.                                                       */
 /*                                                                       */
 /*************************************************************************/
 /*                                                                       */
 /*                Author :  Richard Caley                                */
 /* -------------------------------------------------------------------   */
 /* EST audio module using the enlightenment speech daemon.               */
 /*                                                                       */
 /*************************************************************************/


#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cctype>
#include <unistd.h>
#include <sstream>
#include <sys/stat.h>
#include "EST_Wave.h"
#include "EST_Option.h"
#include "audioP.h"
#include "EST_io_aux.h"

using namespace std;

#ifdef SUPPORT_ESD

// Hack hack. aupvlist.h is broken at least on FBSD 3.1.1

#undef __cplusplus

extern "C"
{
#include <aupvlist.h>
}
#define __cplusplus

#include <esd.h>
// Code to block signals while sound is playing.
// Needed inside Java on (at least some) linux systems
// as scheduling interrupts seem to break the writes.

#ifdef THREAD_SAFETY
#include <signal.h>
#include <pthread.h>

#define THREAD_DECS() \
    sigset_t  oldmask \

#define THREAD_PROTECT() do { \
    sigset_t  newmask; \
    \
    sigfillset(&newmask); \
    \
    pthread_sigmask(SIG_BLOCK, &newmask, &oldmask); \
    } while(0)

#define THREAD_UNPROTECT() do { \
     pthread_sigmask(SIG_SETMASK, &oldmask, NULL); \
     } while (0)

#else
#define  THREAD_DECS() //empty
#define  THREAD_PROTECT() //empty
#define  THREAD_UNPROTECT() //empty
#endif


#define au_serverrate 16000

bool esd_supported = TRUE;

static int endian_int = 1;
#define ESD_BIG_ENDIAN (((char *)&endian_int)[0] == 0)

EST_String server;

void get_server(EST_Option &al)
{
  if (al.present("-display"))
    {
      EST_String display = al.val("-display");
      if (display.contains(":"))
	{
	  server = display.before(":");
	}
      else
	{
	  server = display;
	}
    }
  else
    server = "";
}

int play_esd_wave(EST_Wave &inwave, EST_Option &al)
{

  get_server(al);

  int format;

  switch (inwave.num_channels())
    {
    case 1: format=ESD_MONO;
      break;
      
    case 2: format=ESD_STEREO;
      break;

    default:
      cerr << "EST: " << inwave.num_channels() << " channel data not supported\n";
      return -1;
    }

  format |= ESD_BITS16 | ESD_STREAM | ESD_PLAY;
      
  int sample_rate = inwave.sample_rate();
  THREAD_DECS();
  THREAD_PROTECT();

  stringstream esd_name_stream;
  esd_name_stream << "from est/" << getpid() << '\0';
  // esd_name_stream.freeze();
  const char *esd_name = esd_name_stream.str().c_str();


  int esd =  esd_play_stream( format, sample_rate, 
			      server==EST_String::Empty?NULL:(const char *)server, esd_name);

  int n = inwave.num_samples() * sizeof(short) * inwave.num_channels();
  int nw=0, tot=0;
  const char *data = (const char *)(inwave.values().memory());

  while(n > 0 && (nw = write(esd, data+tot, n)) >0)
    {
      n -= nw;
      tot+=nw;
    }

  if (nw < 0)
    {
      cerr << "ESD: error writing - " << strerror(errno) << "\n";
    }

  esd_close(esd);
  int monitor = esd_open_sound(server==EST_String::Empty?NULL:(const char *)server);
  for (;;)
    {
      esd_info_t *info = esd_get_all_info(monitor);
      if (!info)
        {
          break;
        }
      esd_player_info_t *player = info->player_list;
      int done = 1;
      while (player)
        {
          if (strcmp(player->name, esd_name) == 0)
            {
              done = 0;
              break;
            }

          player = player->next;
        }

      if (done)
        {
          break;
        }

      esd_free_all_info(info);
      usleep(10000L);
    }
  esd_close(monitor);

  THREAD_UNPROTECT();

  return 1;
}

int record_esd_wave(EST_Wave &wave, EST_Option &al)
{
    (void)wave;
    (void)al;

    cerr << "ESD: record not written yet\n";
    return -1;
}

#else
int esd_supported = FALSE;

int play_esd_wave(EST_Wave &inwave, EST_Option &al)
{
    (void)inwave;
    (void)al;
    cerr << "ESD playback not supported" << endl;
    return -1;
}


int record_esd_wave(EST_Wave &wave, EST_Option &al)
{
    (void)wave;
    (void)al;
    cerr << "ESD record not supported" << endl;
    return -1;
}

#endif
