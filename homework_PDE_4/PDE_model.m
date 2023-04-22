% �����������
m = 100; % ����������
n = 100; % ����������
h = 1/(m-1); % �����С
x = 0:h:1;  % x ������
y = 0:h:1;  % y ������

% ��ʼ����ֵ��
u = zeros(m,n); % ��ʼֵΪ0

% ���ñ߽�����
u(1,:) = cosh(1/5*x);  % �±߽��¶�
u(m,:) = cosh(1/5*x);  % �ϱ߽��¶�
u(:,1) = cosh(1/5*y);  % ��߽��¶�
u(:,n) = cosh(1/5*y);  % �ұ߽��¶�

% ��˹-���¶���������ڲ������ϵ���ֵ��
u0 = u;
max_iter = 1000;
tol = 1e-6;
for iter = 1:max_iter
    for i = 2:m-1
        for j = 2:n-1
            u(i,j) = 1/4*(u(i-1,j)+u(i+1,j)+u(i,j-1)+u(i,j+1)-h^2/25*u(i,j));
        end
    end
    % ����������Ƿ�С�������
    err = max(max(abs(u-u0)./abs(u)));
    if err < tol
        break;
    end
    % ���µ������
    u0 = u;
end

% ���ӻ����
[X,Y] = meshgrid(x,y);
surf(X,Y,u');
title('��ֵ��');
xlabel('x');
ylabel('y');
zlabel('u');