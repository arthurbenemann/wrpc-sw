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

struct  spll_main_state *s;

 static int cmd_tune(const char *args[])
 {
 	int cur, tgt,GuidoTemp;
 pp_printf("Tuning the world\n");
 pp_printf("Val of %d\n",*Kphpsec);
 if (!strcasecmp(args[0], "hello hal")){
   if (!args[1])
    return -EINVAL;
    pp_printf("Hello dave\n");
  }else if (!strcasecmp(args[0], "KpKi")){
    if (!args[2])
     return -EINVAL;
     GuidoTemp=(atoi(args[1]));
     *Kphpsec = GuidoTemp;
     GuidoTemp=(atoi(args[2]));
     *Kihpsec = GuidoTemp;
     pp_printf("Kp %d\n",*Kphpsec);
     pp_printf("Ki %d\n",*Kihpsec);
     //pp_printf("test GuidoTemp %d\n",GuidoTemp);
  }


    //pp_printf("%d\n", spll_check_lock(atoi(args[1])));
 else
   return -EINVAL;

 	return 0;
 }

 DEFINE_WRC_COMMAND(tune) = {
 	.name = "tune",
 	.exec = cmd_tune,
 };
