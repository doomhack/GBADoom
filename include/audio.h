/*******************************************************************
 *
 *	File:		Audio.h
 *
 *	Author:		Peter van Sebille (peter@yipton.demon.co.uk)
 *
 *	(c) Copyright 2001, Peter van Sebille
 *	All Rights Reserved
 *
 *******************************************************************/

#ifndef __AUDIO_H
#define __AUDIO_H

//We're just going to do a little fake SDL audio wrapper thing here.

typedef struct
{
  int freq;
  unsigned char channels;
  unsigned short samples;
  unsigned int size;
  unsigned int (*callback)(unsigned char *stream, const int len);
} AudioSpec;


int OpenAudio(AudioSpec *desired);

void StopAudio();

#endif			/* __AUDIO_H */
