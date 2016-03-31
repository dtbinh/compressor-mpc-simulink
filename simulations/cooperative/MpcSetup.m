% Tuning parameters for MPC controller

clear all

load sys.mat
As = A;
Bs = B;
Cs = C;
clear A B C

addpath('../call_qpoases_m')
addpath('../call_qpoases_m/qpoases3/interfaces/matlab/')
addpath('../common')

[Ts, xsize_comp, xsize, ~, ysize, uoff1, uoff2, ud] = const_sim();
[n_delay,dsize,ucontrolsize,p,m,UWT,YWT] = const_mpc();

xtotalsize = xsize + 2*sum(n_delay) + 2*dsize;


%% Initial state
% global P_D

P_D = 1.08;

% Use increased P2 to get stable system for observer design
% x_init_lin = [0.899; 1.126; 0.15; 440; 0];
x_init_lin = [0.899; 1.2; 0.15; 440; 0];

xinit = [x_init_lin; x_init_lin; P_D];

u_out = uoff1(3);
u_d = ud;

u_init = zeros(2*ucontrolsize,1);

[A,B,C] = get_qp_matrices(xinit, u_init);

% Go back to original p2 value
x_init_lin = [0.899; 1.126; 0.15; 440; 0];
xinit = [x_init_lin; x_init_lin; P_D];


% A(1:5,1:5) = As;
% A(6:10,6:10) = As;
% A(12,1:5) = Bs(:,2);
% A(32,6:10) = Bs(:,2);
% C(1:2,1:5) = Cs;
% C(3:4,6:10) = Cs;

D = zeros(ysize);

%% Design observer
% expectation of output disturbance
Qn = eye(xsize);
Rn = eye(2*dsize);

% state disturbance to state matrix
G = [zeros(xtotalsize-2*dsize,xsize);
    zeros(dsize,1), eye(dsize)*10, zeros(dsize, xsize_comp-dsize-1), zeros(dsize,xsize-xsize_comp);
    zeros(dsize,xsize_comp), zeros(dsize,1), eye(dsize)*10, zeros(dsize, xsize-xsize_comp-dsize-1)];

Hkalman = [zeros(dsize, xsize_comp-dsize), eye(dsize), zeros(dsize,xsize-xsize_comp);
    zeros(dsize,xsize_comp), zeros(dsize,xsize_comp-dsize), eye(dsize), zeros(dsize, xsize-2*xsize_comp)];

sys_kalman = ss(A, [B G], C, [D Hkalman], Ts);

[KEST, L, P, M, Z] = kalman(sys_kalman, Qn, Rn);

%% Define upper/lower bounds
lb = [-0.3; 0];
ub = [0.3; 1];
% lb = [-0.1; 0];
% ub = [0.1; 1];
% lb = [0;0];
% ub = [0;0];


lb_rate = [0.1; 0.1];
ub_rate = [0.1; 1];

LB = repmat(lb,2*m,1);
UB = repmat(ub,2*m,1);
LBrate = repmat(lb_rate,2*m,1);
UBrate = repmat(ub_rate,2*m,1);

usize = 2*ucontrolsize;
Ain = full(spdiags(ones(usize*m,1)*[1,-1],[-usize,0],usize*m,usize*m));

Ain = [Ain; -Ain];
b = [LBrate; UBrate];

%% Initialize system state
xinit = [xinit; zeros(xtotalsize-xsize,1)];
upast = u_init;
deltax = zeros(xtotalsize,1);


disp('MPC Problem Formulated');

generate_file_linearized;

disp('Embedded files generated.');
