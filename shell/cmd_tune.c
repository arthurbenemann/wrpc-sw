/*
 * This work is part of the White Rabbit project
 *
 * Released according to the GNU GPL, version 2 or any later version.
 */

 #include <stdlib.h>
 #include <string.h>
 #include <errno.h>
 #include <wrc.h>
 #include "softpll_ng.h"
 #include "shell.h"
 #include "spll_common.h"
 #include <TuneGuido.h>
 #include "softpll_ng.h"

struct  spll_main_state *s;

 static int cmd_tune(const char *args[])
 {
 	int GuidoTemp=0,A=0,B=0;
 //pp_printf("Tuning the world\n");
 if (!strcasecmp(args[0], "val")){
   if (!args[0])
    return -EINVAL;
    }else if (!strcasecmp(args[0], "kpki")){
    if (!args[2])
     return -EINVAL;
     A=(atoi(args[1]));
     Kphpsec = &A;
	 pp_printf("A:%i\t Aptr: %i\n",A,*Kphpsec);
     B=(atoi(args[2]));
	 Kihpsec = &B;
	 pp_printf("B:%i\t Bptr: %i\n",B,*Kihpsec);
     pp_printf("Entered Kp:%i\t Ki: %i\n",*Kphpsec,*Kihpsec);
	 spll_set_pi(*Kphpsec,*Kihpsec);
    }else if(!strcasecmp(args[0], "kp")){
	 if (!args[1])
     return -EINVAL;
	 A=(atoi(args[1]));
	 Kihpsec = &A;
	 //pp_printf("A:%i\t Aptr: %i\n",A,*Kphpsec);
	 spll_set_pi_solo_kp(A);
	}else if(!strcasecmp(args[0], "ki")){
	 if (!args[1])
     return -EINVAL;
	B=(atoi(args[1]));
	Kihpsec = &B;
    //pp_printf("A:%i\t Aptr: %i\n",B,*Kihpsec);
	spll_set_pi_solo_ki(B);
	}else if(!strcasecmp(args[0], "show")){
    if (!args[0])
     return -EINVAL;
     spll_show_kpki();
	}else if(!strcasecmp(args[0], "init")){
    if (!args[0])
     return -EINVAL;
     spll_init(3, 0, 0);
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
