% 定义网格参数
m = 100; % 横向网格数
n = 100; % 纵向网格数
h = 1/(m-1); % 网格大小
x = 0:h:1;  % x 轴坐标
y = 0:h:1;  % y 轴坐标

% 初始化数值解
u = zeros(m,n); % 初始值为0

% 设置边界条件
u(1,:) = cosh(1/5*x);  % 下边界温度
u(m,:) = cosh(1/5*x);  % 上边界温度
u(:,1) = cosh(1/5*y);  % 左边界温度
u(:,n) = cosh(1/5*y);  % 右边界温度

% 高斯-赛德尔迭代求解内部网格上的数值解
u0 = u;
max_iter = 1000;
tol = 1e-6;
for iter = 1:max_iter
    for i = 2:m-1
        for j = 2:n-1
            u(i,j) = 1/4*(u(i-1,j)+u(i+1,j)+u(i,j-1)+u(i,j+1)-h^2/25*u(i,j));
        end
    end
    % 检查相对误差是否小于容许度
    err = max(max(abs(u-u0)./abs(u)));
    if err < tol
        break;
    end
    % 更新迭代起点
    u0 = u;
end

% 可视化结果
[X,Y] = meshgrid(x,y);
surf(X,Y,u');
title('数值解');
xlabel('x');
ylabel('y');
zlabel('u');