% ����SIT_Model��ʼֵ
S=[1.0];
I=[1.27*10^(-6)];
R=[0.0];
t=200;
m=1/2;%��ֵ
n=1/3;%��ֵ
% m=1/3;%��ֵ
% n=1/2;%��ֵ

% ����R��S��Iֵ
for i=2:t
    R(i) = R(i-1)+calculatedrdt(m,I(i-1));
    S(i) = S(i-1)-calculatedsdt(n,S(i-1),I(i-1));
    I(i) = I(i-1)+calculatedsdt(n,S(i-1),I(i-1))-calculatedrdt(m,I(i-1));
end

% ����R��S��I�仯����
plot(R,'r');
hold on;
plot(I,'g');
hold on;
plot(S,'c');
legend('R','I','S');
xlabel('Time/t') 
ylabel('Proportion') 
title('SIR Model')

% ����dr/dt=��i
function result = calculatedrdt(m,i)
    result =m*i;
end

% ����ds/dt=��si�ľ���ֵ
function result = calculatedsdt(n,s,i)
    result =n*s*i;
end