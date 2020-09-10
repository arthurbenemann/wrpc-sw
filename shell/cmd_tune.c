/*
 * This work is part of the White Rabbit project
 *
 * (c) 10-09-2020
 * Author: Guido Visser <guidov@nikhef.nl>
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

 #include <stdlib.h>
 #include <string.h>
 #include <errno.h>
 #include <wrc.h>
 #include "softpll_ng.h"
 #include "shell.h"



//struct  spll_main_state *s;

 static int cmd_tune(const char *args[])
 {
 	
 if (!strcasecmp(args[0], "val")){
   if (!args[0])
    return -EINVAL;
    }else if (!strcasecmp(args[0], "kpki")){
    if (!args[2])
     return -EINVAL;
     spll_set_pi_kpki(atoi(args[1]),atoi(args[2]));
    }else if(!strcasecmp(args[0], "kp")){
	 if (!args[1])
     return -EINVAL;
	 spll_set_pi_solo_kp((atoi(args[1])));
	}else if(!strcasecmp(args[0], "ki")){
	 if (!args[1])
     return -EINVAL;
	spll_set_pi_solo_ki((atoi(args[1])));
	}else if(!strcasecmp(args[0], "show")){
    if (!args[0])
     return -EINVAL;
     spll_show_kpki();
	}else if(!strcasecmp(args[0], "gasopdielolly")){
    if (!args[0])
     return -EINVAL;
     spll_set_pi_kpki(-3000,-5);
	}else if(!strcasecmp(args[0], "picontrol")){
	if (!args[1])
     return -EINVAL;
     spll_set_vtune_off(atoi(args[1])); 
}
    
else
   return -EINVAL;

 	return 0;
 }

 DEFINE_WRC_COMMAND(tune) = {
 	.name = "tune",
 	.exec = cmd_tune,
 };
