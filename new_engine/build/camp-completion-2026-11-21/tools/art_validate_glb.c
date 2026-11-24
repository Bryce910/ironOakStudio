#define CGLTF_IMPLEMENTATION
#include "cgltf.h"
#include <stdio.h>
int main(int argc,char **argv){unsigned pass=0;for(int i=1;i<argc;i++){cgltf_options o={0};cgltf_data*d=0;cgltf_result r=cgltf_parse_file(&o,argv[i],&d);if(r==cgltf_result_success)r=cgltf_load_buffers(&o,d,argv[i]);if(r==cgltf_result_success)r=cgltf_validate(d);if(r!=cgltf_result_success){fprintf(stderr,"INVALID %s (%d)\n",argv[i],r);cgltf_free(d);return 1;}pass++;cgltf_free(d);}printf("Validated %u complete GLBs and embedded buffers\n",pass);return 0;}
