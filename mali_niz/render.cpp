#include <Bela.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <sstream>
#include <libraries/Convolver/Convolver.h>
#include <libraries/AudioFile/AudioFile.h>


#define NUM_OF_SPEAKERS 12
#define W_LEN 1024

std::vector<float> data;
Convolver convolver;

const unsigned int gInputChannel = 0;


bool setup(BelaContext *context, void *userData)
{
	std::ifstream file("matrica_12zv_1024.csv");
    std::string line;
    std::vector<std::vector<float>> w;
    
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string value;
        std::vector<float> row;

        while (std::getline(iss, value, ',')) {
            row.push_back(std::stod(value));
        }
        w.push_back(row);
    }
    convolver.setup (w,  context->audioFrames);
    
     //rt_printf("%d\n",context->audioFrames);
    
	return true;
}

void render(BelaContext *context, void *userData)
{
	float inBuf[context->audioFrames];
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		float in = 0;
		in = audioRead(context, n, gInputChannel);
		inBuf[n] = in;
	}

	float outBuf[context->audioFrames];
	convolver.process(outBuf, inBuf, context->audioFrames);

	for(unsigned int n = 0; n < context->audioFrames; n++) {
		for(unsigned int channel = 0; channel < NUM_OF_SPEAKERS; ++channel)
		audioWrite(context, n, channel, 5*outBuf[n]);
		//	audioWrite(context, n, channel, 3*inBuf[n]);
	}
	
}

void cleanup(BelaContext *context, void *userData)
{

}