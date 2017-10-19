#include "gme.h"
#include "Effects_Buffer.h"
#include <stdlib.h>
#include <stdio.h>

#include <Windows.h>
#include "..\xmpfunc.h"
#include "..\xmpin.h"


gme_t* emu = NULL;
Effects_Buffer *effects = NULL;
gme_info_t *info = NULL;
int tracklen = 0;
int subsong = 0;

static HINSTANCE hInst;
static const XMPFUNC_MISC *xmpfmisc;
static const XMPFUNC_REGISTRY *xmpfreg;
static const XMPFUNC_FILE *xmpffile;
static const XMPFUNC_IN *xmpfin;

static BOOL WINAPI GME_CheckFile(const char *filename, XMPFILE file)
{
	gme_t *emu = NULL;
	byte *data  = (byte*)xmpfmisc->Alloc(xmpffile->GetSize(file));
	int filesize = xmpffile->Read(file, data, xmpffile->GetSize(file));
	gme_err_t check = gme_open_data((void*)data,filesize,&emu,gme_info_only);
	if (check != NULL)
	{
		xmpfmisc->Free(data);
		gme_delete(emu);
		emu = NULL;
		return false;
	}
	xmpfmisc->Free(data);
	if(emu)
	{
		gme_delete(emu);
		emu = NULL;
	}
	return true;
}

static void GetTag(const char *src, char **dest)
{
	int len;
	if (src[0] == '\0')
		return;
	len = strlen(src) + 1;
	*dest = (char*)xmpfmisc->Alloc(len);
	memcpy(*dest, src, len);
}

int songlen(int song, gme_t *emu, gme_info_t *info)
{
	gme_track_info( emu, &info,song );
	if ( info->length <= 0 && info->loop_length <=0){
		info->length = (long) (3.0 * 60 * 1000);
	}else{
		if ( info->loop_length > 0)info->length = info->intro_length + info->loop_length;
	}
	return info->length;
}

static void GetTotalLength(float *length)
{
	int total_duration = 0;
	int song;
	for (song = 0; song < gme_track_count(emu); song++) {
		int duration = songlen(song,emu,info);
		if (duration < 0) {
			*length = 0;
			return;
		}
		total_duration += duration;
	}
	*length = total_duration / 1000.0;
}

static BOOL WINAPI GME_GetTags(char *tags[8])
{
	gme_track_info(emu, &info, subsong);
	GetTag(info->song,tags);
	GetTag(info->author,tags +1);
	GetTag(info->game,tags + 2);
	GetTag(info->comment, tags + 6);
	return TRUE;
}

static BOOL WINAPI GME_GetFileInfo(const char *filename, XMPFILE file, float *length, char *tags[8])
{
	gme_t *emu = NULL;
	gme_info_t *infofile = NULL;
	byte *data  = (byte*)xmpfmisc->Alloc(xmpffile->GetSize(file));
	int filesize = xmpffile->Read(file, data, xmpffile->GetSize(file));
	gme_err_t check = gme_open_data((void*)data,filesize,&emu,gme_info_only);
	if (check != NULL)
	{
		xmpfmisc->Free(data);
		gme_delete(emu);
		emu = NULL;
		return false;
	}
	
	gme_track_info( emu, &infofile,0 );
	*length = songlen(0, emu, infofile)/1000.0;
	GetTag(infofile->song,tags);
	GetTag(infofile->author,tags +1);
	GetTag(infofile->game,tags + 2);
	GetTag(infofile->comment, tags + 6);
	GetTag(gme_identify_header(data), tags + 7);
	xmpfmisc->Free(data);
	if (emu)
	{
		gme_delete(emu);
		emu = NULL;
	}
	return TRUE;
}

static DWORD WINAPI GME_Open(const char *filename, XMPFILE file)
{
	byte *data  = (byte*)xmpfmisc->Alloc(xmpffile->GetSize(file));
	int filesize = xmpffile->Read(file, data, xmpffile->GetSize(file));
	gme_err_t check =  gme_open_data((void*)data,filesize, &emu, 48000);
	gme_set_stereo_depth(emu, 0.5);
	if (check !=NULL)
	{
		xmpfmisc->Free(data);
		if (emu)
		{
			gme_delete(emu);
			emu = NULL;
		}
	}
	subsong = 0;
	gme_track_info(emu, &info, subsong);
	float len = songlen(subsong, emu, info) / 1000.0;
	tracklen = songlen(subsong, emu, info);
	if (len > 0) xmpfin->SetLength(len, TRUE);
	gme_start_track(emu, subsong);
	xmpfmisc->Free(data);
	return 2; /* close file */
}

static void WINAPI GME_Close()
{
	gme_delete(emu);
	emu = NULL;
	subsong = 0;
}

static void WINAPI GME_SetFormat(XMPFORMAT *form)
{
	form->rate = 48000;
	form->chan = 2;
	form->res = 16 / 8;
}

static void WINAPI GME_GetInfoText(char *format, char *length)
{
	if (format != NULL)strcpy(format, info->system);
}

static void WINAPI GME_GetGeneralInfo(char *buf)
{
	buf = NULL;
}

static int loop_ms = 5000; //5 seconds
static DWORD WINAPI GME_Process(float *buf, DWORD count)
{
	DWORD n = count;
	short *buf2 = (short *) buf;
	DWORD option_loop = xmpfmisc->GetConfig(XMPCONFIG_LOOP)&3;
	if (!option_loop)
	{
		if (gme_track_ended(emu))return 0;
		if (info->length <= 0 && info->loop_length <= 0)
			gme_set_fade(emu, tracklen - loop_ms, loop_ms);
		else
			gme_set_fade(emu, tracklen, 0);
		gme_ignore_silence(emu,false);
	}
	else
	{
		//negative value disables fade
		gme_set_fade(emu, -1, 0);
		gme_ignore_silence(emu,true);
	}

	gme_play( emu, count, buf2 );
	int i;
	for (i = n; --i >= 0; )
		buf[i] = buf2[i] / 32767.0;
	return n;	
}

static double WINAPI GME_SetPosition(DWORD pos)
{
	int song = pos - XMPIN_POS_SUBSONG;
	if (song >= 0 && song < gme_track_count(emu)) {
		subsong = song;
		gme_track_info(emu, &info, subsong);
		float len = songlen(subsong, emu, info) / 1000.0;
		tracklen = songlen(subsong, emu, info);
		if (len > 0) xmpfin->SetLength(len, TRUE);
		gme_start_track(emu, subsong);
		//request title update too, could be something like a .nsfe
		xmpfin->UpdateTitle(NULL);
		return 0;
	}
	gme_seek(emu,pos);
	return pos / 1000.0;
}

static double WINAPI GME_GetGranularity()
{
	return 0.001;
}

static DWORD WINAPI GME_GetSubSongs(float *length)
{
	GetTotalLength(length);
	return gme_track_count(emu);
}

extern "C" XMPIN *WINAPI XMPIN_GetInterface(DWORD face, InterfaceProc faceproc)
{
	static XMPIN xmpin = {
		NULL,
		"GME (rev .41fork)",
		"Console music files\0ay/gbs/gym/hes/kss/nsf/nsfe/rsn/sap/sgc/spc/sfm/vgm/vgz",
		NULL,
		NULL,
		GME_CheckFile,
		GME_GetFileInfo,
		GME_Open,
		GME_Close,
		NULL,
		GME_SetFormat,
		GME_GetTags,
		GME_GetInfoText,
		GME_GetGeneralInfo,
		NULL,
		GME_SetPosition,
		GME_GetGranularity,
		NULL,
		GME_Process,
		NULL,
		NULL,
		GME_GetSubSongs,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL,
		NULL
	};
	if (face != XMPIN_FACE)
		return NULL;

	xmpfmisc = (const XMPFUNC_MISC *) faceproc(XMPFUNC_MISC_FACE);
	xmpfreg = (const XMPFUNC_REGISTRY *) faceproc(XMPFUNC_REGISTRY_FACE);
	xmpffile = (const XMPFUNC_FILE *) faceproc(XMPFUNC_FILE_FACE);
	xmpfin = (const XMPFUNC_IN *) faceproc(XMPFUNC_IN_FACE);
	return &xmpin;
}

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID lpReserved)
{
	if (dwReason == DLL_PROCESS_ATTACH)
		hInst = hInstance;
	return TRUE;
}