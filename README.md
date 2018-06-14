# spectrum analyzer

The goal of this project was to verify that a self made audio signal generator was actually able to produce a 24kHz signal. Of course other frequencies can be analyzed as well, with the upper limit of what is possible depending on the supported sampling rates of your sound card.

![Alt text](screenshot.png?raw=true "Me whistling into the microphone")

### How it works

* An audio device is opened via the ALSA library and configured to a certain sampling frequency.
* The FFT (using the fftw library) is run on a number of samples from the audio device.
* Every result of the FFT is plotted in the UI via the qcustomplot library.

### How to build (on Ubuntu 16.04)

```
sudo apt-get install cmake libasound2-dev fftw-dev libqcustomplot-dev
mkdir <build-folder>
cd <build-folder>
cmake <source-folder> -DCMAKE_BUILD_TYPE=RELEASE
make
```

### How to use

* Next to the spectrum_analyzer, the build will produce another binary called list_pcm_devices. It will print a list of all audio devices found in the system. The name of one of these devices can be passed to the spectrum_analyzer as the device parameter. You will most likely want to use the default audio device (which is some kind of synthetic device from the PulseAudio layer), at least that's what I used the whole time. Other devices in that list (e.g. the real hardware devices) might only support a limited number of sampling frequencies and buffer sizes.
* Run: ```./spectrum_analyzer default 44000 1000```. The FFT will be run on data sampled at 44kHz with a sampling duration of 1 second (which corresponds to 16000 samples as input to the FFT). Actually, the length won't be 1 second, but a value somewhere near 1 second (1024ms in our example) in order to have the fft run on a sample size that is a power of 2 (1024ms at 44kHz makes 16384=2^14 samples). The x-Axis of the graph will plot up-to the nyquist-frequency of 22kHz, the y-Axis will show the amplitude (without unit, just an unnormalized power spectrum).

### Architecture

We have three components (alsa, fft, ui) and two queues connecting them. Every component has its own thread, push'ing/pop'ing the queues. There is also a watchdog for queue monitoring, making sure that all threads are working fast enough (I was concerned with the UI not being able to keep up or the FFT taking too long at 192kHz).
