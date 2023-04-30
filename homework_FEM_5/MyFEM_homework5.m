clear;clc;close all;
% ���������С
nx=100;
ny=100;

% ���ú����õ���ֵ����
NumericalSolution=fem2d_poisson_rectangle_linear ( nx, ny );

% ����ֵ�������и�ʽ�޸ģ����ڻ�ͼ�������
k = 0;
for j = 1 : ny
    for i = 1 : nx
      k = k + 1;
      NumericalSolutionZ(j,i)=NumericalSolution(k);
    end
end

% Ϊ������ֵ�⣬��������
x = linspace(0,1,nx);
y = linspace(0,1,ny);
[X,Y] = meshgrid(x,y);

% ������ֵ��ͼ
figure;
surf(X,Y,NumericalSolutionZ);
xlabel('x');
ylabel('y');
zlabel('u(x,y)');
title('NumericalSolutionZ');

% ���徫ȷ�⺯��
u = @(x,y) sin(2*pi*x) .* sin(2*pi*y) + x.^2;

% ������������㾫ȷ����
ExactSolutionZ = u(X,Y);

% ���ƾ�ȷ����ͼ
figure;
surf(X,Y,ExactSolutionZ);
xlabel('x');
ylabel('y');
zlabel('u(x,y)');
title('ExactSolutionZ');

% �������
Error=abs(ExactSolutionZ-NumericalSolutionZ);
% �������ͼ
figure;
surf(X,Y,Error);
xlabel('x');
ylabel('y');
zlabel('error');
title('Error')