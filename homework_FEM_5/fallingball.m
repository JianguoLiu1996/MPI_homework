%% Create a figure and set position and background color
fig = figure; fig.Position = [1 41 1280 907]; fig.Color = 'w';
%% Generate the meshgrid that will be use to represent water wave
[Xw, Yw] = meshgrid(-15:0.2:15); Hw = 1; Zw = Hw + zeros(size(Xw));
water = surf(Xw,Yw,Zw); hold on; water.FaceColor = 'none';  
water.EdgeColor = 'w'; water.EdgeAlpha = 0.15;
%% Set the bound, relative dimensions and lighting. 
axis([-15,15,-15,15,0,32]); daspect([1,1,1]); camlight; lighting gouraud;
%% Generate a ball using the sphere and extract default colors from matlab
[Xb,Yb,Zb] = sphere(20); clrs = get(gca,'colororder');
%% Set number of balls and generate random positions
Nb = 6; r = rand(3,Nb);  xballs = -14+r(1,:)*28; yballs = -14+r(2,:)*28; 
zballs = 15+r(3,:)*15; 
%% Generate balls and add handles to collection, radial positions on the meshgrid
balls = {}; D = {};
for n = 1:Nb
    ball = surf(Xb + xballs(n), Yb + yballs(n), Zb + Hw + zballs(n)); 
    ball.FaceColor = clrs(n,:); ball.EdgeAlpha = 0.1; balls = [balls, ball];
    d = sqrt((Xw - xballs(n)).^2 + (Yw - yballs(n)).^2); D = [D,d];
end
%% Set gravity, initial and final velocities for each ball
g = 2; Vb = zeros(1,Nb);  Ub = zeros(1,Nb);
%% Set wave characteristics and wave equation
wavevel = 5; decayrate = 0.1; amplitude = 0.5; wavenum = 4;
wave_eq = @(x,t) amplitude*exp(-decayrate*(t+x)).*sin((wavenum*x-wavevel*t));
%% Set logic to indicate ball is dropping, and set timer for wave
balldropping = zballs > Hw;  tic; twave = zeros(1,Nb);
%% Begin simulation
while(1)
    dt = toc; % Get change in time
    dv = -g*dt; % Get change in velocity
    water.ZData = Zw; % Set water to initial state
    for n = 1:Nb
        if(balldropping(n)) % If ball is droping
            % Update velocity and ball position
            Vb(n) = Ub(n) + dv; zballs(n) = zballs(n) + 0.5*(Vb(n)+Ub(n))*dt;
            % Update graphic handle of the ball
            balls(n).ZData = balls(n).ZData + 0.5*(Vb(n)+Ub(n))*dt;
        else % Ball is already on water
            % Accumuate wave time and extract radial positions from the ball
            twave(n) = twave(n) + dt; d = D{n};
            % Accumulate wave positions to reflect interference 
            water.ZData = water.ZData + wave_eq(d, twave(n));
            % Update ball position with wave_eq(0, t) to make it float
            balls(n).ZData = Zb + Hw + wave_eq(0, twave(n));
        end
    end
    %Update ball is dropping logic and initial velocity, reset timer and render objects
    balldropping = zballs > Hw; Ub = Vb; tic; drawnow
end