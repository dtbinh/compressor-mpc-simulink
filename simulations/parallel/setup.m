% Setup for parallel compressor simulation
clear all
global P_D 

Td = 0.0; % torque input
u_rec = 0; % recycle opening
u_d = 0.9; % discharge valve opening

P_D = 1.07; % initialize tank pressure

Ts = 0.05;

% linearization point
x_init_lin = [0.898
      1.126
      0.15
      439.5
      0];
  
x_init_lin = [0.912; 1.17; 0.14; 465; 0];
