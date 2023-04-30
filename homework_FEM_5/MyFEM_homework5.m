clear;clc;close all;
% 设置网格大小
nx=100;
ny=100;

% 调用函数得到数值解结果
NumericalSolution=fem2d_poisson_rectangle_linear ( nx, ny );

% 对数值解结果进行格式修改，用于绘图和求误差
k = 0;
for j = 1 : ny
    for i = 1 : nx
      k = k + 1;
      NumericalSolutionZ(j,i)=NumericalSolution(k);
    end
end

% 为绘制数值解，定义网格
x = linspace(0,1,nx);
y = linspace(0,1,ny);
[X,Y] = meshgrid(x,y);

% 绘制数值解图
figure;
surf(X,Y,NumericalSolutionZ);
xlabel('x');
ylabel('y');
zlabel('u(x,y)');
title('NumericalSolutionZ');

% 定义精确解函数
u = @(x,y) sin(2*pi*x) .* sin(2*pi*y) + x.^2;

% 在网格上面计算精确解结果
ExactSolutionZ = u(X,Y);

% 绘制精确解结果图
figure;
surf(X,Y,ExactSolutionZ);
xlabel('x');
ylabel('y');
zlabel('u(x,y)');
title('ExactSolutionZ');

% 计算误差
Error=abs(ExactSolutionZ-NumericalSolutionZ);
% 绘制误差图
figure;
surf(X,Y,Error);
xlabel('x');
ylabel('y');
zlabel('error');
title('Error')