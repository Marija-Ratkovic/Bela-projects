#include <Bela.h>
#include<stdio.h>
#include <iostream>
#include <fstream>
#include<ctype.h>
#include<string.h>
#include<cmath>
#include <vector>
#include <sstream>
#include<libraries/AudioFile/AudioFile.h>
#include <libraries/Scope/Scope.h>
#include <libraries/Fft/Fft.h>
using namespace std;

int gInputChannel=12;
int FftLen=1024;
int gReadPointer=0;
 
vector<vector<float>> out(gInputChannel, vector<float>(FftLen, 0.0f));
vector<float> in (FftLen);
Fft IN(FftLen);
vector<vector<float>> OUT_Im(gInputChannel, vector<float>(FftLen/2+1, 0.0f));
vector<vector<float>> OUT_Re(gInputChannel, vector<float>(FftLen/2+1, 0.0f));
Fft** OUT = new Fft*[gInputChannel];

vector<vector<float>> W_Re;
vector<vector<float>> W_Im;

bool setup(BelaContext *context, void *userData)
{
	for (unsigned int i = 0; i < gInputChannel; i++) {
        OUT[i] = new Fft(FftLen);
    }
	
	// Popunjavanje nulama in baffera za prvo pozivanje render funkcije
	for(unsigned int n = 0; n < FftLen; ++n){
		in[n]=0;
	}
	
	// Ucitavanje realnog i imaginarnog dela frekvencijske maske filtara
    std::ifstream file("W_Re.csv");
    if (!file.is_open()) {
        rt_fprintf(stderr, "Failed to open CSV file\n");
        return false;
    }
    std::string line;
    while (std::getline(file, line)) {
        std::vector<float> row;
        std::stringstream ss(line);
        std::string value;
        while (std::getline(ss, value, ',')) {
            row.push_back(std::stof(value));
        }
        W_Re.push_back(row);
    }
    file.close();
    
    std::ifstream file1("W_Im.csv");
    if (!file1.is_open()) {
        rt_fprintf(stderr, "Failed to open CSV file\n");
        return false;
    }
    std::string line1;
    while (std::getline(file1, line1)) {
        std::vector<float> row;
        std::stringstream ss(line1);
        std::string value;
        while (std::getline(ss, value, ',')) {
            row.push_back(std::stof(value));
        }
        W_Im.push_back(row);
    }
    file.close();
 
   // rt_printf("CSV loaded with %zu rows\n", W_Re.size());  // -> 513
   // rt_printf("%d",OUT_Re[0].size()); // -> 513
   
	return true;
}

void render(BelaContext *context, void *userData)
{	
	// fft od odbiraka iz in baffera koji su procitani u prethodnoj render funkciji
	IN.fft(in);
	
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		// Citanje 1024 odbirka sa ulaza i smestanje u baffer in
		in[n] = audioRead(context, n, 0);
		
		// Odre]ivanje odbiraka spektralnih komponenata za svaki od zvucnika 
		if (n%2==0 || n==FftLen-1){
			//rt_printf("%d ",n);
			for(unsigned int k = 0; k < gInputChannel; k++){ 
				if (n==FftLen-1){
					OUT_Im[k][FftLen/2]=IN.fdi(FftLen/2)*W_Re[FftLen/2][k]+IN.fdr(FftLen/2)*W_Im[FftLen/2][k];
					OUT_Re[k][FftLen/2]=IN.fdr(FftLen/2)*W_Re[FftLen/2][k]-IN.fdi(FftLen/2)*W_Im[FftLen/2][k];
				}
				else {
					OUT_Im[k][n/2]=IN.fdi(n/2)*W_Re[n/2][k]+IN.fdr(n/2)*W_Im[n/2][k];
					OUT_Re[k][n/2]=IN.fdr(n/2)*W_Re[n/2][k]-IN.fdi(n/2)*W_Im[n/2][k];
				}
			}
		}
	}	
	
	// ifft od odredjenog realnog i imaginarnog dela za svaki od zvucnika 
	for (int m=0; m<gInputChannel; m++)
		OUT[m]->ifft(OUT_Re[m],OUT_Im[m]);
	
	// Slanje na izlaz odbiraka koji su izracunati kroz ifft svaki zvucnik ima svoje odbirke
	for(unsigned int n = 0; n < context->audioFrames; n++) {
		for(unsigned int channel = 0; channel < gInputChannel; ++channel){
			out[channel][n]=OUT[channel]->td(n);
			audioWrite(context, n, channel, out[channel][n]);
		}
	}
	
}

void cleanup(BelaContext *context, void *userData)
{
	for (unsigned int i = 0; i < gInputChannel; ++i) {
        delete OUT[i];  // Delete each Fft object
    }
    delete[] OUT;  // Delete the array of pointers
}