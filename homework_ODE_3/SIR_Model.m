% 设置SIT_Model初始值
S=[1.0];
I=[1.27*10^(-6)];
R=[0.0];
t=200;
m=1/2;%μ值
n=1/3;%λ值
% m=1/3;%μ值
% n=1/2;%λ值

% 计算R、S、I值
for i=2:t
    R(i) = R(i-1)+calculatedrdt(m,I(i-1));
    S(i) = S(i-1)-calculatedsdt(n,S(i-1),I(i-1));
    I(i) = I(i-1)+calculatedsdt(n,S(i-1),I(i-1))-calculatedrdt(m,I(i-1));
end

% 绘制R、S、I变化曲线
plot(R,'r');
hold on;
plot(I,'g');
hold on;
plot(S,'c');
legend('R','I','S');
xlabel('Time/t') 
ylabel('Proportion') 
title('SIR Model')

% 计算dr/dt=μi
function result = calculatedrdt(m,i)
    result =m*i;
end

% 计算ds/dt=λsi的绝对值
function result = calculatedsdt(n,s,i)
    result =n*s*i;
end