#include <Bela.h>
#include <libraries/AudioFile/AudioFile.h>

std::string gFilename = "MLS.wav";	
std::vector<float> gSampleBuffer;			
int gReadPointer = 0;
int channel=0;
int brojac=0;
float out;

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
	if (brojac>context->audioSampleRate*3){
    	brojac=0;
    	channel++;
    	gReadPointer=0;
    	rt_printf("channel '%d\n",channel);
    }
	
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		
		//rt_printf("channel %d\n",brojac);
		
		if (brojac<context->audioSampleRate*2){
			out = gSampleBuffer[gReadPointer];
			gReadPointer++;
		}
		else{
			out=0.0;
		}
		
		if (channel<12) {
        	audioWrite(context, n, channel, 0.1*out);
        }
        
    	else if (channel==12){
       		// na kraju svi zvucnici zajedno
    		for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
    			audioWrite(context, n, ch, 0.1*out);
    		}
    	}
    	else{
    		Bela_requestStop();
            return;
    	}
    	
    	brojac++;
    
    }
}

void cleanup(BelaContext *context, void *userData)
{

}