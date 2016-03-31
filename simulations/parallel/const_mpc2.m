function [Ts, n_delay,dsize,ucontrolsize,p,m,UWT,YWT] = const_mpc2()
%#eml

Ts = 0.05;

n_delay = [0, 40];

dsize = 2;

ucontrolsize = 2;

p = 100;
m = 2;

UW = [1e3 1e5];
YW = [1 1 0.1 0.1];

UWT = kron(eye(m),diag([UW,UW]'));
YWT = kron(eye(p),diag(YW'));

end