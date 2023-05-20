clc
clear all
while 1
%     A=input("请输入系数矩阵（方阵A）：");
%     b=input("请输入常数向量（列向量b）：");
A=[10,-7,0; -3,2,6; 5,-1,5];
b=[7,4,6]';
    [H,L,MARK]=isLegal(A,b);
    if(MARK==1)
        [Ax,bx]=GSelimination(A,b,H);
        [x]=GSback(Ax,bx,H);
        disp("Ax=b的解向量x：")
        disp(x);
        break;
    else
        disp("输入不合法，请重新输入：");
    end
end


function [H,L,MARK]=isLegal(A,b)
    %判断输入是否合法
    %合法MARK返回1，否则返回0
    [HA,LA]=size(A);   %返回A矩阵的行和列，分别用HA和LA表示
    [Hb,Lb]=size(b);    %返回b向量的行（正常来说应该为1）和列，用Hb和Lb表示
    if(HA==LA)
        if(HA==Hb)
            if(Lb==1)
                if(det(A)~=0)
                    MARK=1;H=HA;L=LA;
                else
                    MARK=0;H=0;L=0;
                    disp("该方程组无解！");
                    return;
                end
            else
                MARK=0;H=0;L=0;
                disp("常数向量b输入不合法！");
                return;
            end
        else
            MARK=0;H=0;L=0;
            disp("系数矩阵A和常数向量b行数不相等！");
            return;
        end
    else
        MARK=0;H=0;L=0;
        disp("系数矩阵A不是方阵！");
        return;
    end
end
  
function [Ax,bx]=GSelimination(A,b,n)
    %消去过程
    %n表示系数矩阵(方阵)的规模
    %A表示系数矩阵
    %b表示常数矩阵
    for k=1:(n-1)    %要进行n-1次消去过程
        if(A(k,k)~=0)   %主元素不能为0,
            for i=(k+1):n  %循环行
                c=(-1*A(i,k))/A(k,k); %倍乘因子
                for j=1:n  %单行循环
                    A(i,j)=A(i,j)+c*A(k,j); %更新矩阵元素
                end
                b(i)=b(i)+c*b(k);   %更新常数向量
            end
        else
            disp("主元素出现零，程序错误！");
            return;
        end
    end
    Ax=A;bx=b;
end

function [x]=GSback(Ax,bx,n)
    %回代过程
    %Ax为经过消去过程后得到的上三角矩阵
    %bx为经过消去过程后得到的常数向量
    %n表示系数矩阵的规模
    for i=n:-1:1
        if(Ax(i,i)~=0)
            x(i)=bx(i);
            for j=n:-1:i+1
                x(i)=x(i)-Ax(i,j)*x(j);
            end
            x(i)=x(i)/Ax(i,i);
        else
            disp("主元素出现零，程序错误！");
            return;
        end
    end  
    x=x';
end   