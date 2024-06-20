##
##This work is part of the White Rabbit project
##
##Copyright (C) 2024 CERN (www.cern.ch)
##Author: Mattia Rizzi <mattia.rizzi@ing.unibs.it>
##Author: Harvey Leicester <harvey.leicester@cern.ch>
##
##Released according to the GNU GPL, version 2 or any later version.

clear all; 
close all;

##parameters
Ki = 2;           #integrator gain: helper = 2, main = 30
Kp = 150;         #proportional gain: helper = 150, main = 1100
ppm_range = 100;  #ppm pull range of oscillator: helper = 100ppm, main = 10ppm
frac_bits = 12;   #PI_FRACBITS
HPLL_N = 14;      #tag interval = 2^HPLL_N
f_ref = 62.5e6;   #reference frequency 
delta = 1;        #dmtd delta 
dac_bits = 16;    #controller dac bits
dac_v_range = 3.3;  #dac full scale range (V)

##do not modify
fs = f_ref*(delta/(delta+(2^HPLL_N))) #offset frequency = sample rate, i.e result of dmtd mixing
Ts = 1/(fs);    #fs period
Kpfd = (2^HPLL_N)/(2*pi);   #phase detector gain
vco_pullrange = (f_ref * ppm_range) / 10^6;    #vco pull range (Hz)
dac_v_res = dac_v_range/2^dac_bits;   #dac V resolution 
Kvco = vco_pullrange * dac_v_res;     #vco gain
PI = tf([(Ki+Kp) -Kp], [1 -1], Ts)    #pi tf
VCO = tf([Ts*Kvco],[1 -1], Ts)        #vco tf

L = Kpfd*PI*(1/2^frac_bits)*VCO*2*pi  
L = minreal(L);
Sys = feedback(L,1);  #close the loop

##bode plot
[m, ph, w] = bode(Sys);
f = w/(2*pi());
m_db = 20*log10(m);

figure(1)
subplot(2,1,1);                    
semilogx(f, m_db);             
zoom on;
grid on;
title('Bode');
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');
subplot(2,1,2);                   
semilogx(f, ph);              
grid on;
zoom on;
xlabel('Frequency (Hz)');
ylabel('Phase (deg)');

##step response
[y,t]=step(Sys);

figure 
plot(t, y)
title("Step")
xlabel("Time (s)")
ylabel("Magnitude") 
grid on

##ntf
Sys_rejection = feedback(1,L);
[m, ph, w] = bode(Sys_rejection);
f = w/(2*pi());
m_db = 20*log10(m);
figure
semilogx(f, m_db);
grid on 
zoom on 
title("NTF")
xlabel('Frequency (Hz)');
ylabel('Magnitude (dB)');
