#ifndef TR2_SNOWMOBILE_H
#define TR2_SNOWMOBILE_H
#include "tr2_runtime.h"
typedef struct {int armed,turn,engine_pitch;} Tr2SnowDrive;
typedef struct {Tr2SnowDrive drive;int left_fall,right_fall;int16_t momentum,rotation;} Tr2Snowmobile;
int tr2_snowmobile_input(Tr2Actor *sled,int floor,uint32_t input,Tr2SnowDrive *drive,
                         uint32_t *random_state,int *pitch,int *look);
#endif
