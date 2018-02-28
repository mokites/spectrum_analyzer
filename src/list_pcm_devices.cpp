
#include <iostream>

#include <alsa/asoundlib.h>

/**
 * blatantly copied from stackoverflow
 */
int main()
{
	char** hints;
	int result = snd_device_name_hint(-1, "pcm", (void***)&hints);
	if (result < 0) {
		std::cerr << "failed to get device name hints: " << snd_strerror(result);
		return -1;
	}

	char** n = hints;
	while (*n != nullptr) {
	    char *name = snd_device_name_get_hint(*n, "NAME");
	    if (name != nullptr && strcmp("null", name) != 0) {
	        std::cout << name << std::endl;
	        free(name);
	    }
	    n++;
	}

	snd_device_name_free_hint((void**)hints);
	return 0;
}
