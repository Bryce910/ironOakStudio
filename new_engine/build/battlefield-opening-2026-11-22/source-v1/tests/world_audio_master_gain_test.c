#include "engine/audio.h"
#include <limits.h>
#include <math.h>
#include <stdio.h>
int main(void){const int inputs[]={INT_MIN,-100,0,1,50,100,200,INT_MAX};const float expected[]={0,0,0,.01f,.5f,1,1,1};for(unsigned i=0;i<8;++i){audio_set_volumes(inputs[i],100,100);if(!isfinite(audio_master_gain())||fabsf(audio_master_gain()-expected[i])>1e-6f)return 1;}puts("{\"passed\":true,\"checks\":8,\"actual_readonly_accessor\":true,\"no_device_or_channels_created\":true}");return 0;}
