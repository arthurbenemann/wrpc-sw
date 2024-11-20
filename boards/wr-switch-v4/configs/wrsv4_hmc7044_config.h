{
162,
{
  {0x0054, 0x01}, //sdata open drain
  {0x0046, 0x00}, //gpi1
  {0x0047, 0x00}, //gpi2
  {0x0048, 0x00}, //gpi3 (reset value = 0x09 = sleep mode!)
  {0x0049, 0x00}, //gpi4
  {0x0050, 0x00}, //gpo1
  {0x0051, 0x00}, //gpo2
  {0x0052, 0x00}, //gpo3
  {0x0053, 0x00}, //gpo4
  {0x00C8, 0x00},
  {0x00D2, 0x00},
  {0x00DC, 0x00},
  {0x00E6, 0x00},
  {0x00F0, 0x00},
  {0x00FA, 0x00},
  {0x0104, 0x00},
  {0x010E, 0x00},
  {0x0118, 0x00},
  {0x0122, 0x00},
  {0x012C, 0x00},
  {0x0136, 0x00},
  {0x0140, 0x00},
  {0x014A, 0x00},
  {0x009F, 0x4D},  
  {0x00A0, 0xDF},
  {0x00A5, 0x06},
  {0x00A8, 0x06},
  {0x00B0, 0x04},
  {0x0005, 0x00}, //disable sync pin and pll1 ref path
  {0x0003, 0x36}, //disable pll1
  {0x005B, 0x06}, //defaults
  {0x0033, 0x01}, //Set R2 = 1 (LSB)
  {0x0034, 0x00}, //Set R2 = 1 (MSB)
  {0x0035, 0x7D}, //Set N2 = 125 (LSB)
  {0x0036, 0x00}, //Set R2 = 125 (MSB)
  {0x0032, 0x00}, //Enable freq doubler before R2
  {0x001A, 0x00}, //charge pump current (0 as is disabled)
  {0x0020, 0x01}, //OSCIN input prescaler = 1  
  {0x000A, 0x00}, //Disable CLKIN0
  {0x000B, 0x00}, //Configure CLKIN1 (ext 10 MHz ref disable for now)
  {0x000C, 0x00}, //Disable CLKIN2
  {0x000D, 0x00}, //Disable CLKIN3
  {0x000E, 0x07}, //Enable OSCIN (int 10 MHz ref): enable, 100 Ohm + ac coupling
  {0x005C, 0xD0}, //SYSREF timer LSB (Fvco = 2.5 GHz * 2, SYSREF = 2000 --> Fsysref = 5e9/2000 = 2.5 MHz < 4 MHz)
  {0x005D, 0x07}, //SYSREF timer MSB
  //channel 0, WR_CLK, 62.5MHz
  {0x00C8, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x00C9, 0x28}, //Divider(/40) LSB 
  {0x00CA, 0x00}, //Divider (/40) MSB 
  {0x00CB, 0x00}, //Fine analog delay
  {0x00CC, 0x00}, //Coarse digital delay
  {0x00CD, 0x00}, //12bit multislip digital delay (LSB)
  {0x00CE, 0x00}, //12bit multislip digital delay (MSB)
  {0x00CF, 0x00}, //Output mux: channel divider
  {0x00D0, 0x10}, //LVDS   
  //channel 1 REF_CLK, 62.5MHz
  {0x00D2, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x00D3, 0x28}, //Divider(/40) LSB 
  {0x00D4, 0x00}, //Divider (/40) MSB 
  {0x00D5, 0x00}, //Fine analog delay
  {0x00D6, 0x00}, //Coarse digital delay
  {0x00D7, 0x00}, //12bit multislip digital delay (LSB)
  {0x00D8, 0x00}, //12bit multislip digital delay (MSB)
  {0x00D9, 0x00}, //Output mux: channel divider
  {0x00DA, 0x10}, //LVDS   
  //channel 13 SYNC_CLK1, 62.5MHz
  {0x014A, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x014B, 0x28}, //Divider(/40) LSB 
  {0x014C, 0x00}, //Divider (/40) MSB 
  {0x014D, 0x00}, //Fine analog delay
  {0x014E, 0x00}, //Coarse digital delay
  {0x014F, 0x00}, //12bit multislip digital delay (LSB)
  {0x0150, 0x00}, //12bit multislip digital delay (MSB)
  {0x0151, 0x00}, //Output mux: channel divider
  {0x0152, 0x08}, //LVPECL      
  //channel 11 SYNC_CLK2, 250MHz
  {0x0136, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x0137, 0x0A}, //Divider(/40) LSB 
  {0x0138, 0x00}, //Divider (/40) MSB 
  {0x0139, 0x00}, //Fine analog delay
  {0x013A, 0x00}, //Coarse digital delay
  {0x013B, 0x00}, //12bit multislip digital delay (LSB)
  {0x013C, 0x00}, //12bit multislip digital delay (MSB)
  {0x013D, 0x00}, //Output mux: channel divider
  {0x013E, 0x08}, //LVPECL    
  //channel 7 EXT_C2M, 62.5MHz
  {0x010E, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x010F, 0x28}, //Divider(/40) LSB 
  {0x0110, 0x00}, //Divider (/40) MSB 
  {0x0111, 0x00}, //Fine analog delay
  {0x0112, 0x00}, //Coarse digital delay
  {0x0113, 0x00}, //12bit multislip digital delay (LSB)
  {0x0114, 0x00}, //12bit multislip digital delay (MSB)
  {0x0115, 0x00}, //Output mux: channel divider
  {0x0116, 0x10}, //LVDS   
  //channel 8 62M5_OUT, 62.5MHz
  {0x0118, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x0119, 0x28}, //Divider(/40) LSB 
  {0x011A, 0x00}, //Divider (/40) MSB 
  {0x011B, 0x00}, //Fine analog delay
  {0x011C, 0x00}, //Coarse digital delay
  {0x011D, 0x00}, //12bit multislip digital delay (LSB)
  {0x011E, 0x00}, //12bit multislip digital delay (MSB)
  {0x011F, 0x00}, //Output mux: channel divider
  {0x0120, 0x08}, //LVPECL        
  //channel 10 PLL_OUT, 10MHz
  {0x012C, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x012D, 0xFA}, //Divider(/40) LSB 
  {0x012E, 0x00}, //Divider (/40) MSB 
  {0x012F, 0x00}, //Fine analog delay
  {0x0130, 0x00}, //Coarse digital delay
  {0x0131, 0x00}, //12bit multislip digital delay (LSB)
  {0x0132, 0x00}, //12bit multislip digital delay (MSB)
  {0x0133, 0x00}, //Output mux: channel divider
  {0x0134, 0x08}, //LVPECL         
  //channel 6: GTH_CLK1, 125 MHz
  {0x0104, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x0105, 0x14}, //Divider(/40) LSB 
  {0x0106, 0x00}, //Divider (/40) MSB 
  {0x0107, 0x00}, //Fine analog delay
  {0x0108, 0x00}, //Coarse digital delay
  {0x0109, 0x00}, //12bit multislip digital delay (LSB)
  {0x010A, 0x00}, //12bit multislip digital delay (MSB)
  {0x010B, 0x00}, //Output mux: channel divider
  {0x010C, 0x10}, //LVDS  
  //channel 12: GTH_CLK0, 125 MHz
  {0x0140, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x0141, 0x14}, //Divider(/40) LSB 
  {0x0142, 0x00}, //Divider (/40) MSB 
  {0x0143, 0x00}, //Fine analog delay
  {0x0144, 0x00}, //Coarse digital delay
  {0x0145, 0x00}, //12bit multislip digital delay (LSB)
  {0x0146, 0x00}, //12bit multislip digital delay (MSB)
  {0x0147, 0x00}, //Output mux: channel divider
  {0x0148, 0x10}, //LVDS     
  //channel 4: GTY_128_CLK0, 125 MHz
  {0x00F0, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x00F1, 0x14}, //Divider(/40) LSB 
  {0x00F2, 0x00}, //Divider (/40) MSB 
  {0x00F3, 0x00}, //Fine analog delay
  {0x00F4, 0x00}, //Coarse digital delay
  {0x00F5, 0x00}, //12bit multislip digital delay (LSB)
  {0x00F6, 0x00}, //12bit multislip digital delay (MSB)
  {0x00F7, 0x00}, //Output mux: channel divider
  {0x00F8, 0x10}, //LVDS     
  //channel 2: GTY_129_CLK0, 125 MHz
  {0x00DC, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x00DD, 0x14}, //Divider(/40) LSB 
  {0x00DE, 0x00}, //Divider (/40) MSB 
  {0x00DF, 0x00}, //Fine analog delay
  {0x00E0, 0x00}, //Coarse digital delay
  {0x00E1, 0x00}, //12bit multislip digital delay (LSB)
  {0x00E2, 0x00}, //12bit multislip digital delay (MSB)
  {0x00E3, 0x00}, //Output mux: channel divider
  {0x00E4, 0x10}, //LVDS   
  //channel 5: GTY_130_CLK0, 125 MHz
  {0x00FA, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x00FB, 0x14}, //Divider(/40) LSB 
  {0x00FC, 0x00}, //Divider (/40) MSB 
  {0x00FD, 0x00}, //Fine analog delay
  {0x00FE, 0x00}, //Coarse digital delay
  {0x00FF, 0x00}, //12bit multislip digital delay (LSB)
  {0x0100, 0x00}, //12bit multislip digital delay (MSB)
  {0x0101, 0x00}, //Output mux: channel divider
  {0x0102, 0x10}, //LVDS         
  //channel 3: GTY_131_CLK0, 125 MHz
  {0x00E6, 0xC1}, //High perf, SYNC, normal startup, Ch enable
  {0x00E7, 0x14}, //Divider(/40) LSB 
  {0x00E8, 0x00}, //Divider (/40) MSB 
  {0x00E9, 0x00}, //Fine analog delay
  {0x00EA, 0x00}, //Coarse digital delay
  {0x00EB, 0x00}, //12bit multislip digital delay (LSB)
  {0x00EC, 0x00}, //12bit multislip digital delay (MSB)
  {0x00ED, 0x00}, //Output mux: channel divider
  {0x00EE, 0x10}, //LVDS  
}
};
