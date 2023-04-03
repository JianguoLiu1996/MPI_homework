a=-0.01;
b=0.01;
n=10000;
h=(b-a)/n;

x=a:h:b;
sx = sin(x);
cs = cos(x);
y1= (1-cs.*cs)./(x.*x);
y2=sx.*sx./(x.*x);
plot(x,y1,'r-o', x,y2, 'b-')