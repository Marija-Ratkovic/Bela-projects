#include <Bela.h>
#include <libraries/Fft/Fft.h>
#include <libraries/Scope/Scope.h>
#include <libraries/Gui/Gui.h>
#include <libraries/GuiController/GuiController.h>
#include <cmath>
#include <cstring>
#include <vector>
#include <sstream>
#include<ctype.h>
#include<stdio.h>
#include <iostream>
#include <fstream>
#include <algorithm>
using namespace std;

Scope gScope;

int gOutputChannel=12;

// FFT-related variables
Fft gFft;					// FFT processing object
vector <Fft> gFft_out (gOutputChannel);
const int gFftSize = 1024;	// FFT window size in samples
int gHopSize = 256;			// How often we calculate a window

// Circular buffer and pointer for assembling a window of samples
const int gBufferSize = 16384;
std::vector<float> gInputBuffer;
int gInputBufferPointer = 0;
int gHopCounter = 0;

// Circular buffer for collecting the output of the overlap-add process
std::vector<std::vector<float>> gOutputBuffer;
int gOutputBufferWritePointer = gFftSize + gHopSize;
int gOutputBufferReadPointer = 0;

// Buffer to hold the windows for FFT analysis and synthesis
std::vector<float> gAnalysisWindowBuffer;
std::vector<float> gSynthesisWindowBuffer;

// Filter coefficients
vector<vector<float>> W_Re;
vector<vector<float>> W_Im;

// Thread for FFT processing
AuxiliaryTask gFftTask;
int gCachedInputBufferPointer = 0;

void process_fft_background(void *);

std::vector<float> H_Im;
std::vector<float> H_Re;


std::vector<std::vector<float>> readCSVMatrix(const std::string& filename, int expectedRows = 12, int expectedCols = 1024) {
    std::vector<std::vector<float>> matrix;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error opening file: " << filename << "\n";
        return matrix;
    }

    std::string line;
    int rowCount = 0;

    while (std::getline(file, line)) {
        std::vector<float> row;
        std::stringstream ss(line);
        std::string cell;
        while (std::getline(ss, cell, ',')) {
            try {
                row.push_back(std::stof(cell));
            } catch (const std::invalid_argument& e) {
                std::cerr << "Invalid number in row " << rowCount << ": " << cell << "\n";
            }
        }

        if (row.size() != expectedCols) {
            std::cerr << "Warning: Row " << rowCount << " has " << row.size() << " columns (expected " << expectedCols << ")\n";
        }

        matrix.push_back(row);
        rowCount++;
    }

    file.close();

    if (matrix.size() != expectedRows) {
        std::cerr << "Warning: Matrix has " << matrix.size() << " rows (expected " << expectedRows << ")\n";
    }

    return matrix;
}



bool setup(BelaContext *context, void *userData)
{
	gFft.setup(gFftSize);
	
	gInputBuffer.resize(gBufferSize);
	gOutputBuffer.resize(gOutputChannel);
	for (int k = 0; k < gOutputChannel; k++){
		gOutputBuffer[k].resize(gBufferSize);
		gFft_out[k].setup(gFftSize);
	}
	
	// Calculate the window
	gAnalysisWindowBuffer.resize(gFftSize);
	gSynthesisWindowBuffer.resize(gFftSize);
	for(int n = 0; n < gFftSize; n++) {
		// Hann window
		gAnalysisWindowBuffer[n] = 0.5f * (1.0f - cosf(2.0 * M_PI * n / (float)(gFftSize - 1)));
		gSynthesisWindowBuffer[n]=gAnalysisWindowBuffer[n];
	}
	
	// Set up the thread for the FFT
	gFftTask = Bela_createAuxiliaryTask(process_fft_background, 50, "bela-process-fft");


	W_Re= readCSVMatrix("W_real.csv");
	W_Im= readCSVMatrix("W_imag.csv");

	if (W_Re.size() != gOutputChannel || W_Re[1].size() != gFftSize || W_Im.size() != gOutputChannel || W_Im[1].size() != gFftSize) {
    	std::cerr << "Warning: expected there is not enaif samples of filter\n";
	}

	// Initialise the scope
	gScope.setup(2, context->audioSampleRate);

	return true;
}


void process_fft(std::vector<float> const& inBuffer, unsigned int inPointer, std::vector<vector<float>>& outBuffer, unsigned int outPointer)
{
	static std::vector<float> unwrappedBuffer(gFftSize);	// Container to hold the unwrapped values
	
	// Copy buffer into FFT input
	for(int n = 0; n < gFftSize; n++) {
		// Use modulo arithmetic to calculate the circular buffer index
		int circularBufferIndex = (inPointer + n - gFftSize + gBufferSize) % gBufferSize;
		unwrappedBuffer[n] = inBuffer[circularBufferIndex] * gAnalysisWindowBuffer[n];
	}
	
	// Process the FFT based on the time domain input
	gFft.fft(unwrappedBuffer);
	
	for (int k = 0; k < gOutputChannel; k++){	
		// Filtering in frequency domain
		for(int n = 0; n < gFftSize; n++) {
			gFft_out[k].fdr(n) = gFft.fdr(n)*W_Re[k][n]-gFft.fdi(n)*W_Im[k][n];
			gFft_out[k].fdi(n) = gFft.fdr(n)*W_Im[k][n]+gFft.fdi(n)*W_Re[k][n];
		}
		
		// Run the inverse FFT
		gFft_out[k].ifft();
		
		// Add timeDomainOut into the output buffer
		for(int n = 0; n < gFftSize; n++) {
			int circularBufferIndex = (outPointer + n - gFftSize + gBufferSize) % gBufferSize;
			outBuffer[k][circularBufferIndex] += gFft_out[k].td(n)*gSynthesisWindowBuffer[n];
		}
	}
}

// This function runs in an auxiliary task on Bela, calling process_fft
void process_fft_background(void *)
{
	process_fft(gInputBuffer, gCachedInputBufferPointer, gOutputBuffer, gOutputBufferWritePointer);

	// Update the output buffer write pointer to start at the next hop
	gOutputBufferWritePointer = (gOutputBufferWritePointer + gHopSize) % gBufferSize;
}


void render(BelaContext *context, void *userData)
{
	for(unsigned int n = 0; n < context->audioFrames; n++){
		
		float in = audioRead(context, n, 0);
		
		gInputBuffer[gInputBufferPointer++] = in;
		if(gInputBufferPointer >= gBufferSize) {
			gInputBufferPointer = 0;
		}
		
		// Get the output sample from the output buffer.
		//Then clear the output sample in the buffer so it is ready for the next overlap-add
		// Scale the output down by the overlap factor (e.g. how many windows overlap per sample?)
		vector <float> out (gOutputChannel);
		for (int k = 0; k < gOutputChannel; k++){
			out[k]=gOutputBuffer[k][gOutputBufferReadPointer];
			gOutputBuffer[k][gOutputBufferReadPointer]=0;
			out[k] *= (float)gHopSize / (float)gFftSize;
		}
		
		// Increment the read pointer in the output cicular buffer
		gOutputBufferReadPointer++;
		if(gOutputBufferReadPointer >= gBufferSize)
		gOutputBufferReadPointer = 0;
		
		// Increment the hop counter and start a new FFT if we've reached the hop size
		if(++gHopCounter >= gHopSize) {
			gHopCounter = 0;
			
			gCachedInputBufferPointer = gInputBufferPointer;
			Bela_scheduleAuxiliaryTask(gFftTask);
		}
		
		for(unsigned int channel = 0; channel < gOutputChannel; channel++) {
			audioWrite(context, n, channel, 2*out[channel]);
		//audioWrite(context, n, channel, in);
		}
		
		// Log to the scope
		gScope.log(in, out[2]);
	}

}

void cleanup(BelaContext *context, void *userData)
{

}