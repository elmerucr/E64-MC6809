#include <cstdio>

#include "host.hpp"
#include "common.hpp"

E64::host_t::host_t()
{
	printf("[Host] E64 version %i.%i.%i (C)%i elmerucr\n",
	       E64_MAJOR_VERSION,
	       E64_MINOR_VERSION,
	       E64_BUILD, E64_YEAR);
	
	settings = new settings_t();
	video = new video_t();
}

E64::host_t::~host_t()
{
	printf("[Host] closing E64\n");
	delete video;
	delete settings;
}
