#include <Bela.h>
#include <cmath>
#include <libraries/AudioFile/AudioFile.h>

std::string gFilename = "MLS.wav";	
std::vector<float> gSampleBuffer;			
int gReadPointer = 0;
int channel=-1;
int brojac=10*48000;
float out;
int ind=0;

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
		if (brojac>context->audioSampleRate*11){
    		brojac=0;
    		channel++;
    		gReadPointer=0;
    		rt_printf("channel %d\n",channel);
		}
	
		for(unsigned int n = 0; n < context->audioFrames; n++) {
			
			if (brojac<context->audioSampleRate*10 && channel>-1){
				out = gSampleBuffer[gReadPointer];
				gReadPointer++;
			}
			else if(channel>-1){
				out=0.0;
			}
			else {
				out = 12*sin(2.0 * M_PI * gReadPointer * 1000.0 / context->audioSampleRate) ;
				gReadPointer++;
			}
		
			if (channel<12 && channel>-1){
        		audioWrite(context, n, channel, 6*out);
        	}
        
    		else if (channel==12){
       			for(unsigned int ch = 0; ch < context->audioOutChannels; ch++) {
    			audioWrite(context, n, ch, 4*out);
    			}
    		}
    		else if (channel==-1){
    			audioWrite(context, n, 0, out);
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