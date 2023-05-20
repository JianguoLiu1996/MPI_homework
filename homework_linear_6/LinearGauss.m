clc
clear all
while 1
%     A=input("������ϵ�����󣨷���A����");
%     b=input("�����볣��������������b����");
A=[10,-7,0; -3,2,6; 5,-1,5];
b=[7,4,6]';
    [H,L,MARK]=isLegal(A,b);
    if(MARK==1)
        [Ax,bx]=GSelimination(A,b,H);
        [x]=GSback(Ax,bx,H);
        disp("Ax=b�Ľ�����x��")
        disp(x);
        break;
    else
        disp("���벻�Ϸ������������룺");
    end
end


function [H,L,MARK]=isLegal(A,b)
    %�ж������Ƿ�Ϸ�
    %�Ϸ�MARK����1�����򷵻�0
    [HA,LA]=size(A);   %����A������к��У��ֱ���HA��LA��ʾ
    [Hb,Lb]=size(b);    %����b�������У�������˵Ӧ��Ϊ1�����У���Hb��Lb��ʾ
    if(HA==LA)
        if(HA==Hb)
            if(Lb==1)
                if(det(A)~=0)
                    MARK=1;H=HA;L=LA;
                else
                    MARK=0;H=0;L=0;
                    disp("�÷������޽⣡");
                    return;
                end
            else
                MARK=0;H=0;L=0;
                disp("��������b���벻�Ϸ���");
                return;
            end
        else
            MARK=0;H=0;L=0;
            disp("ϵ������A�ͳ�������b��������ȣ�");
            return;
        end
    else
        MARK=0;H=0;L=0;
        disp("ϵ������A���Ƿ���");
        return;
    end
end
  
function [Ax,bx]=GSelimination(A,b,n)
    %��ȥ����
    %n��ʾϵ������(����)�Ĺ�ģ
    %A��ʾϵ������
    %b��ʾ��������
    for k=1:(n-1)    %Ҫ����n-1����ȥ����
        if(A(k,k)~=0)   %��Ԫ�ز���Ϊ0,
            for i=(k+1):n  %ѭ����
                c=(-1*A(i,k))/A(k,k); %��������
                for j=1:n  %����ѭ��
                    A(i,j)=A(i,j)+c*A(k,j); %���¾���Ԫ��
                end
                b(i)=b(i)+c*b(k);   %���³�������
            end
        else
            disp("��Ԫ�س����㣬�������");
            return;
        end
    end
    Ax=A;bx=b;
end

function [x]=GSback(Ax,bx,n)
    %�ش�����
    %AxΪ������ȥ���̺�õ��������Ǿ���
    %bxΪ������ȥ���̺�õ��ĳ�������
    %n��ʾϵ������Ĺ�ģ
    for i=n:-1:1
        if(Ax(i,i)~=0)
            x(i)=bx(i);
            for j=n:-1:i+1
                x(i)=x(i)-Ax(i,j)*x(j);
            end
            x(i)=x(i)/Ax(i,i);
        else
            disp("��Ԫ�س����㣬�������");
            return;
        end
    end  
    x=x';
end   