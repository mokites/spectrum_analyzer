
#include "logger.h"
#include "alsa.h"
#include <cstdlib>
#include <stdexcept>

int main(int argc, char** argv) {
	if (argc < 3) {
		return -1;
	}

	ockl::Logger logger;
	ockl::Alsa alsa(argv[1], ::atoi(argv[2]), logger);

	try {
		alsa.init();
		alsa.start();
		alsa.stop();
	} catch (const std::runtime_error& ex) {
		logger.error(ex.what());
	}

	logger.shutdown();

	return 0;
}
