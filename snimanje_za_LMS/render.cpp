#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>

std::string gFilename = "MLS.wav";	
std::vector<float> gSampleBuffer;			
int gReadPointer = 0;						

bool setup(BelaContext *context, void *userData)
{
	gSampleBuffer = AudioFileUtilities::loadMono(gFilename);
	
	if(gSampleBuffer.size() == 0) {
    	rt_printf("Error loading audio file '%s'\n", gFilename.c_str());
    	return false;
	}

    rt_printf("Loaded the audio file '%s' with %d frames (%.1f seconds)\n", 
    			gFilename.c_str(), gSampleBuffer.size(),
    			gSampleBuffer.size() / context->audioSampleRate);

	return true;
}

void render(BelaContext *context, void *userData)
{
    for(unsigned int n = 0; n < context->audioFrames; n++) {
		
        float out = gSampleBuffer[gReadPointer];
        if (gReadPointer==(gSampleBuffer.size()-1)){
        	gReadPointer=0;
        }
        else{
        	gReadPointer++;
        }

    //	for(unsigned int channel = 0; channel < context->audioOutChannels; channel++) {
    		audioWrite(context, n, 0, 0.1*out);
    //	}
    }
}

void cleanup(BelaContext *context, void *userData)
{

}
